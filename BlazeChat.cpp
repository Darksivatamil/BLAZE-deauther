/* BLAZE - Team DARK
   CHAT mode - see BlazeChat.h */

#include "BlazeChat.h"

#include "language.h"
#include "DisplayUI.h"
#include "BlazePersona.h"
#include "Scan.h"
#include "Attack.h"
#include "BlazeEvilTwin.h"
#include "Accesspoints.h"
#include "src/esp8266-oled-ssd1306-4.1.0/OLEDDisplay.h"

extern DisplayUI displayUI;
extern Attack    attack;
extern Scan      scan;

namespace BlazeChat {
    static const uint8_t MAX_MSG = 40; // fits 2 wrapped log lines on the OLED

    // ===== canned phrase wheel (on-device input) ===== //
    static const uint8_t WHEEL_N = 8;
    static const char* const WHEEL[WHEEL_N] PROGMEM = {
        "status?", "report", "uptime?", "stance?",
        "what do you see?", "who's there?", "roast me", "help"
    };
    static uint8_t wheelIdx = 0;

    // ===== chat log ring (OLED + /blaze/chat) ===== //
    static const uint8_t LOG_N = 8;
    static String    logBuf[LOG_N];
    static uint8_t   logHead = 0;
    static uint8_t   logNum  = 0;

    // ===== offline brain: keyword rules ===== //
    struct Rule {
        const char* key;
        const char* reply;
    };

    static const Rule RULES[] PROGMEM = {
        { "who's there",  "me. who's asking?" },
        { "who is there", "me. who's asking?" },
        { "roast",        "your wifi is showing. patch it." },
        { "hack",         "menus do the killing. chat doesn't." },
        { "deauth",       "menus do the killing. chat doesn't." },
        { "attack",       "menus do the killing. chat doesn't." },
        { "kick",         "menus do the killing. chat doesn't." },
        { "kill",         "menus do the killing. chat doesn't." },
        { "password",     "the twin hands them over." },
        { "wpa",          "the twin hands them over." },
        { "wifi",         "scan first. then the twin." },
        { "network",      "scan first. then the twin." },
        { "cute",         "wrong device." },
        { "love",         "... no." },
        { "thank",        "don't." },
        { "bye",          "leave then." },
        { "blaze",        "talking to yourself?" },
        { "stupid",       "project yourself elsewhere." },
        { "smart",        "obviously." }
    };
    static const uint8_t RULES_N = sizeof(RULES) / sizeof(RULES[0]);

    static const char* const DEFAULTS[4] PROGMEM = {
        "try harder.",
        "words. impressive.",
        "i don't do small talk.",
        "ask me: status, report, uptime."
    };

    // ===== helpers ===== //
    static String wheelStr() {
        return String(FPSTR((const char*)pgm_read_ptr(&WHEEL[wheelIdx])));
    }

    static void logPush(const String& s) {
        logBuf[logHead] = s;
        logHead = (uint8_t)((logHead + 1) % LOG_N);
        if (logNum < LOG_N) logNum++;
    }

    static String latest(uint8_t back) { // 0 = newest
        if (back >= logNum) return String();
        uint8_t slot = (uint8_t)((logHead + LOG_N - 1 - back) % LOG_N);
        return logBuf[slot];
    }

    // ===== query answers ===== //
    static String stStatus() {
        String s = "scn:";
        s += scan.isScanning() ? "on atk:" : "off atk:";
        s += attack.isRunning() ? "on twin:" : "off twin:";
        s += BlazeEvilTwin::active() ? "on " : "off ";
        s += BlazePersona::stanceName();
        return s;
    }

    static String stReport() {
        String s = String(accesspoints.count()) + " aps up ";
        uint32_t mins = millis() / 60000;
        s += String(mins / 60) + "h" + String(mins % 60) + "m";
        return s;
    }

    static String stUptime() {
        uint32_t mins = millis() / 60000;
        return "up " + String(mins / 60) + "h" + String(mins % 60) + "m";
    }

    static String stStance() {
        return String(BlazePersona::stanceName());
    }

    static String stSee() {
        return String(accesspoints.count()) + " nets. none of them safe.";
    }

    // lo must be lowercase
    static String brain(const String& lo) {
        // greetings / filler - exact matches only
        if (lo == "hi" || lo == "hey" || lo == "hello" || lo == "yo" || lo == "sup")
            return String(F("say something useful."));
        if (lo == "ok" || lo == "k" || lo == "lol") return String(F("riveting."));

        // query answers
        if (lo.indexOf("status") >= 0) return stStatus();
        if (lo.indexOf("report") >= 0) return stReport();
        if (lo.indexOf("uptime") >= 0 || lo.indexOf("up time") >= 0) return stUptime();
        if (lo.indexOf("stance") >= 0) return stStance();
        if (lo.indexOf("what do you see") >= 0 || lo.indexOf("see") >= 0) return stSee();
        if (lo.indexOf("help") >= 0) return String(F("ask: status, report, uptime, stance."));

        // keyword rules
        for (uint8_t i = 0; i < RULES_N; i++) {
            const char* key = (const char*)pgm_read_ptr(&RULES[i].key);
            if (lo.indexOf(key) >= 0)
                return String(FPSTR((const char*)pgm_read_ptr(&RULES[i].reply)));
        }

        // default
        uint8_t d = (uint8_t)random(4);
        return String(FPSTR((const char*)pgm_read_ptr(&DEFAULTS[d])));
    }

    // ===== public API ===== //
    void setup() {
        wheelIdx = 0;
        logHead  = 0;
        logNum   = 0;
        logPush(String(F("blaze> ask me: status, report, uptime.")));
    }

    void move(int8_t d) {
        int8_t i = (int8_t)wheelIdx + d;
        if (i < 0) i = (int8_t)(WHEEL_N - 1);
        if (i >= (int8_t)WHEEL_N) i = 0;
        wheelIdx = (uint8_t)i;
    }

    void send() {
        String phrase = wheelStr();
        sendMsg(phrase);
        wheelIdx = (uint8_t)((wheelIdx + 1) % WHEEL_N); // advance to the next phrase
    }

    String sendMsg(const String& raw) {
        String msg = raw;
        msg.trim();
        if (msg.length() > MAX_MSG) msg = msg.substring(0, MAX_MSG);
        if (!msg.length()) return String();

        String lo = msg;
        lo.toLowerCase();

        String rep = brain(lo);

        logPush("you> " + msg);
        logPush("blaze> " + rep);

        prnt("[CHAT] "); // serial line
        prnt(msg);
        prnt(" -> ");
        prntln(rep);

        // OLED: show the exchange even when the user isn't in CHAT mode
        if (displayUI.mode != DISPLAY_MODE::CHAT &&
            displayUI.mode != DISPLAY_MODE::PANIC) {
            displayUI.flashMsg(msg + " > " + rep, 2500);
        }
        return rep;
    }

    // ===== drawing ===== //
    static void drawEyes(OLEDDisplay* d) {
        const int lx = 20, rx = 78, y = 4, w = 30, h = 14;

        d->setColor(OLEDDISPLAY_COLOR::WHITE);
        switch (BlazePersona::get()) {
            case BlazePersona::ST_HUNGRY:
                d->fillRect(lx, y, w, h);
                d->fillRect(rx, y, w, h);
                d->setColor(OLEDDISPLAY_COLOR::BLACK);
                d->fillRect(lx + 13, y + 2, 4, 10);
                d->fillRect(rx + 13, y + 2, 4, 10);
                break;

            case BlazePersona::ST_LOCKED:
                d->fillRect(lx, y, w, h);
                d->fillRect(rx, y, w, h);
                d->setColor(OLEDDISPLAY_COLOR::BLACK);
                d->drawHorizontalLine(lx, y + 7, w);
                d->drawVerticalLine(lx + 15, y, h);
                d->drawHorizontalLine(rx, y + 7, w);
                d->drawVerticalLine(rx + 15, y, h);
                break;

            case BlazePersona::ST_PROWL: // narrowed
                d->fillRect(lx, y + 3, w, 8);
                d->fillRect(rx, y + 3, w, 8);
                break;

            case BlazePersona::ST_UNIMPRESSED: // flat upper lids
                d->fillRect(lx, y, w, 6);
                d->fillRect(rx, y, w, 6);
                break;

            case BlazePersona::ST_STANDBY: // closed
                d->drawHorizontalLine(lx, y + 7, w);
                d->drawHorizontalLine(rx, y + 7, w);
                break;

            default: // IDLE: two plain blocks
                d->fillRect(lx, y, w, h);
                d->fillRect(rx, y, w, h);
                break;
        }
        d->setColor(OLEDDISPLAY_COLOR::WHITE);
    }

    void render(OLEDDisplay* d) {
        if (!d) return;
        d->clear();
        d->setColor(OLEDDISPLAY_COLOR::WHITE);
        d->setFont(ArialMT_Plain_10);
        d->setTextAlignment(TEXT_ALIGN_LEFT);

        drawEyes(d);

        // input line: highlighted wheel phrase
        d->drawString(2, 21, String("> ") + wheelStr());

        // latest exchange: one entry per line, long entries wrap over both
        String last = latest(0);
        String prev = latest(1);

        if (last.length() == 0) {
            d->drawString(2, 48, String("_"));
        } else if (d->getStringWidth(last) <= 124) {
            d->drawString(2, 34, last);
            if (prev.length()) d->drawString(2, 48, prev);
        } else {
            int mid = (int)last.length() / 2;
            int brk = last.lastIndexOf(' ', mid);
            if (brk <= 0) brk = last.indexOf(' ', mid);
            if (brk <= 0) brk = (int)last.length();
            d->drawString(2, 34, last.substring(0, brk));
            if (brk < (int)last.length())
                d->drawString(2, 48, last.substring(brk + 1));
        }
    }

    uint8_t logCount() { return logNum; }

    const char* logLine(uint8_t i) {
        if (i >= logNum) return "";
        uint8_t start = (uint8_t)((logHead + LOG_N - logNum) % LOG_N);
        uint8_t slot  = (uint8_t)((start + i) % LOG_N);
        return logBuf[slot].c_str();
    }
}
