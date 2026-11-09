/* BLAZE - Team DARK
   Recon module (plan 4.4 / 4.5 / 4.6):
   - 4.4 WPA handshake (EAPOL) capture -> /eapol.pcap (pcap, DLT 105)
   - 4.5 probe-request logging (RAM ring, 32 entries)
   - 4.6 vulnerability assessment + report export
     (/report.json, /report.csv, CLI "report")

   SAFETY RULE: sniff() runs in the SDK RX callback context.
   No flash writes, no printing, no heap churn there - frames are queued
   in RAM and flushed from update() in the main loop. */

#pragma once

#include <Arduino.h>

namespace BlazeRecon {
    void setup();
    void update(); // flush pcap queue + capture phase machine (main loop)

    // ===== RX callback hook (called from Scan::sniffer) =====
    void sniff(uint8_t* buf, uint16_t len);

    // ===== EAPOL capture session (4.4) =====
    // phase: 0 = off, 1 = KICK (4 s deauth), 2 = RECORD (sniff, max 45 s)
    bool     startCapture(int apIdx);
    void     stopCapture();
    bool     capturing();
    uint8_t  capturePhase();
    uint8_t  eapolFrames();   // frames captured in the current session
    uint32_t eapolTotal();    // frames captured this boot (all sessions)
    String   pcapPath();

    // ===== probe-request log (4.5) =====
    int      probeCount();    // entries currently in the ring
    uint32_t probeTotal();    // probe requests seen this boot
    String   probeLine(int i);// i = 0 is the newest entry
    void     clearProbes();

    // Read-only snapshot for PNL/victim/timeline (Phase 9).
    struct ProbeInfo {
        uint8_t  mac[6];
        char     ssid[33];
        uint8_t  ch;
        int8_t   rssi;
        uint32_t t;    // millis() when last seen
        uint16_t hits; // times this MAC+SSID was seen (saturated)
    };
    bool probeGet(int i, ProbeInfo& out); // i = 0 newest; false = out of range

    // ===== vulnerability assessment (4.6) =====
    int         riskScore();    // 0..100 - weakest link in view
    const char* riskLevel();    // NO DATA / LOW / MEDIUM / HIGH / CRITICAL
    int         worstAp();      // index of the most vulnerable AP (-1 = none)
    String      reportJson();
    String      reportCsv();
}
