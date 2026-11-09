/* BLAZE - Team DARK
   Probe intelligence (Phase 9): hidden-SSID unmasker + PNL + victim ID +
   probe forensics timeline. See BlazeIntel.h for the safety contract.

   RX safety: sniff() only runs while the hidden-unmasker listen window is
   open. It reads fixed arrays with bounds checks and copies into char
   buffers - no String, no flash IO, no printing, no allocation. */

#include "BlazeIntel.h"

#include "Accesspoints.h"
#include "Attack.h"
#include "Scan.h"
#include "settings.h"
#include "wifi.h"
#include "BlazeRecon.h"
#include "BlazeEvilTwin.h"
#include "BlazeDetector.h"

extern Attack attack;
extern Scan   scan;

namespace BlazeIntel {
    #define INTEL_HID_MAX 8
    #define INTEL_AGG_MAX 8
    #define INTEL_TL_MAX  32

    // ===== hidden-SSID unmasker =====
    struct HiddenAp {
        uint8_t mac[6];
        char    ssid[33]; // exact or candidate (never both)
        uint8_t ch;
        int8_t  rssi;
        uint8_t conf;     // 0 none, 1 candidate, 2 exact, 3 ambiguous
        uint8_t src;      // 0 none, 1 assoc, 2 reassoc, 3 probe-resp, 4 probe-req
    };

    static HiddenAp hid[INTEL_HID_MAX];
    static uint8_t  hidN       = 0;
    static uint8_t  hidPhase   = HID_OFF;
    static uint32_t hidStart   = 0;
    static uint32_t hidKickAt  = 0;
    static uint8_t  hidKickIdx = 0;

    // ===== PNL / victim aggregate (shared: one list is on screen at a time) =====
    struct Agg {
        uint16_t sec; // millis()/1000 when last seen (wrap-safe 16-bit)
        char     ssid[33];
        uint8_t  mac[6];
        uint16_t probes;
        uint8_t  clients;
        int8_t   rssi;
        uint8_t  ch;
    };

    static Agg     agg[INTEL_AGG_MAX];
    static uint8_t aggN    = 0;
    static uint8_t aggKind = 0; // 0 none, 1 PNL, 2 victims

    // ===== timeline (indices into the probe ring, newest first) =====
    // The timeline is never on screen at the same time as PNL/victims, so its
    // 32-byte index array shares the aggregate storage (saves 32 B static RAM).
    // timelineBuild() invalidates aggKind; the PNL/victim getters rebuild.
    static uint8_t* const tlIdx = (uint8_t*)agg;
    static uint8_t tlN = 0;

    // ================= helpers ================= //

    static const char* confName(uint8_t c) {
        switch (c) {
            case 2:  return "exact";
            case 1:  return "candidate";
            case 3:  return "ambiguous";
            default: return "hidden";
        }
    }

    static const char* srcName(uint8_t s) {
        switch (s) {
            case 1:  return "assoc";
            case 2:  return "reassoc";
            case 3:  return "probe-resp";
            case 4:  return "probe-req";
            default: return "none";
        }
    }

    static char srcChar(uint8_t s) {
        switch (s) {
            case 1:  return 'a';
            case 2:  return 'r';
            case 3:  return 'p';
            case 4:  return 'q';
            default: return '-';
        }
    }

    // fit a left column + right column into len chars (SSID gets truncated)
    static String fitRight(String a, String b, int len) {
        int room = len - b.length();
        if (room < 0) {
            b    = b.substring(0, len);
            room = 0;
        }
        if ((int)a.length() > room) a = a.substring(0, room);
        while ((int)(a.length() + b.length()) < len) a += ' ';
        return a + b;
    }

    static void mac3(const uint8_t* mac, char out[7]) {
        sprintf(out, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    }

    static String ageStr(uint32_t t) {
        uint32_t s = (millis() - t) / 1000;
        if (s < 60) return String(s) + "s";
        if (s < 3600) return String(s / 60) + "m";
        return String(s / 3600) + "h";
    }

    static HiddenAp* findHid(const uint8_t* mac) {
        for (uint8_t i = 0; i < hidN; i++)
            if (memcmp(hid[i].mac, mac, 6) == 0) return &hid[i];
        return 0;
    }

    static void setExact(HiddenAp* h, const char* ssid, uint8_t src) {
        if (!h || !ssid || !ssid[0]) return;

        if (h->conf == 2) {
            if (strncmp(h->ssid, ssid, 33) != 0) h->conf = 3; // conflicting exact evidence
            return;
        }
        if ((h->conf == 3) && (h->src != 0)) return; // exact conflict already logged

        memcpy(h->ssid, ssid, 33);
        h->ssid[32] = '\0';
        h->conf     = 2;
        h->src      = src;
    }

    static void setCandidate(HiddenAp* h, const char* ssid) {
        if (!h || !ssid || !ssid[0]) return;
        if ((h->conf == 2) || (h->conf == 3)) return;

        if (h->conf == 1) {
            if (strncmp(h->ssid, ssid, 33) != 0) {
                h->conf = 3; // two different candidate names -> never a fact
                h->src  = 0;
            }
            return;
        }

        memcpy(h->ssid, ssid, 33);
        h->ssid[32] = '\0';
        h->conf     = 1;
        h->src      = 4;
    }

    // read the SSID element starting at `start` (RX context: no String)
    static bool readSsid(const uint8_t* buf, uint16_t len, uint16_t start, char out[33]) {
        uint16_t pos = start;

        while (pos + 1 < len) {
            uint8_t tag  = buf[pos];
            uint8_t elen = buf[pos + 1];
            if ((uint32_t)pos + 2 + elen > len) break;

            if (tag == 0x00) {
                if (elen == 0) return false;
                uint8_t n = (elen > 32) ? 32 : elen;
                for (uint8_t i = 0; i < n; i++) {
                    char c = (char)buf[pos + 2 + i];
                    out[i] = ((uint8_t)c < 0x20) ? '.' : c;
                }
                out[n] = '\0';
                return true;
            }
            pos = (uint16_t)(pos + 2 + elen);
        }
        return false;
    }

    static void handleAssoc(uint8_t* buf, uint16_t len, bool reassoc) {
        HiddenAp* h = findHid(&buf[28]); // addr3 = BSSID
        if (!h) return;

        char    ssid[33];
        uint8_t start = reassoc ? 46 : 40; // cap+listen (+current AP)
        if (readSsid(buf, len, start, ssid)) setExact(h, ssid, reassoc ? 2 : 1);
    }

    static void handleProbeResp(uint8_t* buf, uint16_t len) {
        HiddenAp* h = findHid(&buf[28]); // addr3 = BSSID
        if (!h) return;

        char ssid[33];
        if (readSsid(buf, len, 48, ssid)) setExact(h, ssid, 3);
    }

    static void handleProbeReq(uint8_t* buf, uint16_t len) {
        char ssid[33];
        if (!readSsid(buf, len, 36, ssid)) return;

        // A directed probe is only a candidate when exactly one hidden AP
        // lives on the channel we heard it on. Never presented as fact.
        uint8_t ch = wifi_channel;
        if ((ch < 1) || (ch > 14)) return;

        int only  = -1;
        int count = 0;
        for (uint8_t i = 0; i < hidN; i++) {
            if (hid[i].ch == ch) {
                count++;
                only = i;
            }
        }
        if ((count == 1) && (only >= 0)) setCandidate(&hid[only], ssid);
    }

    static bool anyUnmasked() {
        if (hidN == 0) return false;
        for (uint8_t i = 0; i < hidN; i++)
            if (hid[i].conf != 2) return false;
        return true;
    }

    static void buildHiddenTable() {
        hidN = 0;
        int n = accesspoints.count();

        for (int i = 0; (i < n) && (hidN < INTEL_HID_MAX); i++) {
            if (!accesspoints.getHidden(i)) continue;

            HiddenAp& h = hid[hidN];
            uint8_t* m  = accesspoints.getMac(i);
            if (!m) continue;

            memset(&h, 0, sizeof(HiddenAp));
            memcpy(h.mac, m, 6);
            h.ch   = accesspoints.getCh(i);
            if ((h.ch < 1) || (h.ch > 14)) h.ch = 1;
            h.rssi = (int8_t)constrain(accesspoints.getRSSI(i), -127, 0);
            hidN++;
        }
    }

    static void kickNext() {
        if (hidN == 0) return;

        for (uint8_t tries = 0; tries < hidN; tries++) {
            uint8_t i = hidKickIdx % hidN;
            hidKickIdx = (uint8_t)((i + 1) % hidN);
            if (hid[i].conf == 2) continue; // already solved

            attack.deauthDevice(hid[i].mac, broadcast,
                                settings::getAttackSettings().deauth_reason, hid[i].ch);
            return;
        }
    }

    static void sortAgg(bool pnl) {
        for (uint8_t i = 0; i < aggN; i++) {
            for (uint8_t j = (uint8_t)(i + 1); j < aggN; j++) {
                bool swap;
                if (pnl) {
                    if (agg[j].clients != agg[i].clients) swap = agg[j].clients > agg[i].clients;
                    else if (agg[j].probes != agg[i].probes) swap = agg[j].probes > agg[i].probes;
                    else swap = agg[j].rssi > agg[i].rssi;
                } else {
                    swap = (int16_t)(agg[j].sec - agg[i].sec) > 0;
                }

                if (swap) {
                    Agg tmp = agg[i];
                    agg[i]  = agg[j];
                    agg[j]  = tmp;
                }
            }
        }
    }

    // ================= hidden state machine ================= //

    void setup() {
        memset(hid, 0, sizeof(hid));
        hidN       = 0;
        hidPhase   = HID_OFF;
        hidStart   = 0;
        hidKickAt  = 0;
        hidKickIdx = 0;

        memset(agg, 0, sizeof(agg));
        aggN    = 0;
        aggKind = 0;

        memset(tlIdx, 0, INTEL_TL_MAX);
        tlN = 0;
    }

    bool startHidden() {
        if ((hidPhase != HID_OFF) && (hidPhase != HID_DONE)) return false;
        if (attack.isRunning() || scan.isScanning() || scan.isSniffing() ||
            BlazeEvilTwin::active() || BlazeDetector::running() || BlazeDetector::confirming()) return false;

        memset(hid, 0, sizeof(hid));
        hidN       = 0;
        hidKickIdx = 0;
        hidKickAt  = 0;
        hidPhase   = HID_SCAN;
        hidStart   = millis();

        scan.start(SCAN_MODE_APS, 15000, SCAN_MODE_OFF, 0, true, wifi_channel);
        prntln(String(F("[BLAZE] HIDDEN: AP scan (15s)")));
        return true;
    }

    void stopHidden() {
        if (hidPhase == HID_OFF) return;

        // scan.stop() also resumes our AP when the web UI is enabled. Call it
        // unconditionally while the run is active: during KICK the scan is
        // already closed but the radio is still in TX mode with the AP down.
        bool wasRunning = hiddenRunning();
        if (wasRunning || scan.isScanning() || scan.isSniffing()) scan.stop();
        if (wasRunning) prntln(String(F("[BLAZE] HIDDEN: stopped")));
        hidPhase = HID_OFF;
    }

    void update() {
        if (hidPhase == HID_OFF || hidPhase == HID_DONE) return;

        uint32_t now = millis();

        switch (hidPhase) {
            case HID_SCAN: {
                // Scan::update() closes the AP scan; time out defensively.
                if ((now - hidStart < 1000) && scan.isScanning()) return;
                if (scan.isScanning() && (now - hidStart < 20000)) return;

                buildHiddenTable();
                if (hidN == 0) {
                    if (scan.isScanning() || scan.isSniffing()) scan.stop();
                    hidPhase = HID_DONE;
                    prntln(String(F("[BLAZE] HIDDEN: no hidden APs")));
                    return;
                }

                scan.start(SCAN_MODE_SNIFFER, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
                hidPhase = HID_LISTEN;
                hidStart = now;
                prnt("[BLAZE] HIDDEN: listening for ");
                prnt(hidN);
                prntln(String(F(" hidden AP(s) - assoc/probe evidence")));
                break;
            }

            case HID_LISTEN: {
                if (anyUnmasked()) {
                    if (scan.isScanning() || scan.isSniffing()) scan.stop();
                    hidPhase = HID_DONE;
                    prntln(String(F("[BLAZE] HIDDEN: all unmasked")));
                } else if (now - hidStart >= 12000) {
                    if (scan.isScanning() || scan.isSniffing()) scan.stop();
                    wifi::stopAP(); // radio free for directed deauth TX
                    hidPhase  = HID_KICK;
                    hidStart  = now;
                    hidKickAt = 0;
                    prntln(String(F("[BLAZE] HIDDEN: directed kick (4s)")));
                }
                break;
            }

            case HID_KICK: {
                if (now - hidStart >= 4000) {
                    scan.start(SCAN_MODE_SNIFFER, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
                    hidPhase = HID_RELISTEN;
                    hidStart = now;
                    prntln(String(F("[BLAZE] HIDDEN: re-listen (12s)")));
                } else if (now - hidKickAt >= 180) {
                    hidKickAt = now;
                    kickNext();
                }
                break;
            }

            case HID_RELISTEN: {
                if (anyUnmasked() || (now - hidStart >= 12000)) {
                    if (scan.isScanning() || scan.isSniffing()) scan.stop();
                    hidPhase = HID_DONE;
                    prnt("[BLAZE] HIDDEN: done - ");
                    prnt(hiddenUnmasked());
                    prnt("/");
                    prnt(hidN);
                    prntln(String(F(" revealed")));
                }
                break;
            }

            default:
                hidPhase = HID_DONE;
                break;
        }
    }

    void sniff(uint8_t* buf, uint16_t len) {
        if ((hidPhase != HID_LISTEN) && (hidPhase != HID_RELISTEN)) return;
        if (len < 36) return;
        if ((buf[12] & 0x0C) != 0x00) return; // management frames only

        switch (buf[12] & 0xF0) {
            case 0x00: // association request
                handleAssoc(buf, len, false);
                break;
            case 0x20: // reassociation request
                handleAssoc(buf, len, true);
                break;
            case 0x40: // probe request
                handleProbeReq(buf, len);
                break;
            case 0x50: // probe response
                handleProbeResp(buf, len);
                break;
            default:
                break;
        }
    }

    bool hiddenRunning() {
        return (hidPhase >= HID_SCAN) && (hidPhase <= HID_RELISTEN);
    }

    uint8_t hiddenPhase() { return hidPhase; }

    const char* hiddenPhaseName() {
        switch (hidPhase) {
            case HID_SCAN:     return "scan";
            case HID_LISTEN:   return "listen";
            case HID_KICK:     return "kick";
            case HID_RELISTEN: return "listen2";
            case HID_DONE:     return "done";
            default:           return "off";
        }
    }

    int hiddenCount() { return hidN; }

    int hiddenUnmasked() {
        int n = 0;
        for (uint8_t i = 0; i < hidN; i++)
            if (hid[i].conf == 2) n++;
        return n;
    }

    bool hiddenAllDone() { return anyUnmasked(); }

    String hiddenLine(int i) {
        if ((i < 0) || (i >= hidN)) return String();

        switch (hid[i].conf) {
            case 2:  return String('=') + String(hid[i].ssid);
            case 1:  return String('~') + String(hid[i].ssid);
            case 3:  return String(F("?<ambig>"));
            default: return String(F(" <hidden>"));
        }
    }

    String hiddenDetail(int i) {
        if ((i < 0) || (i >= hidN)) return String();

        char line[24];
        snprintf(line, sizeof(line), "ch%u %d %02X%02X%02X %c",
                 (unsigned)hid[i].ch, (int)hid[i].rssi,
                 hid[i].mac[3], hid[i].mac[4], hid[i].mac[5], srcChar(hid[i].src));
        return String(line);
    }

    // ================= PNL / victims ================= //

    void pnlBuild() {
        aggN    = 0;
        aggKind = 1;

        int n = BlazeRecon::probeCount();
        BlazeRecon::ProbeInfo p;
        uint16_t nowSec = (uint16_t)(millis() / 1000);

        for (int i = 0; i < n; i++) {
            if (!BlazeRecon::probeGet(i, p)) continue;
            if (p.ssid[0] == '\0') continue; // wildcard probe: no network name

            int slot = -1;
            for (uint8_t k = 0; k < aggN; k++) {
                if (strncmp(agg[k].ssid, p.ssid, 33) == 0) {
                    slot = k;
                    break;
                }
            }

            if (slot < 0) {
                if (aggN >= INTEL_AGG_MAX) continue;
                slot = aggN++;
                memset(&agg[slot], 0, sizeof(Agg));
                memcpy(agg[slot].ssid, p.ssid, 33);
                agg[slot].rssi = -127;
                agg[slot].sec  = nowSec;
                agg[slot].ch   = p.ch;
            }

            Agg& a = agg[slot];
            if (a.clients < 255) a.clients++;
            uint32_t sum = (uint32_t)a.probes + (p.hits ? p.hits : 1);
            a.probes = (sum > 65535) ? 65535 : (uint16_t)sum;
            if (p.rssi > a.rssi) a.rssi = p.rssi;
            uint16_t psec = (uint16_t)(p.t / 1000);
            if ((int16_t)(psec - a.sec) > 0) {
                a.sec = psec;
                a.ch  = p.ch;
            }
        }

        sortAgg(true);
    }

    void victimBuild() {
        aggN    = 0;
        aggKind = 2;

        int n = BlazeRecon::probeCount();
        BlazeRecon::ProbeInfo p;
        uint16_t nowSec = (uint16_t)(millis() / 1000);

        for (int i = 0; i < n; i++) {
            if (!BlazeRecon::probeGet(i, p)) continue;

            int slot = -1;
            for (uint8_t k = 0; k < aggN; k++) {
                if (memcmp(agg[k].mac, p.mac, 6) == 0) {
                    slot = k;
                    break;
                }
            }

            if (slot < 0) {
                if (aggN >= INTEL_AGG_MAX) continue;
                slot = aggN++;
                memset(&agg[slot], 0, sizeof(Agg));
                memcpy(agg[slot].mac, p.mac, 6);
                agg[slot].rssi = -127;
                agg[slot].sec  = nowSec;
                agg[slot].ch   = p.ch;
                memcpy(agg[slot].ssid, p.ssid, 33);
            }

            Agg& a = agg[slot];
            if (a.clients < 255) a.clients++;
            uint32_t sum = (uint32_t)a.probes + (p.hits ? p.hits : 1);
            a.probes = (sum > 65535) ? 65535 : (uint16_t)sum;
            if (p.rssi > a.rssi) a.rssi = p.rssi;
            uint16_t psec = (uint16_t)(p.t / 1000);
            if ((int16_t)(psec - a.sec) > 0) {
                a.sec = psec;
                a.ch  = p.ch;
                memcpy(a.ssid, p.ssid, 33); // last network this client looked for
            }
        }

        sortAgg(false);
    }

    static void ensurePnl() {
        if (aggKind != 1) pnlBuild();
    }

    static void ensureVictim() {
        if (aggKind != 2) victimBuild();
    }

    int pnlCount() { ensurePnl(); return aggN; }

    String pnlLine(int i) {
        ensurePnl();
        if ((i < 0) || (i >= aggN)) return String();
        return fitRight(String(agg[i].ssid), String(agg[i].clients) + "u", 17);
    }

    String pnlSsid(int i) {
        ensurePnl();
        if ((i < 0) || (i >= aggN)) return String();
        return String(agg[i].ssid);
    }

    uint8_t pnlClients(int i) {
        ensurePnl();
        if ((i < 0) || (i >= aggN)) return 0;
        return agg[i].clients;
    }

    uint16_t pnlProbeCount(int i) {
        ensurePnl();
        if ((i < 0) || (i >= aggN)) return 0;
        return agg[i].probes;
    }

    int8_t pnlRssi(int i) {
        ensurePnl();
        if ((i < 0) || (i >= aggN)) return 0;
        return agg[i].rssi;
    }

    uint8_t pnlCh(int i) {
        ensurePnl();
        if ((i < 0) || (i >= aggN)) return 0;
        return agg[i].ch;
    }

    uint32_t pnlAgeSec(int i) {
        ensurePnl();
        if ((i < 0) || (i >= aggN)) return 0;
        return (uint16_t)(millis() / 1000 - agg[i].sec);
    }

    int victimCount() { ensureVictim(); return aggN; }

    String victimLine(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return String();

        char m[7];
        mac3(agg[i].mac, m);
        return fitRight(victimVendor(i), String(m), 17);
    }

    String victimVendor(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return String();

        if ((agg[i].mac[0] & 0x02) != 0) return String(F("rand")); // LAA = randomized MAC

        String v = searchVendor(agg[i].mac);
        if (v.length() == 0) v = String(F("unknown"));
        return v;
    }

    String victimMac(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return String();
        return macToStr(agg[i].mac);
    }

    String victimSsid(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return String();
        if (agg[i].ssid[0] == '\0') return String(F("<wild>"));
        return String(agg[i].ssid);
    }

    uint16_t victimProbeCount(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return 0;
        return agg[i].probes;
    }

    int8_t victimRssi(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return 0;
        return agg[i].rssi;
    }

    uint8_t victimCh(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return 0;
        return agg[i].ch;
    }

    uint32_t victimAgeSec(int i) {
        ensureVictim();
        if ((i < 0) || (i >= aggN)) return 0;
        return (uint16_t)(millis() / 1000 - agg[i].sec);
    }

    // ================= timeline ================= //

    void timelineBuild() {
        aggKind = 0; // the index array shares this storage; force a PNL/victim rebuild
        int n = BlazeRecon::probeCount();
        if (n > INTEL_TL_MAX) n = INTEL_TL_MAX;

        tlN = (uint8_t)n;
        for (uint8_t i = 0; i < tlN; i++) tlIdx[i] = i;

        // insertion sort, newest timestamp first (the ring keeps slot order,
        // but dedup updates timestamps in place, so slot order is not time order)
        BlazeRecon::ProbeInfo a, b;
        for (uint8_t i = 1; i < tlN; i++) {
            uint8_t key = tlIdx[i];
            BlazeRecon::probeGet(key, a);
            int j = (int)i - 1;
            while (j >= 0) {
                BlazeRecon::probeGet(tlIdx[j], b);
                if ((int32_t)(b.t - a.t) >= 0) break;
                tlIdx[j + 1] = tlIdx[j];
                j--;
            }
            tlIdx[j + 1] = key;
        }
    }

    int timelineCount() { return tlN; }

    String timelineLine(int i) {
        if ((i < 0) || (i >= tlN)) return String();

        BlazeRecon::ProbeInfo p;
        if (!BlazeRecon::probeGet(tlIdx[i], p)) return String();

        char m[7];
        mac3(p.mac, m);
        String ssid = p.ssid[0] ? String(p.ssid) : String(F("<wild>"));
        return ageStr(p.t) + " c" + String(p.ch) + " " + String(m) + " " + ssid;
    }

    String timelineSsid(int i) {
        if ((i < 0) || (i >= tlN)) return String();

        BlazeRecon::ProbeInfo p;
        if (!BlazeRecon::probeGet(tlIdx[i], p)) return String();
        return p.ssid[0] ? String(p.ssid) : String(F("<wild>"));
    }

    String timelineRight(int i) {
        if ((i < 0) || (i >= tlN)) return String();

        BlazeRecon::ProbeInfo p;
        if (!BlazeRecon::probeGet(tlIdx[i], p)) return String();
        return ageStr(p.t) + " c" + String(p.ch);
    }

    // ================= web ================= //

    String reportJson() {
        String j = F("{\"hidden\":{\"running\":");
        j += hiddenRunning() ? F("true") : F("false");
        j += F(",\"phase\":\"");
        j += hiddenPhaseName();
        j += F("\",\"count\":");
        j += hidN;
        j += F(",\"unmasked\":");
        j += hiddenUnmasked();
        j += F(",\"aps\":[");

        for (uint8_t i = 0; i < hidN; i++) {
            if (i) j += ',';
            j += F("{\"mac\":\"");
            j += macToStr(hid[i].mac);
            j += F("\",\"ch\":");
            j += hid[i].ch;
            j += F(",\"rssi\":");
            j += (int)hid[i].rssi;
            j += F(",\"ssid\":\"");
            j += escape(String(hid[i].ssid));
            j += F("\",\"confidence\":\"");
            j += confName(hid[i].conf);
            j += F("\",\"source\":\"");
            j += srcName(hid[i].src);
            j += F("\"}");
        }

        j += F("],\"pnl\":[");

        pnlBuild();
        for (uint8_t i = 0; i < aggN; i++) {
            if (i) j += ',';
            j += F("{\"ssid\":\"");
            j += escape(String(agg[i].ssid));
            j += F("\",\"clients\":");
            j += agg[i].clients;
            j += F(",\"probes\":");
            j += agg[i].probes;
            j += F(",\"rssi\":");
            j += (int)agg[i].rssi;
            j += F(",\"ch\":");
            j += agg[i].ch;
            j += F(",\"age\":");
            j += (uint16_t)(millis() / 1000 - agg[i].sec);
            j += '}';
        }

        j += F("],\"victims\":[");

        victimBuild();
        for (uint8_t i = 0; i < aggN; i++) {
            if (i) j += ',';
            j += F("{\"mac\":\"");
            j += macToStr(agg[i].mac);
            j += F("\",\"vendor\":\"");
            j += escape(victimVendor(i));
            j += F("\",\"ssid\":\"");
            j += escape(victimSsid(i));
            j += F("\",\"probes\":");
            j += agg[i].probes;
            j += F(",\"rssi\":");
            j += (int)agg[i].rssi;
            j += F(",\"ch\":");
            j += agg[i].ch;
            j += F(",\"age\":");
            j += (uint16_t)(millis() / 1000 - agg[i].sec);
            j += '}';
        }

        j += F("],\"timeline\":[");

        timelineBuild();
        for (uint8_t i = 0; i < tlN; i++) {
            if (i) j += ',';
            BlazeRecon::ProbeInfo p;
            if (!BlazeRecon::probeGet(tlIdx[i], p)) continue;
            j += F("{\"age\":");
            j += (millis() - p.t) / 1000;
            j += F(",\"ch\":");
            j += p.ch;
            j += F(",\"rssi\":");
            j += (int)p.rssi;
            j += F(",\"mac\":\"");
            j += macToStr(p.mac);
            j += F("\",\"ssid\":\"");
            j += escape(String(p.ssid));
            j += F("\"}");
        }

        j += F("]}");
        return j;
    }
}
