/* BLAZE - Team DARK
   Glue between the deauther core and the BLAZE modules
   - boot: loading -> greeting -> wake-up -> anime face
   - tracker + fx tick (glitch tear, matrix rain) */

#include "BlazeCore.h"

#include "settings.h"
#include "Attack.h"
#include "Scan.h"
#include "DisplayUI.h"
#include "BlazeFace.h"
#include "BlazeTracker.h"
#include "BlazeFx.h"
#include "BlazePersona.h"
#include "BlazeChat.h"
#include "BlazeEvilTwin.h"
#include "BlazeRecon.h"
#include "BlazeDetector.h"
#include "BlazeCfg.h"
#include "BlazeLoyal.h"

extern Attack attack;
extern Scan   scan;
extern DisplayUI displayUI;

namespace BlazeCore {
    bool prevAttack = false;
    bool prevScan   = false; // wantLoop state of the scan beep
    bool bootLogged = false; // EV_BOOT fired once when the face wakes

    // KILL-mode state (plan 4.7)
    bool     prevKill   = false;
    uint32_t killStart  = 0;

    void setup() {
        BlazeTracker::setup();
        BlazeFx::setup();
        BlazePersona::setup();
        BlazeChat::setup();
        BlazeEvilTwin::setup();
        BlazeRecon::setup();
        BlazeDetector::setup(); // DETECTOR (Phase 7)
        BlazeLoyal::setup(); // OwnNetGuard + IntruderAlert (plan 2.3)
        BlazeFace::loadWeights(); // reaction weights editor (plan 3.2)

        if (settings::getDisplaySettings().enabled) {
            BlazeFace::setDisplay(&displayUI.display);
            BlazeFace::beginLoading();               // power-on: loading -> wake -> greeting -> face
            displayUI.mode = DISPLAY_MODE::LOADING;  // render the boot animation first
        }
    }

    void update() {
        BlazeFace::update();
        BlazeTracker::update();
        BlazePersona::update();
        BlazeEvilTwin::update();
        BlazeRecon::update(); // pcap flush + EAPOL capture phase machine
        BlazeDetector::update(); // detector cycle + confirm state machine
        BlazeLoyal::update(); // loyalty watchdogs (own-net sweep + intruder alert)

        // boot animation finished -> companion says hello (once)
        if (!bootLogged && BlazeFace::bootDone()) {
            bootLogged = true;
            BlazePersona::onEvent(BlazePersona::EV_BOOT);
        }

        // scanning / tracking -> keep the hunting stance alive (no line)
        bool scn = scan.isScanning();

        if (scn) BlazePersona::poke(BlazePersona::ST_PROWL, 3000);
        if (BlazeTracker::active() && BlazeTracker::hasSignal())
            BlazePersona::poke(BlazePersona::ST_PROWL, 3000);

        // attack start/stop -> persona + glitch burst
        bool atk = attack.isRunning();

        if (atk && !prevAttack) BlazePersona::onEvent(BlazePersona::EV_ATTACK);
        if (!atk && prevAttack) {
            BlazePersona::onEvent(BlazePersona::EV_ATTACK_DONE);
            BlazeFx::glitchStart();
        }
        prevAttack = atk;
        if (atk) BlazePersona::poke(BlazePersona::ST_HUNGRY, 3000); // feeding wins

        // ===== KILL mode polish (plan 4.7) =====
        // start -> KILL persona line; auto-stop after
        // kill.minutes (default 5) so a forgotten flood can't run forever
        bool kill = attack.isKill();

        if (kill && !prevKill) {
            killStart = millis();
            BlazePersona::onEvent(BlazePersona::EV_KILL);
        }
        if (kill) {
            long mins = BlazeCfg::getInt("kill.minutes", 5);
            if (mins < 1) mins = 1;
            if (millis() - killStart >= (uint32_t)mins * 60000UL) {
                prntln("[BLAZE] KILL auto-stop: time limit reached");
                attack.stop();
                kill = false;
            }
        }
        prevKill = kill;

        // scan finished -> persona blip
        if (!scn && prevScan) BlazePersona::onEvent(BlazePersona::EV_SCAN_DONE);
        prevScan = scn;
    }
}
