/* BLAZE - Team DARK
   Glue between the deauther core and the BLAZE modules
   - boot: loading -> greeting -> wake-up -> anime face
   - tracker + fx tick (confetti, matrix rain) */

#include "BlazeCore.h"

#include "settings.h"
#include "Attack.h"
#include "Scan.h"
#include "DisplayUI.h"
#include "BlazeFace.h"
#include "BlazeTracker.h"
#include "BlazeFx.h"
#include "BlazeMood.h"
#include "BlazeEvilTwin.h"
#include "BlazeRecon.h"
#include "BlazeCfg.h"

extern Attack attack;
extern Scan   scan;
extern DisplayUI displayUI;

namespace BlazeCore {
    bool prevAttack = false;
    bool prevScan   = false; // wantLoop state of the scan beep

    // KILL-mode state (plan 4.7)
    bool     prevKill   = false;
    uint32_t killStart  = 0;

    void setup() {
        BlazeTracker::setup();
        BlazeFx::setup();
        BlazeMood::setup();
        BlazeEvilTwin::setup();
        BlazeRecon::setup();
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
        BlazeMood::update();
        BlazeEvilTwin::update();
        BlazeRecon::update(); // pcap flush + EAPOL capture phase machine

        // attack start/stop -> mood + confetti
        bool atk = attack.isRunning();

        if (atk && !prevAttack) BlazeMood::onAttackStart();
        if (!atk && prevAttack) {
            BlazeMood::onAttackStop();
            BlazeFx::confettiStart();
        }
        prevAttack = atk;

        // ===== KILL mode polish (plan 4.7) =====
        // start -> alert mood + "KILL!" face flash; auto-stop after
        // kill.minutes (default 5) so a forgotten flood can't run forever
        bool kill = attack.isKill();

        if (kill && !prevKill) {
            killStart = millis();
            BlazeMood::setMood(BlazeMood::MOOD_ALERT);
            BlazeFace::flashMessage("KILL!", 1200);
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

        // scan finished -> mood blip
        bool scn = scan.isScanning();

        if (!scn && prevScan) BlazeMood::onScanDone();
        prevScan = scn;
    }
}
