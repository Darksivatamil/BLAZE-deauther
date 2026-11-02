/* BLAZE - Team DARK
   Tiny key=value config on LittleFS (/blaze.cfg)
   keeps BLAZE settings out of the deauther EEPROM layout */

#pragma once

#include <Arduino.h>

namespace BlazeCfg {
    void   setup();
    String get(const String& key, const String& def = "");
    long   getInt(const String& key, long def);
    float  getFloat(const String& key, float def);
    bool   set(const String& key, const String& value);
}
