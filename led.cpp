/* This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#include "led.h"

#include "A_config.h" // Config for LEDs
#include <Arduino.h>  // digitalWrite, analogWrite, pinMode
#include "language.h" // Strings used in printColor and tempDisable
#include "settings.h" // used in update()
#include "Attack.h"   // used in update()
#include "Scan.h"     // used in update()
#include "BlazePersona.h"

// Inlcude libraries for Neopixel or LED_MY92xx if used
#if defined(LED_NEOPIXEL)
#include "src/Adafruit_NeoPixel-1.7.0/Adafruit_NeoPixel.h"
#elif defined(LED_MY92)
#include "src/my92xx-3.0.3/my92xx.h"
#elif defined(LED_DOTSTAR)
#include "src/Adafruit_DotStar-1.1.4/Adafruit_DotStar.h"
#endif // if defined(LED_NEOPIXEL)

extern Attack attack;
extern Scan   scan;

namespace led {
    // ===== PRIVATE ===== //
    LED_MODE mode = OFF;
    uint8_t lastStance = 255; // 255 = stance color not applied (invalidate)

#if defined(LED_NEOPIXEL_RGB)
    Adafruit_NeoPixel strip { LED_NUM, LED_NEOPIXEL_PIN, NEO_RGB + NEO_KHZ800 };
#elif defined(LED_NEOPIXEL_GRB)
    Adafruit_NeoPixel strip { LED_NUM, LED_NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800 };
#elif defined(LED_MY92)
    my92xx myled { LED_MY92_MODEL, LED_NUM, LED_MY92_DATA, LED_MY92_CLK, MY92XX_COMMAND_DEFAULT };
#elif defined(LED_DOTSTAR)
    Adafruit_DotStar strip { LED_NUM, LED_DOTSTAR_DATA, LED_DOTSTAR_CLK, DOTSTAR_BGR };
#endif // if defined(LED_NEOPIXEL_RGB)


    void setColor(uint8_t r, uint8_t g, uint8_t b) {
#if defined(LED_DIGITAL)
        if (LED_ANODE) {
            if (LED_PIN_R < 255) digitalWrite(LED_PIN_R, r > 0);
            if (LED_PIN_G < 255) digitalWrite(LED_PIN_G, g > 0);
            if (LED_PIN_B < 255) digitalWrite(LED_PIN_B, b > 0);
        } else {
            if (LED_PIN_R < 255) digitalWrite(LED_PIN_R, r == 0);
            if (LED_PIN_G < 255) digitalWrite(LED_PIN_G, g == 0);
            if (LED_PIN_B < 255) digitalWrite(LED_PIN_B, b == 0);
        }
#elif defined(LED_RGB)
        if (r > 0) r = r * LED_MODE_BRIGHTNESS / 100;
        if (g > 0) g = g * LED_MODE_BRIGHTNESS / 100;
        if (b > 0) b = b * LED_MODE_BRIGHTNESS / 100;

        if (LED_ANODE) {
            r = 255 - r;
            g = 255 - g;
            b = 255 - b;
        }

        analogWrite(LED_PIN_R, r);
        analogWrite(LED_PIN_G, g);
        analogWrite(LED_PIN_B, b);
#elif defined(LED_NEOPIXEL) || defined(LED_DOTSTAR)

        for (size_t i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, r, g, b);
        }

        strip.show();
#elif defined(LED_MY9291)
        myled.setChannel(LED_MY92_CH_R, r);
        myled.setChannel(LED_MY92_CH_G, g);
        myled.setChannel(LED_MY92_CH_B, b);
        myled.setChannel(LED_MY92_CH_BRIGHTNESS, LED_MODE_BRIGHTNESS);
        myled.setState(true);
        myled.update();
#endif // if defined(LED_DIGITAL)
    }

    // ===== PUBLIC ===== //
    void setup() {
        analogWriteRange(0xff);

#if defined(LED_DIGITAL) || defined(LED_RGB)
        if (LED_PIN_R < 255) pinMode(LED_PIN_R, OUTPUT);
        if (LED_PIN_G < 255) pinMode(LED_PIN_G, OUTPUT);
        if (LED_PIN_B < 255) pinMode(LED_PIN_B, OUTPUT);
#elif defined(LED_NEOPIXEL) || defined(LED_DOTSTAR)
        strip.begin();
        strip.setBrightness(LED_MODE_BRIGHTNESS);
        strip.show();
#elif defined(LED_MY9291)
        myled.setChannel(LED_MY92_CH_R, 0);
        myled.setChannel(LED_MY92_CH_G, 0);
        myled.setChannel(LED_MY92_CH_B, 0);
        myled.setChannel(LED_MY92_CH_BRIGHTNESS, LED_MODE_BRIGHTNESS);
        myled.setState(true);
        myled.update();
#endif // if defined(LED_DIGITAL) || defined(LED_RGB)
    }

    // stance sync (plan phase 4): when no scan/attack drives the LED, it speaks the stance
    static void stanceColor() {
        switch (BlazePersona::get()) {
            case BlazePersona::ST_HUNGRY:      setColor(255, 0, 0);    break; // feeding: red
            case BlazePersona::ST_LOCKED:      setColor(0, 255, 255);  break; // jackpot: cyan
            case BlazePersona::ST_PROWL:       setColor(70, 0, 255);   break; // hunting: violet
            case BlazePersona::ST_UNIMPRESSED: setColor(255, 90, 0);   break; // burnt: amber
            case BlazePersona::ST_STANDBY:     setColor(0, 0, 0);      break; // screen off: dark
            default:                          setColor(LED_MODE_IDLE); break; // calm: base green
        }
    }

    void update() {
        if (!settings::getLEDSettings().enabled) {
            setMode(OFF);
        } else if (scan.isScanning() && (scan.deauths < settings::getSnifferSettings().min_deauth_frames)) {
            setMode(SCAN);
        } else if (attack.isRunning()) {
            setMode(ATTACK);
        } else {
            setMode(IDLE);
        }

        // stance sync: only IDLE mode yields the LED to the persona
        if (mode == IDLE) {
            uint8_t s = (uint8_t)BlazePersona::get();
            if (s != lastStance) {
                lastStance = s;
                stanceColor();
            }
        } else {
            lastStance = 255; // scan/attack/off own the LED - re-apply on return
        }
    }

    void setMode(LED_MODE new_mode, bool force) {
        if ((new_mode != mode) || force) {
            mode = new_mode;
            lastStance = 255; // any mode (re)write invalidates the stance color

            switch (mode) {
                case OFF:
                    setColor(LED_MODE_OFF);
                    break;
                case SCAN:
                    setColor(LED_MODE_SCAN);
                    break;
                case ATTACK:
                    setColor(LED_MODE_ATTACK);
                    break;
                case IDLE:
                    setColor(LED_MODE_IDLE);
                    break;
            }
        }
    }
}
