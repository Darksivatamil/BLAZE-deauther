/* BLAZE - Team DARK
   DETECTOR (Phase 7): find other deauthers / fake beacons on the air.
   Every verdict is backed by measured frames - no guesses:
     INFO (0)     - factual observation (duplicate SSID, same security)
     LIKELY (1)   - measured anomaly (mixed-security clone, spoofed source,
                    beacon/SSID flood, wrong-channel beacon)
     CONFIRMED (2)- measured attack behavior (deauth/beacon flood rates
                    far beyond spec, or active proof: 2+ radios with
                    DIFFERENT security answering the same SSID)
   Cycle: WiFi scan -> 10 s channel-hop sniff -> analyze -> findings.
   Active proof: directed probe -> responses from distinct BSSIDs.
   Honest limits: single radio hears one channel at a time; verdicts
   are graded by evidence, never guessed.

   SAFETY: sniff() runs in the SDK RX callback context - RAM only,
   no flash writes, no printing, no heap churn. Analysis runs in
   update() on the main loop. OwnNetGuard: never probes own/home SSID.

   JSON schema (/detect.json, reportJson) - consumers depend on it:
   {"running":bool,"phase":0-4,"count":N,"findings":[{"level":0-2,
     "type":"DEAUTH_FLOOD|DEAUTH_SPOOF|BEACON_SPAM|SSID_FLOOD|DUP_SSID|CH_MISMATCH",
     "ssid":"...","detail":"...","confirm":bool}, ...]} */

#pragma once

#include <Arduino.h>

namespace BlazeDetector {
    void setup();
    void update(); // cycle + confirm state machine (main loop)

    // ===== RX callback hook (called from Scan::sniffer) =====
    void sniff(uint8_t* buf, uint16_t len); // RAM-only accounting

    // ===== cycle ===== //
    bool start();   // scan -> sniff -> analyze; false = refused (attack/scan/twin busy)
    void stop();    // abort cycle/confirm, release sniff (findings kept)
    bool running(); // cycle in progress?
    int  phase();   // 0 idle, 1 scanning, 2 sniffing, 3 analyzing, 4 confirming

    // ===== active twin proof (directed probe -> responses) ===== //
    bool confirm(int idx); // finding idx must be type DUP; false = refused
    bool confirming();

    // ===== findings ===== //
    int      findingCount();
    String   findingLine(int i);   // short display line (level badge + type + ssid)
    String   findingDetail(int i); // evidence line (the numbers behind the verdict)
    uint8_t  findingLevel(int i);  // 0 INFO, 1 LIKELY, 2 CONFIRMED
    void     clear();

    // ===== exports ===== //
    String reportJson(); // schema in comment above
}
