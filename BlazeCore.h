/* BLAZE - Team DARK
   Glue between the deauther core and the BLAZE modules
   - boot: 4s loading -> wake-up -> anime face
   - sound hooks driven by attack/scan state (read-only, no core edits) */

#pragma once

#include <Arduino.h>

namespace BlazeCore {
    void setup();   // call once from .ino setup() (after displayUI.setup)
    void update();  // call every loop()
}
