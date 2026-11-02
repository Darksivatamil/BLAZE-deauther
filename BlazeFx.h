/* BLAZE - Team DARK
   Screen effects:
   - confetti burst (2s) when an attack finishes
   - matrix rain background behind the menu text */

#pragma once

#include <Arduino.h>

class OLEDDisplay;

namespace BlazeFx {
    void setup(); // loads rain setting from cfg

    // ---- confetti ---- //
    void confettiStart();
    bool confettiActive();
    void confettiDraw(OLEDDisplay* d); // advances + paints, call after scene draw

    // ---- matrix rain (menu background) ---- //
    bool rainEnabled();
    void toggleRain();                  // flips + saves to cfg
    void rainDraw(OLEDDisplay* d);      // call BEFORE drawing menu rows
}
