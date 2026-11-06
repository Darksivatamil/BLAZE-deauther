/* BLAZE - Team DARK
   CHAT mode - offline companion chat (no uplink, see UPGRADE_PLAN.md)
   - canned phrase wheel (UP/DOWN), A sends (on-device)
   - offline keyword brain (also serves web POST /blaze/chat + CLI)
   - screen: top = stance eyes, input line, latest exchange below */

#pragma once

#include <Arduino.h>

class OLEDDisplay;

namespace BlazeChat {
    void setup(); // seed the log

    void move(int8_t d);               // wheel scroll (UP = -1, DOWN = +1)
    void send();                       // A: send the highlighted phrase
    String sendMsg(const String& raw); // web / CLI entry, returns the reply

    void render(OLEDDisplay* d); // full-screen draw while mode == CHAT

    uint8_t     logCount();
    const char* logLine(uint8_t i); // 0 = oldest kept, logCount()-1 = newest
}
