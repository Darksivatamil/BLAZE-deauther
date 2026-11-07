/* BLAZE - Team DARK
   TRACKER mode engine - see BlazeTracker.h */

#include "BlazeTracker.h"

#include <math.h>

#include "Accesspoints.h"
#include "Attack.h"
#include "BlazeCfg.h"
#include "Scan.h"

extern Scan       scan;
extern Attack     attack;
extern uint32_t   currentTime;
extern uint8_t    wifi_channel;
extern void getRandomMac(uint8_t* mac);

namespace BlazeTracker {
    const uint8_t  HIST_LEN        = 40;
    const uint32_t WORK_INTERVAL   = 250;   // main tick
    const uint32_t HIST_INTERVAL   = 600;   // sparkline sample
    const uint32_t SCAN_INTERVAL   = 6000;  // auto re-scan while tracking
    const uint32_t LOST_RSSI_TIME  = 12000; // rssi unchanged this long -> lost
    const uint32_t LOST_INDEX_TIME = 4000;  // target missing from list this long -> lost

    // ---- state ---- //
    bool    activeFlag      = false;
    bool    pickFlag        = false; // target-select step before tracking
    int     pickIdx         = 0;     // highlighted row in the pick list
    bool    calibratingFlag = false;
    bool    lostFlag        = false;
    uint8_t mac[6]          = { 0, 0, 0, 0, 0, 0 };
    String  ssid;
    int     idx        = -1;
    int     lastRaw    = 0;
    bool    emaInit    = false;
    float   ema        = -70.0f;
    float   txp         = -42.0f; // dBm @ 1m (calibratable)
    float   envN       = 2.7f;   // path loss exponent
    float   minDist    = 999.0f;
    int8_t  dir        = 0;
    float   dirRef     = -70.0f;
    int16_t hist[HIST_LEN];
    uint8_t histLen    = 0;
    uint8_t histHead   = 0;
    uint32_t lastWork  = 0;
    uint32_t lastHist  = 0;
    uint32_t lastScan  = 0;
    uint32_t lastBeep  = 0;
    uint32_t lastChg   = 0; // last time the raw rssi VALUE changed
    uint32_t dirTime   = 0;
    uint32_t noIdxSince = 0;
    bool    prevScanning = false;
    bool    weScanned  = false;
    Alert   pendingAlert = ALERT_NONE;

    // ---- helpers ---- //
    void pushHist(int16_t v) {
        hist[histHead] = v;
        histHead = (histHead + 1) % HIST_LEN;
        if (histLen < HIST_LEN) histLen++;
    }

    float calcDist() {
        if (!emaInit) return -1.0f;
        float d = powf(10.0f, (txp - ema) / (10.0f * envN));
        if (d < 0.1f) d = 0.1f;
        if (d > 99.0f) d = 99.0f;
        return d;
    }

    void setLost(bool l) {
        if (lostFlag == l) return;
        lostFlag = l;
        pendingAlert = l ? ALERT_LOST : ALERT_FOUND;
    }

    // re-bind the list index to our stored BSSID (indices move after every scan)
    void resolve() {
        int c    = accesspoints.count();
        int found = -1;

        for (int i = 0; i < c; i++) {
            uint8_t* m = accesspoints.getMac(i);
            if (m && memcmp(m, mac, 6) == 0) {
                found = i;
                break;
            }
        }

        int oldIdx = idx;
        idx        = found;
        if (found < 0) return;

        if (found != oldIdx) ssid = accesspoints.getSSID(found);

        int raw = accesspoints.getRSSI(found);
        if (raw <= -1 && raw >= -101) {
            noIdxSince = 0;
            if (lostFlag) setLost(false); // target back in scan results
        }
    }

    // ---- public ---- //

    void setup() {
        txp = BlazeCfg::getFloat("txp", -42.0f);
        envN    = BlazeCfg::getFloat("env", 2.7f);
        if (envN < 1.0f || envN > 6.0f) envN = 2.7f;
        if (txp > -10.0f || txp < -100.0f) txp = -42.0f;
        pickFlag = false;
        pickIdx  = 0;
    }

    // ===== target pick (mandatory before start) ===== //

    void beginPick() {
        int c = accesspoints.count();
        pickFlag = true;
        activeFlag = false;
        calibratingFlag = false;

        // pre-highlight the first starred AP, else the top row
        pickIdx = 0;
        for (int i = 0; i < c; i++) {
            if (accesspoints.getSelected(i)) { pickIdx = i; break; }
        }
        if (c > 0 && pickIdx >= c) pickIdx = c - 1;
        if (c <= 0) pickIdx = 0;
    }

    bool picking() { return pickFlag; }

    int pickCount() { return accesspoints.count(); }

    int pickIndex() { return pickIdx; }

    String pickSsid(int i) {
        if (i < 0 || i >= accesspoints.count()) return String();
        return accesspoints.getSSID(i);
    }

    int pickRssi(int i) {
        if (i < 0 || i >= accesspoints.count()) return 0;
        return accesspoints.getRSSI(i);
    }

    int pickCh(int i) {
        if (i < 0 || i >= accesspoints.count()) return 0;
        return accesspoints.getCh(i);
    }

    bool pickSelected(int i) {
        if (i < 0 || i >= accesspoints.count()) return false;
        return accesspoints.getSelected(i);
    }

    void pickMove(int8_t d) {
        int c = accesspoints.count();
        if (c <= 0) return;
        pickIdx += d;
        while (pickIdx >= c) pickIdx -= c;
        while (pickIdx < 0) pickIdx += c;
    }

    bool pickConfirm() {
        if (!pickFlag) return false;
        int c = accesspoints.count();
        if (c <= 0) { pickFlag = false; return false; }
        if (pickIdx < 0) pickIdx = 0;
        if (pickIdx >= c) pickIdx = c - 1;
        pickFlag = false;
        return start(pickIdx);
    }

    void pickCancel() {
        pickFlag = false;
    }

    bool start(int listIndex) {
        int c = accesspoints.count();
        if (listIndex < 0 || listIndex >= c) return false;

        uint8_t* m = accesspoints.getMac(listIndex);
        if (!m) return false;
        memcpy(mac, m, 6);

        pickFlag          = false;
        ssid            = accesspoints.getSSID(listIndex);
        idx             = listIndex;
        activeFlag      = true;
        calibratingFlag = false;
        lostFlag        = false;
        emaInit         = false;
        lastRaw         = 0;
        minDist         = 999.0f;
        dir             = 0;
        dirRef          = -70.0f;
        histLen         = 0;
        histHead        = 0;
        lastChg         = millis();
        dirTime         = millis();
        noIdxSince      = millis();
        lastWork        = millis();
        lastHist        = millis();
        lastBeep        = millis();
        lastScan        = millis() - SCAN_INTERVAL + 1500; // first re-scan after 1.5s
        prevScanning    = scan.isScanning();
        weScanned       = false;
        return true;
    }

    bool startBest() {
        int c = accesspoints.count();
        if (c <= 0) return false;
        for (int i = 0; i < c; i++) {
            if (accesspoints.getSelected(i)) return start(i);
        }
        return start(0);
    }

    void stop() {
        pickFlag         = false;
        if (!activeFlag) return;
        activeFlag      = false;
        calibratingFlag = false;
        if (weScanned && scan.isScanning()) scan.stop();
        weScanned = false;
    }

    bool active() {
        return activeFlag;
    }

    void update() {
        if (!activeFlag) return;

        uint32_t now   = millis();
        bool     sc    = scan.isScanning();

        if (prevScanning && !sc && weScanned) weScanned = false;
        prevScanning = sc;

        if (now - lastWork < WORK_INTERVAL) return;
        lastWork = now;

        resolve();

        // ---- rssi / ema / direction / distance ---- //
        bool haveTarget = (idx >= 0);
        int  raw        = haveTarget ? accesspoints.getRSSI(idx) : 0;
        bool valid      = haveTarget && (raw <= -1) && (raw >= -101);

        if (valid) {
            if (!emaInit) {
                ema      = (float)raw;
                emaInit  = true;
                dirRef   = ema;
                lastChg  = now;
                dirTime  = now;
            } else {
                if (raw != lastRaw) lastChg = now;
                ema = 0.7f * ema + 0.3f * (float)raw;
            }
            lastRaw = raw;

            if (now - dirTime > 3000) {
                float delta = ema - dirRef;
                if (delta >= 1.5f) dir = 1;
                else if (delta <= -1.5f) dir = -1;
                else dir = 0;
                dirRef = ema;
                dirTime = now;
            }

            float d = calcDist();
            if (d > 0.0f && d < minDist) minDist = d;

            if (now - lastHist >= HIST_INTERVAL) {
                lastHist = now;
                pushHist((int16_t)ema);
            }
        }

        // ---- lost signal ---- //
        if (!sc) {
            if (!haveTarget) {
                if (noIdxSince == 0) noIdxSince = now;
                else if (now - noIdxSince > LOST_INDEX_TIME) setLost(true);
            } else {
                noIdxSince = 0;
                // only judge stale rssi when nothing blocks fresh AP scans
                if (emaInit && !attack.isRunning() && !scan.isSniffing() &&
                    (now - lastChg > LOST_RSSI_TIME)) setLost(true);
            }
        } else {
            // during a scan the list is rebuilt - don't judge
            if (noIdxSince == 0) noIdxSince = now;
        }

        // ---- auto re-scan (fresh rssi) ---- //
        if (!sc && !attack.isRunning() && !scan.isSniffing() &&
            (now - lastScan >= SCAN_INTERVAL)) {
            lastScan  = now;
            weScanned = true;
            scan.start(SCAN_MODE_APS, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
        }

        // ---- probe ping: elicit responses from the target while tracking ---- //
        if (!sc && !attack.isRunning() && !scan.isSniffing() && haveTarget && !lostFlag) {
            static uint32_t lastProbe = 0;
            if (now - lastProbe >= 2500) {
                lastProbe = now;
                uint8_t* m = accesspoints.getMac(idx);
                String   s = accesspoints.getSSID(idx);
                uint8_t  c = accesspoints.getCh(idx);
                if (m && s.length() > 0) {
                    uint8_t pm[6];
                    getRandomMac(pm);
                    attack.sendProbe(pm, s.c_str(), c);
                }
            }
        }

        // ---- distance indicator (visual only) ---- //
        // cadence is driven by the OLED gauge; nothing to tick here
    }

    void selectNext() {
        if (!activeFlag) return;
        int c = accesspoints.count();
        if (c <= 0) return;
        int n = (idx >= 0) ? idx + 1 : 0;
        if (n >= c) n = 0;
        start(n);
    }

    void selectPrev() {
        if (!activeFlag) return;
        int c = accesspoints.count();
        if (c <= 0) return;
        int n = (idx >= 0) ? idx - 1 : c - 1;
        if (n < 0) n = c - 1;
        start(n);
    }

    void selectHold() {
        if (!activeFlag) return;
        calibratingFlag = !calibratingFlag;
    }

    void calibrateSave() {
        doCalibrate();
        calibratingFlag = false;
    }

    void calibrateCancel() {
        calibratingFlag = false;
    }

    void doCalibrate() {
        if (!activeFlag || !emaInit) return;
        txp = ema; // 1m reference: d = 1 -> txp = rssi
        BlazeCfg::set("txp", String(txp, 1));
    }

    bool calibrating() {
        return calibratingFlag;
    }

    bool lost() {
        return lostFlag;
    }

    bool hasSignal() {
        return emaInit && (idx >= 0);
    }

    int rssi() {
        return lastRaw;
    }

    float distance() {
        return calcDist();
    }

    float minDistance() {
        if (minDist > 900.0f) return -1.0f;
        return minDist;
    }

    float txPower() {
        return txp;
    }

    float envFactor() {
        return envN;
    }

    String envName() {
        if (envN < 2.35f) return String("OPEN");
        if (envN < 3.1f) return String("HOME");
        if (envN < 4.2f) return String("WALLS");
        return String("CUSTOM");
    }

    void setEnvFactor(float n) {
        if (n < 1.0f || n > 6.0f) return;
        envN = n;
        BlazeCfg::set("env", String(envN, 1));
    }

    void setEnvName(const String& name) {
        String s = name;
        s.toUpperCase();
        if (s == "OPEN") setEnvFactor(2.0f);
        else if (s == "HOME") setEnvFactor(2.7f);
        else if (s == "WALLS") setEnvFactor(3.5f);
        else {
            float v = name.toFloat();
            if (v >= 1.0f && v <= 6.0f) setEnvFactor(v);
        }
    }

    void setTxPower(float t) {
        if (t > -10.0f || t < -100.0f) return;
        txp = t;
        BlazeCfg::set("txp", String(txp, 1));
    }

    int direction() {
        return dir;
    }

    String targetSsid() {
        return ssid;
    }

    int targetIndex() {
        return idx;
    }

    uint8_t histCount() {
        return histLen;
    }

    int histAt(uint8_t i) {
        if (i >= histLen) return 0;
        uint8_t start = (histHead + HIST_LEN - histLen) % HIST_LEN;
        return hist[(start + i) % HIST_LEN];
    }

    Alert takeAlert() {
        Alert a = pendingAlert;
        pendingAlert = ALERT_NONE;
        return a;
    }

    String jsonEsc(String s) {
        s.replace("\\", "\\\\");
        s.replace("\"", "\\\"");
        s.replace("\n", " ");
        return s;
    }

    String toJson() {
        String j = "{\"active\":";
        j += activeFlag ? "true" : "false";
        j += ",\"picking\":";
        j += pickFlag ? "true" : "false";
        j += ",\"pickIdx\":" + String(pickIdx);
        j += ",\"cal\":";
        j += calibratingFlag ? "true" : "false";
        j += ",\"lost\":";
        j += lostFlag ? "true" : "false";
        j += ",\"scanning\":";
        j += scan.isScanning() ? "true" : "false";
        j += ",\"hasSignal\":";
        j += hasSignal() ? "true" : "false";
        j += ",\"ssid\":\"" + jsonEsc(ssid) + "\"";
        j += ",\"idx\":" + String(idx);
        j += ",\"rssi\":" + String(lastRaw);
        j += ",\"ema\":" + String((int)(ema < 0 ? ema - 0.5f : ema + 0.5f));
        float d = distance();
        j += ",\"dist\":";
        j += (d > 0 ? String(d, 2) : String("null"));
        j += ",\"min\":";
        j += (minDist < 900.0f ? String(minDist, 2) : String("null"));
        j += ",\"txp\":" + String(txp, 1);
        j += ",\"env\":" + String(envN, 1);
        j += ",\"envName\":\"" + envName() + "\"";
        j += ",\"dir\":" + String(dir);
        j += ",\"hist\":[";
        for (uint8_t i = 0; i < histLen; i++) {
            j += String(histAt(i));
            if (i + 1 < histLen) j += ',';
        }
        j += "],\"aps\":[";
        int c = accesspoints.count();
        for (int i = 0; i < c && i < 24; i++) {
            j += "{\"i\":" + String(i);
            j += ",\"ssid\":\"" + jsonEsc(accesspoints.getSSID(i)) + "\"";
            j += ",\"rssi\":" + String(accesspoints.getRSSI(i));
            j += ",\"sel\":";
            j += accesspoints.getSelected(i) ? "true" : "false";
            j += ",\"ch\":" + String(accesspoints.getCh(i)) + "}";
            if ((i + 1 < c) && (i + 1 < 24)) j += ',';
        }
        j += "]}";
        return j;
    }
}
