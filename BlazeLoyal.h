/* BLAZE - Team DARK
   Loyalty watchdogs (UPGRADE_PLAN 2.3):
   - OwnNetGuard: the device's own AP SSID and the configured home network
     (BlazeCfg key "blaze.home", seeded from BLAZE_HOME_SSID) can never
     become an attack target - selection refuses it, Attack::start()
     double-checks every path (web/CLI/selectAll) as a safety net, and
     blockMac() stops packet-level leaks (deauthAll / one-off CLI sends).
   - IntruderAlert: the home SSID showing up in the probe log, or a clone
     of it in scan results (same name, different radio) while idle ->
     persona EV_INTRUDER (stance LOCKED, 30 s cooldown).
   Reuses existing state only - no new radio work. */

#pragma once

#include <Arduino.h>

namespace BlazeLoyal {
    void setup();   // load/seed "blaze.home", reset watchdog state
    void update();  // IntruderAlert polling (main loop, cheap)

    bool isProtected(const String& ssid); // device AP SSID or home SSID?
    bool allowSelect(const String& ssid); // false -> refusal flash (EV_HOUSE)
    bool allowAttack(bool useSsids);      // safety net for Attack::start():
                                           // drop protected selections, and when
                                           // beacon/probe run, refuse protected
                                           // payload names
    bool blockMac(const uint8_t* mac);    // absolute packet guard: true = blocked
                                           // (caller treats it as handled so its
                                           // target rotation keeps moving)
    void refused();                       // fire the refusal (serial + flash)

    String home();                       // configured home SSID ("" = unset)
    void   setHome(const String& ssid);  // CLI "set blaze.home <ssid>"
}
