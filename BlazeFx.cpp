/* BLAZE - Team DARK
   Screen effects - see BlazeFx.h */

#include "BlazeFx.h"

#include "BlazeCfg.h"
#include "src/esp8266-oled-ssd1306-4.1.0/OLEDDisplay.h"

namespace BlazeFx {
    // ===== confetti ===== //
    const uint8_t  CF_N      = 18;
    const uint32_t CF_MILLIS = 2200;

    struct CFP {
        int16_t x;
        int16_t y;
        int8_t  vy;
        uint8_t kind;
    };

    CFP    cf[CF_N];
    uint32_t cfEnd = 0;

    void confettiStart() {
        for (uint8_t i = 0; i < CF_N; i++) {
            cf[i].x    = random(0, 128);
            cf[i].y    = random(-64, 0);
            cf[i].vy   = 2 + random(0, 5);
            cf[i].kind = random(0, 3);
        }
        cfEnd = millis() + CF_MILLIS;
    }

    bool confettiActive() {
        return (int32_t)(cfEnd - millis()) > 0;
    }

    void confettiDraw(OLEDDisplay* d) {
        if (!d || !confettiActive()) return;

        uint32_t now = millis();

        for (uint8_t i = 0; i < CF_N; i++) {
            cf[i].y += cf[i].vy;

            if (cf[i].y > 66) {
                cf[i].y = random(-16, 0);
                cf[i].x = random(0, 128);
            }

            // twinkle: skip some particles on some frames
            if (((now / 140) + i) % 4 == 0) continue;

            int16_t x = cf[i].x;
            int16_t y = cf[i].y;
            if (y < 0 || y > 63 || x < 0 || x > 127) continue;

            if (cf[i].kind == 0) {
                d->setPixel(x, y);                                  // dot
            } else if (cf[i].kind == 1) {
                d->setPixel(x, y);                                  // plus
                if (x > 0) d->setPixel(x - 1, y);
                if (x < 127) d->setPixel(x + 1, y);
                if (y > 0) d->setPixel(x, y - 1);
                if (y < 63) d->setPixel(x, y + 1);
            } else {
                d->setPixel(x, y);                                  // x-shape
                if (x > 0 && y > 0) d->setPixel(x - 1, y - 1);
                if (x < 127 && y < 63) d->setPixel(x + 1, y + 1);
                if (x < 127 && y > 0) d->setPixel(x + 1, y - 1);
                if (x > 0 && y < 63) d->setPixel(x - 1, y + 1);
            }
        }
    }

    // ===== matrix rain ===== //
    const uint8_t RN = 9;

    struct RCol {
        int8_t  x;
        int16_t head;
        int8_t  speed;
        int8_t  len;
    };

    RCol rc[RN];
    bool rainOn    = true;
    bool rainInitd = false;

    void rainRandomize(uint8_t i) {
        rc[i].x     = 6 + i * 14;
        rc[i].head  = random(-60, 64);
        rc[i].speed = 2 + random(0, 4);
        rc[i].len   = 5 + random(0, 8);
    }

    void setup() {
        rainOn = BlazeCfg::getInt("rain", 1) != 0;
        for (uint8_t i = 0; i < RN; i++) rainRandomize(i);
        rainInitd = true;
    }

    bool rainEnabled() {
        return rainOn;
    }

    void toggleRain() {
        rainOn = !rainOn;
        BlazeCfg::set("rain", rainOn ? "1" : "0");
    }

    void rainDraw(OLEDDisplay* d) {
        if (!d || !rainOn) return;
        if (!rainInitd) setup();

        for (uint8_t i = 0; i < RN; i++) {
            rc[i].head += rc[i].speed;

            if (rc[i].head - rc[i].len > 64) {
                rainRandomize(i);
                continue;
            }

            int16_t top = rc[i].head - rc[i].len;
            if (top < 0) top = 0;

            // dotted falling trail
            for (int16_t y = top; y < rc[i].head; y += 2) {
                if (y >= 0 && y < 64) d->setPixel(rc[i].x, y);
            }

            // bright head
            if (rc[i].head >= 0 && rc[i].head < 64) {
                d->setPixel(rc[i].x, rc[i].head);
                if (rc[i].head + 1 < 64) d->setPixel(rc[i].x, rc[i].head + 1);
            }
        }
    }
}
