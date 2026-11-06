/* BLAZE - Team DARK
   Loading screen + 26 anime face reactions + startup greeting
   Ported from NEW/blaze_face_features_ori.ino
   (adapted to the deauther ThingPulse OLED library, made non-blocking) */

#pragma once

#include <Arduino.h>
#include "src/esp8266-oled-ssd1306-4.1.0/OLEDDisplay.h"

namespace BlazeFace {
    // call once with the shared deauther display object
    void setDisplay(OLEDDisplay* d);

    // ===== BOOT SEQUENCE ===== //
    void beginLoading();          // loading sparks -> greeting -> wake-up -> random face
    bool bootDone();              // true once loading + greeting + wake-up finished
    void update();                // advance state machines (call every loop)

    // ===== RENDER (draw into buffer, DisplayUI flushes) ===== //
    void renderLoading();         // orbiting sparks + scan line + progress bar
    void renderFace();

    // ===== FACE MODE ===== //
    void startFace();             // pick a random reaction (weighted)
    void setBias(int8_t bias);// -2 unimpressed .. 0 idle .. +2 hungry (filters reaction pool)
    int8_t bias();
    uint8_t reaction();           // current reaction id
    const char* reactionName();   // label for the current reaction
    const char* reactionName(uint8_t idx); // label by reaction index

    // ===== REACTION WEIGHTS EDITOR (plan 3.2) ===== //
    uint8_t getWeight(uint8_t idx); // 0..20
    void setWeight(uint8_t idx, uint8_t w);
    void bumpWeight(uint8_t idx);   // +1, wraps 20 -> 0
    void resetWeights();
    void loadWeights();             // load BlazeCfg "face.weights" (CSV)
    void saveWeights();             // persist to BlazeCfg
    uint8_t weightCount();

    // ===== TEMP MESSAGE (persona flash line) ===== //
    void flashMessage(const String& txt, uint32_t ms);
    bool flashActive();
    void renderFlash();
}
