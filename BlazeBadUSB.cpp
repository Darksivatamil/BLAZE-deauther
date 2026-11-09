/* BLAZE - Team DARK
   BADUSB controller (Phase 8): one-byte UART link to an external Pico.
   The ESP8266 cannot be a USB HID device - the Pico does the typing.

   SAFETY: update() only polls a GPIO and a millis deadline. It never
   blocks, never writes flash, and never allocates in a hot path. */

#include "BlazeBadUSB.h"

#include "A_config.h"
#include "language.h"

namespace BlazeBadUSB {
    enum : uint8_t {
        ACK_IDLE = 0,
        ACK_WAIT = 1,
        ACK_OK   = 2,
        ACK_FAIL = 3
    };

    static bool     activeFlag = false;
    static uint8_t  modeVal    = MODE_IDLE;
    static uint8_t  ackState   = ACK_IDLE;
    static char     pendingCmd = 0;
    static uint32_t ackDeadline = 0;
    static bool     everLinked = false;
    static uint8_t  sentTotal  = 0;

    static void sendCmd(char c) {
        Serial1.write((uint8_t)c);
        Serial1.flush(); // one byte @9600 = ~1 ms
        pendingCmd  = c;
        ackState    = ACK_WAIT;
        ackDeadline = millis() + 400;
    }

    void setup() {
        Serial1.begin(9600);

        // GPIO16 is the only boot-safe free pin on the NodeMCU.
        // Pico drives it HIGH when alive and pulls it LOW for an ACK pulse.
        #if BADUSB_ACK_PIN == 16
        pinMode(BADUSB_ACK_PIN, INPUT_PULLDOWN_16);
        #else
        pinMode(BADUSB_ACK_PIN, INPUT);
        #endif

        activeFlag = false;
        modeVal    = MODE_IDLE;
        ackState   = ACK_IDLE;
        pendingCmd = 0;
        everLinked = false;
        sentTotal  = 0;
    }

    void update() {
        if (ackState != ACK_WAIT) return;

        // Pico ACK = active-low pulse on the presence line.
        if (digitalRead(BADUSB_ACK_PIN) == LOW) {
            ackState   = ACK_OK;
            everLinked = true;
            if ((pendingCmd == '1') || (pendingCmd == '2')) sentTotal++;
            pendingCmd = 0;
            return;
        }

        if ((int32_t)(millis() - ackDeadline) >= 0) {
            ackState = ACK_FAIL;
            pendingCmd = 0;
            if (activeFlag) { // command never reached the duck
                activeFlag = false;
                modeVal    = MODE_IDLE;
                prntln(String(F("BADUSB: no ACK - duck not connected")));
            }
        }
    }

    void probe() {
        if (activeFlag || (ackState == ACK_WAIT)) return;
        sendCmd('?');
    }

    bool start(Mode mode) {
        if (mode == MODE_IDLE) return false;
        if (activeFlag) return false;
        if ((ackState == ACK_WAIT) && (pendingCmd != '?')) return false;

        activeFlag = true;
        modeVal    = (uint8_t)mode;
        sendCmd((mode == MODE_CRITICAL) ? '2' : '1');

        prnt(F("BADUSB: sent "));
        prntln(modeName());
        return true;
    }

    void stop() {
        bool wasActive = activeFlag;
        activeFlag = false;
        modeVal    = MODE_IDLE;

        // Tell the duck to abort only when it may be running.
        if (wasActive || everLinked) sendCmd('0');
    }

    bool running()      { return activeFlag; }
    bool linked()       { return ackState == ACK_OK; }
    bool awaitingAck()  { return ackState == ACK_WAIT; }
    uint8_t mode()      { return modeVal; }
    uint8_t sentCount() { return sentTotal; }

    const char* modeName() {
        switch (modeVal) {
            case MODE_NORMAL:   return "NORMAL";
            case MODE_CRITICAL: return "CRITICAL";
            default:            return "IDLE";
        }
    }

    const char* statusName() {
        if (ackState == ACK_WAIT) return "wait ack";
        if (activeFlag)           return "sent";
        if (ackState == ACK_FAIL) return "no duck";
        if (everLinked)           return "ready";
        return "probe";
    }

    String reportJson() {
        String j = F("{\"running\":");
        j += activeFlag ? F("true") : F("false");
        j += F(",\"mode\":\"");
        j += modeName();
        j += F("\",\"ack\":");
        j += (ackState == ACK_OK) ? F("true") : F("false");
        j += F(",\"linked\":");
        j += everLinked ? F("true") : F("false");
        j += F(",\"sent\":");
        j += (int)sentTotal;
        j += F("}");
        return j;
    }
}
