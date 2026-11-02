/* BLAZE - Team DARK
   Mood state machine driven by attacks / tracker / idle */

#pragma once

#include <Arduino.h>

namespace BlazeMood {
    enum Mood : uint8_t { MOOD_CALM = 0, MOOD_CURIOS, MOOD_BRAVE, MOOD_SAD, MOOD_ALERT, MOOD_SLEEP };

    void setup();
    void update();

    void setMood(Mood m);
    Mood  get();
    const char* name();

    // event hooks (call from BlazeCore)
    void onAttackStart();
    void onAttackStop();
    void onScanDone();
    void onTrackerLost();
    void onTrackerFound();
}
