/* BLAZE - Team DARK
   BADUSB controller (Phase 8): drive an external Raspberry Pi Pico
   (CircuitPython HID) over UART1.

   Why external: the ESP8266 has no USB device peripheral. The NodeMCU
   USB port is a CH340 UART bridge, so BadUSB HID injection is physically
   impossible on the ESP alone. BLAZE sends a one-byte mode command to a
   Pico that enumerates as a keyboard.

   Wiring (3 wires + GND):
     BLAZE D4 / GPIO2  (UART1 TX) -> Pico GP1 (UART RX)
     BLAZE D0 / GPIO16 (ACK in)   <- Pico GP2 (presence/ACK out)
     GND                          -- GND
   Pico drives GP2 HIGH when alive and pulses LOW (~60 ms) as ACK.
   No Pico / no ACK -> the screen says "no duck." and nothing is typed.

   Protocol: '1' = NORMAL payload, '2' = CRITICAL payload, '0' = stop,
             '?' = ping (ACK only).
   SAFETY: no flash writes in update(); ACK polling is non-blocking. */

#pragma once

#include <Arduino.h>

namespace BlazeBadUSB {
    enum Mode : uint8_t {
        MODE_IDLE     = 0,
        MODE_NORMAL   = 1,
        MODE_CRITICAL = 2
    };

    void setup();
    void update(); // ACK timeout / link state (main loop, non-blocking)

    // Menu helpers
    void probe();               // send '?' and wait for ACK (link check)
    bool start(Mode mode);      // send '1'/'2'; false = busy/link wait
    void stop();                // send '0' (safe even when idle)
    bool running();             // mode command sent, not stopped
    bool linked();              // last command ACKed
    bool awaitingAck();
    uint8_t mode();
    const char* modeName();     // "NORMAL" / "CRITICAL" / "IDLE"
    const char* statusName();   // "ready" / "wait ack" / "no duck" / "sent"
    uint8_t sentCount();
    String reportJson();
}
