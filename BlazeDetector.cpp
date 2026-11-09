/* BLAZE - Team DARK
   DETECTOR (Phase 7): find other deauthers / fake beacons on the air.
   API, cycle semantics and the JSON schema are frozen in BlazeDetector.h.

   RX safety: sniff() runs inside the SDK RX callback (Scan::sniffer hook):
   RAM-only accounting, bounded loops, no allocation, no printing, no flash
   reads. All grading, JSON and String work happens on the main loop
   (update() / getters).

   Every verdict is measured:
     deauth   >= 40/10s                -> CONFIRMED (far beyond any legit AP)
              >= 10/10s + not in scan  -> CONFIRMED (spoofed source)
              >= 10/10s + known        -> LIKELY
              5..9 + not in scan       -> LIKELY (random MAC)
              5..9 + known             -> INFO
     beacons  >= 100/10s -> CONFIRMED, >= 30 -> LIKELY, >= 15 -> INFO
     SSIDs    >= 10 distinct on one MAC -> CONFIRMED, >= 5 -> LIKELY
     channel  5+ beacons claiming a different channel -> INFO only
              (channel-hop race makes it weak evidence)
     dup SSID on 2 BSSIDs: mixed encryption or LAA -> LIKELY, else INFO
     active proof: 2+ radios answering the directed probe with MIXED
              security -> CONFIRMED (a clone cannot match security) */

#include "BlazeDetector.h"

#include "Attack.h"
#include "Scan.h"
#include "BlazeLoyal.h"
#include "BlazeEvilTwin.h"

extern Attack attack;
extern Scan   scan;

namespace BlazeDetector {
    // ===== finding types - typeStr() must emit the schema strings =====
    enum : uint8_t {
        TYPE_DEAUTH_FLOOD = 0,
        TYPE_DEAUTH_SPOOF = 1,
        TYPE_BEACON_SPAM  = 2,
        TYPE_SSID_FLOOD   = 3,
        TYPE_DUP_SSID     = 4,
        TYPE_CH_MISMATCH  = 5
    };

    #define DETECT_MAX_SRCS     16
    #define DETECT_MAX_FINDINGS 8
    #define DETECT_MAX_RESP     4

    // ===== sniff accounting (RX context: fixed table, never grows) =====
    struct Src {
        uint8_t  mac[6];
        bool     used;
        uint16_t deauths;
        uint16_t beacons;
        uint32_t ssidHash[4];   // FNV-1a set: first 4 distinct SSID hashes
        uint8_t  distinctSsids; // distinct count (capped at 255)
        uint8_t  dsChannel;     // channel the beacons claim (DS IE)
        uint8_t  dsMismatch;    // beacons claiming != wifi_channel
        uint8_t  heardCh;       // radio channel of the last mismatch
    };

    static Src      srcs[DETECT_MAX_SRCS];
    static uint16_t totalDeauths;
    static uint16_t totalBeacons;
    static bool     windowActive; // gate: cycle sniff window open
    static uint32_t windowStartMs;

    // ===== active proof: distinct probe responders (RX context) =====
    static uint8_t respMac[DETECT_MAX_RESP][6];
    static uint8_t respPrivacy[DETECT_MAX_RESP];
    static uint8_t respCount;
    static bool    confirmSniffActive;
    static char    confirmSsid[33];
    static uint8_t confirmSsidLen;

    // ===== confirm state machine (main loop, millis-based, no delay()) =====
    enum : uint8_t { CONF_IDLE = 0, CONF_WAIT = 1, CONF_TX = 2 };

    static uint8_t  confState;
    static uint8_t  chPlan[2];
    static uint8_t  chIdx;
    static uint8_t  probeSrc[6];
    static int      confIdx;
    static uint32_t confStateStart;
    static uint32_t confLastTx;
    static uint8_t  confTxDone;

    // ===== cycle: 0 idle, 1 scanning, 2 sniffing, 3 analyzing (transient) =====
    static int cycPhase = 0;

    // ===== findings (fixed, max 8) =====
    struct Finding {
        uint8_t  level;   // 0 INFO, 1 LIKELY, 2 CONFIRMED
        uint8_t  type;    // TYPE_*
        uint8_t  ch1;
        uint8_t  ch2;
        uint8_t  enc1;
        uint8_t  enc2;
        uint8_t  bssid2[6];
        char     ssid[33];
        char     detail[64]; // measured evidence, snprintf-built
        uint16_t count;      // measured count behind the line (not in JSON)
        // level bits: 0..1 grade, bit 6 = duel evidence, bit 5 = rival operator
    };

    static const uint8_t FIND_DUEL  = 0x40;
    static const uint8_t FIND_RIVAL = 0x20;

    static Finding finds[DETECT_MAX_FINDINGS];
    static uint8_t findCount;
    static uint8_t viewFilter = FILTER_ALL;

    // ================= RX-context helpers (bounded, RAM only) ================= //

    // FNV-1a 32-bit over raw SSID bytes (never touches flash)
    static uint32_t fnv1a(const uint8_t* data, uint8_t n) {
        uint32_t h = 2166136261UL;
        for (uint8_t i = 0; i < n; i++) {
            h ^= data[i];
            h *= 16777619UL;
        }
        return h;
    }

    // find-or-create the accounting slot for addr2; NULL when the table is full
    static Src* findSrc(const uint8_t* mac) {
        int freeSlot = -1;

        for (uint8_t i = 0; i < DETECT_MAX_SRCS; i++) {
            if (srcs[i].used) {
                if (memcmp(srcs[i].mac, mac, 6) == 0) return &srcs[i];
            } else if (freeSlot < 0) {
                freeSlot = i;
            }
        }

        if (freeSlot < 0) return 0; // fixed table full -> drop (no growth)

        Src* s = &srcs[freeSlot];
        memset(s, 0, sizeof(Src));
        memcpy(s->mac, mac, 6);
        s->used = true;
        return s;
    }

    // beacon body: walk IEs from 48 (bounds-checked against len every step)
    static void walkBeaconIes(uint8_t* buf, uint16_t len, Src* s) {
        uint16_t pos = 48; // hdr24 + timestamp 8 + interval 2 + capability 2

        while (pos + 1 < len) {
            uint8_t tag  = buf[pos];
            uint8_t elen = buf[pos + 1];
            if ((uint16_t)(pos + 2 + elen) > len) break;

            if ((tag == 0x00) && (elen <= 32)) { // SSID -> FNV-1a hash set
                uint32_t h    = fnv1a(&buf[pos + 2], elen);
                uint8_t  kept = (s->distinctSsids < 4) ? s->distinctSsids : 4;
                bool     seen = false;

                for (uint8_t k = 0; k < kept; k++) {
                    if (s->ssidHash[k] == h) { seen = true; break; }
                }
                if (!seen) {
                    if (s->distinctSsids < 4) s->ssidHash[s->distinctSsids] = h;
                    if (s->distinctSsids < 255) s->distinctSsids++;
                }
            } else if ((tag == 0x03) && (elen == 1)) { // DS parameter -> channel
                s->dsChannel = buf[pos + 2];
                if (s->dsChannel != wifi_channel) {
                    s->dsMismatch++;
                    s->heardCh = wifi_channel; // measured: where we heard it
                }
            }

            pos = (uint16_t)(pos + 2 + elen);
        }
    }

    // probe response for OUR proof SSID? (byte compare, no String in RX)
    static bool proofRespMatch(uint8_t* buf, uint16_t len) {
        uint16_t pos = 48;

        while (pos + 1 < len) {
            uint8_t tag  = buf[pos];
            uint8_t elen = buf[pos + 1];
            if ((uint16_t)(pos + 2 + elen) > len) break;

            if (tag == 0x00) { // first SSID IE decides
                return (elen == confirmSsidLen) && (memcmp(&buf[pos + 2], confirmSsid, elen) == 0);
            }
            pos = (uint16_t)(pos + 2 + elen);
        }
        return false;
    }

    // remember a distinct answering radio (max 4, never grows)
    static void recordResp(const uint8_t* mac, uint8_t capByte) {
        for (uint8_t i = 0; i < respCount; i++) {
            if (memcmp(respMac[i], mac, 6) == 0) return; // same radio as before
        }
        if (respCount >= DETECT_MAX_RESP) return; // table full -> ignore

        memcpy(respMac[respCount], mac, 6);
        respPrivacy[respCount] = (capByte & 0x10) ? 1 : 0; // capability privacy bit
        respCount++;
    }

    // ================= main-loop helpers ================= //

    // "14:cc:20:aa:bb:cc" (17 chars + NUL)
    static void macToStr(const uint8_t* mac, char out[18]) {
        snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // is this transmitter one of the APs we just scanned?
    static bool knownAp(const uint8_t* mac) {
        int n = accesspoints.count();
        for (int i = 0; i < n; i++) {
            uint8_t* m = accesspoints.getMac(i);
            if (m && memcmp(m, mac, 6) == 0) return true;
        }
        return false;
    }

    static String typeStr(uint8_t type) {
        switch (type) {
            case TYPE_DEAUTH_FLOOD: return String(F("DEAUTH_FLOOD"));
            case TYPE_DEAUTH_SPOOF: return String(F("DEAUTH_SPOOF"));
            case TYPE_BEACON_SPAM:  return String(F("BEACON_SPAM"));
            case TYPE_SSID_FLOOD:   return String(F("SSID_FLOOD"));
            case TYPE_DUP_SSID:     return String(F("DUP_SSID"));
            case TYPE_CH_MISMATCH:  return String(F("CH_MISMATCH"));
        }
        return String(F("UNKNOWN")); // unreachable: only TYPE_* get stored
    }

    static bool addFinding(uint8_t level, uint8_t type, uint8_t ch1, uint8_t ch2, uint8_t enc1, uint8_t enc2,
                           const uint8_t* bssid2, const char* ssid, const char* detail, uint16_t count) {
        if (findCount >= DETECT_MAX_FINDINGS) return false; // hard cap

        Finding& f = finds[findCount];
        memset(&f, 0, sizeof(Finding));

        f.level = level & 0x03; // grade only; flags live in the high bits
        f.type  = type;
        f.ch1   = ch1;
        f.ch2   = ch2;
        f.enc1  = enc1;
        f.enc2  = enc2;
        f.count = count;
        if (bssid2) memcpy(f.bssid2, bssid2, 6);
        if (ssid) {
            strncpy(f.ssid, ssid, 32);
            f.ssid[32] = '\0';
        }
        if (detail) {
            strncpy(f.detail, detail, 63);
            f.detail[63] = '\0';
        }

        findCount++;
        return true;
    }

    // already reported this SSID as a DUP? (one finding per duplicated SSID)
    static bool dupReported(const char* ssid) {
        for (uint8_t i = 0; i < findCount; i++) {
            if ((finds[i].type == TYPE_DUP_SSID) && (strcmp(finds[i].ssid, ssid) == 0)) return true;
        }
        return false;
    }

    // grade the window - MAIN LOOP only
    static void analyze() {
        // window totals are accounted per design; the grades below are per-source
        (void)totalDeauths;
        (void)totalBeacons;

        uint32_t secs = (millis() - windowStartMs) / 1000;
        if (secs < 1) secs = 1;
        if (secs > 60) secs = 60;

        // ===== 1) deauth/disassoc flooders =====
        for (uint8_t i = 0; i < DETECT_MAX_SRCS && findCount < DETECT_MAX_FINDINGS; i++) {
            const Src& s = srcs[i];
            if (!s.used || (s.deauths < 5)) continue; // <5 = noise, no finding

            bool known = knownAp(s.mac);
            bool laa   = (s.mac[0] & 0x02) != 0; // locally administered bit
            char m[18];
            macToStr(s.mac, m);

            uint8_t level;
            if (s.deauths >= 40) level = 2;
            else if (s.deauths >= 10) level = known ? 1 : 2;
            else level = known ? 0 : 1; // 5..9

            char d[64];
            if (!known && laa) {
                snprintf(d, sizeof(d), "%u deauths/%lus from %s (not in scan, LAA MAC)", (unsigned)s.deauths,
                         (unsigned long)secs, m);
            } else if (!known) {
                snprintf(d, sizeof(d), "%u deauths/%lus from %s (not in scan)", (unsigned)s.deauths,
                         (unsigned long)secs, m);
            } else if (laa) {
                snprintf(d, sizeof(d), "%u deauths/%lus from %s, LAA MAC", (unsigned)s.deauths, (unsigned long)secs,
                         m);
            } else {
                snprintf(d, sizeof(d), "%u deauths/%lus from %s", (unsigned)s.deauths, (unsigned long)secs, m);
            }

            addFinding(level, known ? TYPE_DEAUTH_FLOOD : TYPE_DEAUTH_SPOOF, 0, 0, 0, 0, 0, 0, d, s.deauths);
        }

        // ===== 2) beacon spam / SSID flood / wrong-channel beacons =====
        for (uint8_t i = 0; i < DETECT_MAX_SRCS && findCount < DETECT_MAX_FINDINGS; i++) {
            const Src& s = srcs[i];
            if (!s.used) continue;

            char m[18];
            macToStr(s.mac, m);
            char d[64];

            if (s.beacons >= 15) {
                uint8_t level = (s.beacons >= 100) ? 2 : ((s.beacons >= 30) ? 1 : 0);
                snprintf(d, sizeof(d), "%s %u beacons/%lus (spec ~10/s)", m, (unsigned)s.beacons, (unsigned long)secs);
                addFinding(level, TYPE_BEACON_SPAM, 0, 0, 0, 0, 0, 0, d, s.beacons);
            }

            if ((s.distinctSsids >= 5) && (findCount < DETECT_MAX_FINDINGS)) {
                uint8_t level = (s.distinctSsids >= 10) ? 2 : 1;
                snprintf(d, sizeof(d), "%s advertises %u SSIDs (fake beacon spam)", m, (unsigned)s.distinctSsids);
                addFinding(level, TYPE_SSID_FLOOD, 0, 0, 0, 0, 0, 0, d, s.distinctSsids);
            }

            if ((s.dsMismatch >= 5) && (findCount < DETECT_MAX_FINDINGS)) {
                snprintf(d, sizeof(d), "beacon says ch%u, heard on ch %u (x%u)", (unsigned)s.dsChannel,
                         (unsigned)s.heardCh, (unsigned)s.dsMismatch);
                addFinding(0, TYPE_CH_MISMATCH, s.dsChannel, s.heardCh, 0, 0, 0, 0, d, s.dsMismatch);
            }
        }

        // ===== 3) same SSID on two different BSSIDs =====
        int n = accesspoints.count();
        for (int i = 0; i < n && findCount < DETECT_MAX_FINDINGS; i++) {
            String a = accesspoints.getSSID(i);
            if (a.length() == 0) continue;

            for (int j = i + 1; j < n && findCount < DETECT_MAX_FINDINGS; j++) {
                String b = accesspoints.getSSID(j);
                if ((b.length() == 0) || !(a == b)) continue;

                uint8_t* m1 = accesspoints.getMac(i);
                uint8_t* m2 = accesspoints.getMac(j);
                if (!m1 || !m2 || (memcmp(m1, m2, 6) == 0)) continue; // same radio

                char ssid[33];
                a.toCharArray(ssid, 33);
                for (uint8_t k = 0; ssid[k]; k++) {
                    if ((uint8_t)ssid[k] < 0x20) ssid[k] = '.'; // keep JSON/display clean
                }
                if (dupReported(ssid)) continue; // one finding per duplicated SSID

                uint8_t ch1 = accesspoints.getCh(i);
                uint8_t ch2 = accesspoints.getCh(j);
                uint8_t e1  = accesspoints.getEnc(i);
                uint8_t e2  = accesspoints.getEnc(j);
                bool    laa = ((m1[0] | m2[0]) & 0x02) != 0;

                uint8_t level = ((e1 != e2) || laa) ? 1 : 0;

                char d1[18], d2[18];
                macToStr(m1, d1);
                macToStr(m2, d2);
                char d[64];
                snprintf(d, sizeof(d), "SSID on 2 BSSIDs ch%u/ch%u enc %u/%u (%s vs %s)", (unsigned)ch1,
                         (unsigned)ch2, (unsigned)e1, (unsigned)e2, d1, d2);

                addFinding(level, TYPE_DUP_SSID, ch1, ch2, e1, e2, m2, ssid, d, 2);
            }
        }
    }

    // ================= active proof (phase 4) ================= //

    static void abortConfirm() { // flags only: an open window self-times out at 1500 ms
        confirmSniffActive = false;
        confState          = CONF_IDLE;
    }

    // open a fixed-channel listen window on the planned channel
    static void enterWindow() {
        scan.setChannel(chPlan[chIdx]);
        scan.start(SCAN_MODE_SNIFFER, 1500, SCAN_MODE_OFF, 0, false, chPlan[chIdx]);
        confState      = CONF_WAIT;
        confStateStart = millis();
    }

    // grade the responders into the DUP finding (main loop)
    static void finalizeConfirm() {
        abortConfirm();
        if (scan.isSniffing()) scan.stop(); // never leave our window open

        if ((confIdx < 0) || (confIdx >= findCount)) return;

        Finding& f = finds[confIdx];

        if (respCount >= 2) {
            uint8_t flags = f.level & 0xE0; // keep rival/duel flags already measured
            if ((f.level & 0x03) < 1) f.level = flags | 1;

            bool mixed = false;
            for (uint8_t i = 1; i < respCount; i++) {
                if (respPrivacy[i] != respPrivacy[0]) { mixed = true; break; }
            }

            if (mixed) {
                f.level = flags | 2; // a clone cannot match the real security mode
                snprintf(f.detail, sizeof(f.detail), "PROOF: %u radios answer, mixed security (open+wpa2)",
                         (unsigned)respCount);
            } else {
                snprintf(f.detail, sizeof(f.detail), "%u radios answered probe (same security) - roaming or twin",
                         (unsigned)respCount);
            }
        } else if (respCount == 1) {
            snprintf(f.detail, sizeof(f.detail), "1 radio answered (of 2 seen in scan)");
        } else {
            snprintf(f.detail, sizeof(f.detail), "no answer (AP off-channel or jammed)");
        }
    }

    // millis-based state machine - each tick does a few comparisons, no delay()
    static void updateConfirm() {
        uint32_t now = millis();

        if (attack.isRunning() || BlazeEvilTwin::active()) { // radio taken over
            abortConfirm();
            return;
        }

        if (confState == CONF_WAIT) {
            if (scan.isSniffing()) {
                confState      = CONF_TX;
                confStateStart = now;
                confLastTx     = 0;
                confTxDone     = 0;
            } else if (now - confStateStart > 2000) {
                finalizeConfirm(); // window never opened -> stop cleanly
            }
            return;
        }

        if (confState == CONF_TX) {
            bool sniffing = scan.isSniffing();

            if (confTxDone < 3) {
                if (sniffing) {
                    if ((confTxDone == 0) || (now - confLastTx >= 300)) {
                        wifi_promiscuous_enable(0); // TX needs RX off (Attack.cpp:44)
                        attack.sendProbe(probeSrc, confirmSsid, chPlan[chIdx]);
                        wifi_promiscuous_enable(1); // RX back on; scanMode untouched
                        confLastTx = now;
                        confTxDone++;
                    }
                    return;
                }
                // window closed before all 3 probes went out -> finish below
            } else if (sniffing) {
                if (now - confStateStart > 4000) scan.stop(); // safety net (window ends at 1500 ms)
                return;
            }

            // listen window closed -> channel 2 or grade the result
            if ((chPlan[1] != 0) && (chIdx == 0)) {
                chIdx++;
                enterWindow();
            } else {
                finalizeConfirm();
            }
            return;
        }

        abortConfirm(); // unknown state: never leak the sniffer
    }

    // ================= public API ================= //

    void setup() {
        memset(srcs, 0, sizeof(srcs));
        totalDeauths  = 0;
        totalBeacons  = 0;
        windowActive  = false;
        windowStartMs = 0;

        memset(finds, 0, sizeof(finds));
        findCount = 0;

        respCount          = 0;
        confirmSniffActive = false;
        confState          = CONF_IDLE;
        confirmSsid[0]     = '\0';
        confirmSsidLen     = 0;
        confIdx            = -1;

        cycPhase = 0;
        viewFilter = FILTER_ALL;
    }

    void update() {
        if (confState != CONF_IDLE) {
            updateConfirm();
            return; // cycle and proof are mutually exclusive (guarded in start/confirm)
        }

        if (cycPhase == 1) {
            if (!scan.isScanning()) { // AP scan done -> reset accounting, open window
                memset(srcs, 0, sizeof(srcs));
                totalDeauths  = 0;
                totalBeacons  = 0;
                windowActive  = true;
                windowStartMs = millis();
                cycPhase      = 2;
                scan.start(SCAN_MODE_SNIFFER, 10000, SCAN_MODE_OFF, 0, true, wifi_channel);
            }
        } else if (cycPhase == 2) {
            if (!scan.isSniffing()) { // window closed -> grade it
                windowActive = false;
                cycPhase     = 3;
                analyze();
                cycPhase = 0;
            }
        }
    }

    void sniff(uint8_t* buf, uint16_t len) {
        if (!windowActive && !confirmSniffActive) return; // gate
        if (len < 28) return; // addr2 spans 22..27 (Scan::sniffer gates this too)

        uint8_t fc = buf[12] & 0xFC; // frame control, version bits masked off

        // ===== active proof: only probe responses for our proof SSID =====
        if (confirmSniffActive) {
            if (fc != 0x50) return; // probe response only
            if (len < 50) return;   // capability (46..47) + IEs from 48
            if (proofRespMatch(buf, len)) recordResp(&buf[22], buf[46]);
            return;
        }

        if (!windowActive) return;

        switch (fc) {
            case 0xC0: { // deauthentication
                Src* s = findSrc(&buf[22]);
                if (!s) break; // table full -> drop
                s->deauths++;
                totalDeauths++;
                break;
            }
            case 0xA0: { // disassociation
                Src* s = findSrc(&buf[22]);
                if (!s) break; // table full -> drop
                s->deauths++;
                totalDeauths++;
                break;
            }
            case 0x80: { // beacon
                Src* s = findSrc(&buf[22]);
                if (!s) break; // table full -> drop
                s->beacons++;
                totalBeacons++;
                walkBeaconIes(buf, len, s);
                break;
            }
            default:
                break; // probe req/resp, data, ... -> not evidence
        }
    }

    bool start(uint8_t filter) {
        if (confState != CONF_IDLE) return false; // never overlap the proof machine
        if (attack.isRunning() || scan.isScanning() || scan.isSniffing() || BlazeEvilTwin::active()) return false;

        viewFilter = (filter <= FILTER_RIVAL) ? filter : FILTER_ALL;
        findCount = 0;
        memset(srcs, 0, sizeof(srcs));
        totalDeauths = 0;
        totalBeacons = 0;
        windowActive = false;
        cycPhase     = 1;
        scan.start(SCAN_MODE_APS, 15000, SCAN_MODE_OFF, 0, true, wifi_channel);
        return true;
    }

    void stop() {
        abortConfirm();
        if (scan.isSniffing() || scan.isScanning()) scan.stop();
        windowActive = false;
        cycPhase     = 0;
    }

    bool running() { return cycPhase != 0; }

    int phase() { return confirming() ? 4 : cycPhase; }

    bool confirm(int idx) {
        if ((idx < 0) || (idx >= findCount)) return false;
        if (finds[idx].type != TYPE_DUP_SSID) return false;
        if (cycPhase != 0) return false;
        if (confState != CONF_IDLE) return false;
        if (attack.isRunning()) return false;
        if (scan.isScanning() || scan.isSniffing()) return false;
        if (BlazeEvilTwin::active()) return false; // its softAP would answer our probe
        if (!BlazeLoyal::allowSelect(String(finds[idx].ssid))) return false; // OwnNetGuard flashes itself
        if ((finds[idx].ch1 < 1) || (finds[idx].ch1 > 14)) return false;

        memcpy(confirmSsid, finds[idx].ssid, 33); // stored ssid is NUL-terminated
        confirmSsidLen = (uint8_t)strlen(confirmSsid);

        respCount          = 0;
        confirmSniffActive = true;
        confIdx            = idx;
        chPlan[0]          = finds[idx].ch1;
        chPlan[1]          = ((finds[idx].ch2 >= 1) && (finds[idx].ch2 <= 14) && (finds[idx].ch2 != finds[idx].ch1))
                                 ? finds[idx].ch2
                                 : 0;
        chIdx = 0;

        probeSrc[0] = 0x0E; // locally administered unicast probe source
        for (uint8_t i = 1; i < 6; i++) probeSrc[i] = (uint8_t)random(256);

        enterWindow();
        return true;
    }

    bool confirming() { return confState != CONF_IDLE; }

    void setFilter(uint8_t f) {
        viewFilter = (f <= FILTER_RIVAL) ? f : FILTER_ALL;
    }

    uint8_t filter() { return viewFilter; }

    const char* filterName() {
        switch (viewFilter) {
            case FILTER_DUEL:  return "DUEL";
            case FILTER_RIVAL: return "RIVAL";
            default:           return "ALL";
        }
    }

    static bool passesFilter(const Finding& f) {
        if (viewFilter == FILTER_DUEL) return (f.level & FIND_DUEL) != 0;
        if (viewFilter == FILTER_RIVAL) return (f.level & FIND_RIVAL) != 0;
        return true;
    }

    int filteredCount() {
        int n = 0;
        for (uint8_t i = 0; i < findCount; i++)
            if (passesFilter(finds[i])) n++;
        return n;
    }

    int filteredIndex(int i) {
        if (i < 0) return -1;
        int row = 0;
        for (uint8_t k = 0; k < findCount; k++) {
            if (!passesFilter(finds[k])) continue;
            if (row == i) return k;
            row++;
        }
        return -1;
    }

    int findingCount() { return findCount; }

    String findingLine(int i) {
        if ((i < 0) || (i >= findCount)) return String();

        const Finding& f = finds[i];
        uint8_t        lvl   = f.level & 0x03;
        char           badge = (lvl >= 2) ? '*' : ((lvl == 1) ? '!' : '-');
        char           line[18];

        switch (f.type) {
            case TYPE_DUP_SSID:
                snprintf(line, sizeof(line), "%cDUP %s", badge, f.ssid);
                break;
            case TYPE_DEAUTH_FLOOD:
            case TYPE_DEAUTH_SPOOF:
                snprintf(line, sizeof(line), "%cDEAUTH x%u", badge, (unsigned)f.count);
                break;
            case TYPE_BEACON_SPAM:
                snprintf(line, sizeof(line), "%cBEACON x%u", badge, (unsigned)f.count);
                break;
            case TYPE_SSID_FLOOD:
                snprintf(line, sizeof(line), "%cSSID x%u", badge, (unsigned)f.count);
                break;
            default: // TYPE_CH_MISMATCH
                snprintf(line, sizeof(line), "%cCHMIS", badge);
                break;
        }
        return String(line);
    }

    String findingDetail(int i) {
        if ((i < 0) || (i >= findCount)) return String();
        return String(finds[i].detail);
    }

    uint8_t findingLevel(int i) {
        if ((i < 0) || (i >= findCount)) return 0;
        return finds[i].level & 0x03;
    }

    void clear() {
        abortConfirm();
        findCount = 0;
    }

    String reportJson() {
        String j = F("{\"running\":");
        if (running()) j += String(F("true"));
        else j += String(F("false"));
        j += String(F(",\"phase\":"));
        j += phase();
        j += String(F(",\"count\":"));
        j += (int)findCount;
        j += String(F(",\"filter\":\""));
        j += filterName();
        j += String(F("\",\"findings\":["));

        for (uint8_t i = 0; i < findCount; i++) {
            const Finding& f = finds[i];
            if (i) j += String(F(","));
            j += String(F("{\"level\":"));
            j += (int)(f.level & 0x03);
            j += String(F(",\"type\":\""));
            j += typeStr(f.type);
            j += String(F("\",\"ssid\":\""));
            j += escape(String(f.ssid));
            j += String(F("\",\"detail\":\""));
            j += escape(String(f.detail));
            j += String(F("\",\"confirm\":"));
            if (f.type == TYPE_DUP_SSID) j += String(F("true"));
            else j += String(F("false"));
            j += String(F(",\"duel\":"));
            j += (f.level & FIND_DUEL) ? String(F("true")) : String(F("false"));
            j += String(F(",\"rival\":"));
            j += (f.level & FIND_RIVAL) ? String(F("true")) : String(F("false"));
            j += String(F("}"));
        }

        j += String(F("]}"));
        return j;
    }
}