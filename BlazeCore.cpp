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

extern Attack attack;
extern Scan   scan;
extern DisplayUI displayUI;

namespace BlazeCore {
    bool prevAttack = false;
    bool prevScan   = false; // wantLoop state of the scan beep

    void setup() {
        BlazeTracker::setup();
        BlazeFx::setup();
        BlazeMood::setup();
        BlazeEvilTwin::setup();

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

        // attack start/stop -> mood + confetti
        bool atk = attack.isRunning();

        if (atk && !prevAttack) BlazeMood::onAttackStart();
        if (!atk && prevAttack) {
            BlazeMood::onAttackStop();
            BlazeFx::confettiStart();
        }
        prevAttack = atk;

        // scan finished -> mood blip
        bool scn = scan.isScanning();

        if (!scn && prevScan) BlazeMood::onScanDone();
        prevScan = scn;
    }
}
