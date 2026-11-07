/* BLAZE - Team DARK
   Persona engine - stance machine + offline companion lines.
   Replaces the old BlazeMood "mood" states with bold stances and
   drives the text-only persona output (OLED flash / serial / web log).
   No AI uplink - text only (see UPGRADE_PLAN.md). */

#pragma once

#include <Arduino.h>

namespace BlazePersona {
    // what BLAZE is doing right now
    enum Stance : uint8_t {
        ST_IDLE = 0,     // calm - baseline
        ST_PROWL,        // hunting - scanning / tracking
        ST_HUNGRY,       // feeding - an attack or twin is running
        ST_UNIMPRESSED,  // something went nowhere (failed login, lost signal)
        ST_LOCKED,       // jackpot - handshake captured
        ST_STANDBY       // offline - display switched off
    };

    // persona events -> stance + companion line
    enum Ev : uint8_t {
        EV_BOOT = 0,       // startup animation finished
        EV_ATTACK,         // attack started
        EV_ATTACK_DONE,    // attack stopped
        EV_SCAN_DONE,      // scan finished
        EV_EAPOL,          // handshake frame captured (first of a burst)
        EV_TWIN_START,     // evil twin portal opened
        EV_TWIN_CREDS,     // twin captured credentials
        EV_LOGIN_FAIL,     // refused login
        EV_TRACKER_LOST,   // tracked signal dropped
        EV_TRACKER_FOUND,  // tracked signal back
        EV_KILL,           // KILL mode armed
        EV_PANIC,          // global kill switch fired
        EV_HOUSE,          // OwnNetGuard refused a target (loyalty)
        EV_INTRUDER,       // home SSID probed/cloned (IntruderAlert)
        EV_BANTER,         // idle one-liner (60-120 s random, plan 2.1)
        EV_COUNT
    };

    void setup();
    void update(); // every loop: stance hold decay

    void onEvent(Ev e);                         // fire a persona event (stance + line)
    void poke(Stance s, uint32_t holdMs);       // re-assert a running stance (no line)
    void onStandby(bool sb);                    // display off -> STANDBY
    void flash(const String& txt, uint32_t ms); // OLED interjection (via DisplayUI)

    Stance      get();
    const char* stanceName();
    void        biasFace(int8_t b); // -2 .. +2 reaction pool bias

    // companion log (ring buffer, oldest first) for /blaze/log
    uint8_t     logCount();
    const char* logLine(uint8_t i); // 0 = oldest kept, logCount()-1 = newest
}
