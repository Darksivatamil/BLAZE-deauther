/* BLAZE - Team DARK
   Probe intelligence (Phase 9):
     - Hidden-SSID unmasker: AP scan -> channel-hop listen -> directed kick
       -> second listen. Association requests give an exact BSSID match;
       directed probe requests on a channel with exactly one hidden AP give a
       candidate (marked ~ on screen, never presented as fact). Conflicting
       candidates become ambiguous (?).
     - PNL harvesting: aggregate the existing 32-entry probe ring into
       preferred-network entries (SSID, clients, probes, best RSSI).
     - Victim ID: unique probing clients + vendor + last SSID.
     - Probe forensics timeline: probe ring sorted newest-first.

   SAFETY: sniff() runs in the SDK RX callback context. It only reads a
   fixed table and copies into fixed char buffers - no String, no flash
   writes, no printing, no allocation. Everything else runs in update()
   or in draw/getters on the main loop. OwnNetGuard still blocks every
   deauth at the packet layer (Attack::deauthDevice). */

#pragma once

#include <Arduino.h>

namespace BlazeIntel {
    // hidden-unmasker phases (public so the UI can name them)
    enum : uint8_t {
        HID_OFF      = 0,
        HID_SCAN     = 1,
        HID_LISTEN   = 2,
        HID_KICK     = 3,
        HID_RELISTEN = 4,
        HID_DONE     = 5
    };

    void setup();
    void update(); // hidden-unmasker state machine (main loop)

    // ===== RX callback hook (called from Scan::sniffer) =====
    void sniff(uint8_t* buf, uint16_t len); // RAM-only, hidden listen only

    // ===== hidden-SSID unmasker =====
    bool    startHidden(); // false = air busy
    void    stopHidden();
    bool    hiddenRunning();
    uint8_t hiddenPhase();
    const char* hiddenPhaseName();
    int     hiddenCount();
    int     hiddenUnmasked();
    bool    hiddenAllDone();
    String  hiddenLine(int i);   // "=ssid XXXX" / "~ssid XXXX" / "?<ambig>"
    String  hiddenDetail(int i); // "ch6 -42 AABBCC a"

    // ===== PNL harvesting (snapshot of the probe ring) =====
    void    pnlBuild();
    int     pnlCount();
    String  pnlLine(int i);      // "ssid                  3u"
    String  pnlSsid(int i);
    uint8_t pnlClients(int i);
    uint16_t pnlProbeCount(int i);
    int8_t  pnlRssi(int i);
    uint8_t pnlCh(int i);
    uint32_t pnlAgeSec(int i);

    // ===== victim ID (unique probing clients) =====
    void    victimBuild();
    int     victimCount();
    String  victimLine(int i);   // "VENDOR  AABBCC"
    String  victimVendor(int i);
    String  victimMac(int i);
    String  victimSsid(int i);
    uint16_t victimProbeCount(int i);
    int8_t  victimRssi(int i);
    uint8_t victimCh(int i);
    uint32_t victimAgeSec(int i);

    // ===== probe forensics timeline =====
    void   timelineBuild();      // sort the probe ring newest-first
    int    timelineCount();
    String timelineLine(int i);  // "12s c6 AABBCC ssid" (CLI/report)
    String timelineSsid(int i);  // left column for the OLED
    String timelineRight(int i); // "12s c6"

    // ===== web =====
    String reportJson();
}
