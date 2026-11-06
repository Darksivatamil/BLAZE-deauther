/* BLAZE - Team DARK
   Screen effects - see BlazeFx.h */

#include "BlazeFx.h"

#include "BlazeCfg.h"
#include "src/esp8266-oled-ssd1306-4.1.0/OLEDDisplay.h"

namespace BlazeFx {
    // ===== glitch (screen tear) ===== //
    // Shreds the already-drawn frame buffer: horizontal bands get shifted,
    // inverted and noised out, then the effect decays over GL_MILLIS.
    const uint32_t GL_MILLIS = 2200;
    uint32_t glEnd = 0;

    void glitchStart() {
        glEnd = millis() + GL_MILLIS;
    }

    bool glitchActive() {
        return (int32_t)(glEnd - millis()) > 0;
    }

    // shift one pixel row horizontally (wrapping) inside the page buffer
    static void tearRow(uint8_t* buf, int16_t y, int16_t shift) {
        if (y < 0 || y > 63 || !shift) return;
        uint8_t* page = buf + (y >> 3) * 128;
        uint8_t  m    = (uint8_t)(1 << (y & 7));

        uint8_t tmp[128];
        for (uint8_t x = 0; x < 128; x++) tmp[x] = (page[x] & m) ? 1 : 0;

        for (uint8_t x = 0; x < 128; x++) {
            int16_t sx = (int16_t)x - shift;
            if (sx < 0) sx += 128;
            else if (sx >= 128) sx -= 128;
            if (tmp[sx]) page[x] |= m;
            else         page[x] = (uint8_t)(page[x] & ~m);
        }
    }

    static void invertRow(uint8_t* buf, int16_t y) {
        if (y < 0 || y > 63) return;
        uint8_t* page = buf + (y >> 3) * 128;
        uint8_t  m    = (uint8_t)(1 << (y & 7));
        for (uint8_t x = 0; x < 128; x++) page[x] ^= m;
    }

    static void noiseRow(uint8_t* buf, int16_t y) {
        if (y < 0 || y > 63) return;
        uint8_t* page = buf + (y >> 3) * 128;
        uint8_t  m    = (uint8_t)(1 << (y & 7));
        for (uint8_t x = 0; x < 128; x++) {
            if (random(2)) page[x] |= m;
            else           page[x] = (uint8_t)(page[x] & ~m);
        }
    }

    void glitchDraw(OLEDDisplay* d) {
        if (!d || !d->buffer || !glitchActive()) return;

        uint32_t left = glEnd - millis();
        // full strength for the first ~1.2s, then decay to a light flicker
        uint8_t intensity = 4;
        if (left < 1000) intensity = (uint8_t)(left / 250) + 1; // 1..4

        uint8_t* buf = d->buffer;

        // torn bands (horizontal displacement)
        uint8_t bands = (uint8_t)(2 + random(0, intensity));
        for (uint8_t b = 0; b < bands; b++) {
            int16_t y0    = random(0, 64);
            int16_t h     = 2 + random(0, 4 + 2 * intensity);
            int16_t shift = random(-6 - 4 * intensity, 6 + 4 * intensity);
            if (y0 + h > 64) h = 64 - y0;
            for (int16_t y = y0; y < y0 + h; y++) tearRow(buf, y, shift);
        }

        // inverted slice
        if (random(100) < 8 * intensity) {
            int16_t y0 = random(0, 64);
            int16_t h  = 1 + random(0, 4);
            for (int16_t y = y0; y < y0 + h && y < 64; y++) invertRow(buf, y);
        }

        // static-noise line
        if (random(100) < 6 * intensity) noiseRow(buf, random(0, 64));
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
