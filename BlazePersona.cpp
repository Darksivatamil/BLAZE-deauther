/* BLAZE - Team DARK
   Persona engine - see BlazePersona.h */

#include "BlazePersona.h"

#include "language.h"
#include "DisplayUI.h"
#include "BlazeFace.h"

extern DisplayUI displayUI;

namespace BlazePersona {
    // ===== stance state ===== //
    static Stance   stance     = ST_IDLE;
    static uint32_t stanceAt   = 0;   // when current stance began
    static uint32_t holdUntil  = 0;   // elevated stance expires after this
    static int8_t   curBias    = 0;   // pushed into BlazeFace

    // ===== line cooldowns (per event, ms) ===== //
    static const uint32_t EV_COOLDOWN[EV_COUNT] = {
        0,      // EV_BOOT - fires once anyway
        6000,   // EV_ATTACK
        6000,   // EV_ATTACK_DONE
        8000,   // EV_SCAN_DONE
        15000,  // EV_EAPOL
        15000,  // EV_TWIN_START
        0,      // EV_TWIN_CREDS - jackpot, always say it
        10000,  // EV_LOGIN_FAIL
        12000,  // EV_TRACKER_LOST
        12000,  // EV_TRACKER_FOUND
        6000,   // EV_KILL
        0       // EV_PANIC - always
    };
    static uint32_t evLast[EV_COUNT];

    // ===== companion line tables (PROGMEM, 2 picks per event) ===== //
    static const uint8_t LINES_PER_EVENT = 2;
    static const char* const P_LINES[EV_COUNT][LINES_PER_EVENT] PROGMEM = {
        { "team DARK online.",     "let's hunt." },         // EV_BOOT
        { "let's eat.",            "targets acquired." },   // EV_ATTACK
        { "plate's clean.",        "more." },               // EV_ATTACK_DONE
        { "wolves circling.",      "sniffed out." },        // EV_SCAN_DONE
        { "handshake in the bag.", "gotcha." },             // EV_EAPOL
        { "spoon's out. open wide.", "door's open." },      // EV_TWIN_START
        { "keys handed over.",     "password. mine now." }, // EV_TWIN_CREDS
        { "wrong door.",           "try harder." },         // EV_LOGIN_FAIL
        { "ran off.",              "gone quiet." },         // EV_TRACKER_LOST
        { "back in my sights.",    "there you are." },      // EV_TRACKER_FOUND
        { "kill mode.",            "burn it down." },       // EV_KILL
        { "hands off the keyboard.", "everyone down." }     // EV_PANIC
    };

    // ===== voice log ring buffer (for /blaze/log) ===== //
    static const uint8_t LOG_N = 16;
    static String    logBuf[LOG_N];
    static uint8_t   logHead = 0; // next slot to write
    static uint8_t   logNum  = 0; // how many filled (<= LOG_N)

    // ===== helpers ===== //
    static int8_t biasFor(Stance s) {
        switch (s) {
            case ST_HUNGRY:      return +2;
            case ST_LOCKED:      return +2;
            case ST_PROWL:       return +1;
            case ST_UNIMPRESSED: return -1;
            default:             return 0;
        }
    }

    static void applyStance(Stance s, uint32_t holdMs) {
        if (stance == ST_STANDBY && s != ST_STANDBY) return; // screen off wins
        if (s != stance) {
            stance   = s;
            stanceAt = millis();
        }
        holdUntil = holdMs ? (millis() + holdMs) : 0;
        int8_t b = biasFor(s);
        if (b != curBias) {
            curBias = b;
            biasFace(b);
        }
    }

    static String linePick(Ev e) {
        uint8_t i = (uint8_t)random(LINES_PER_EVENT);
        return String(FPSTR((const char*)pgm_read_ptr(&P_LINES[e][i])));
    }

    static void logPush(const String& s) {
        logBuf[logHead] = s;
        logHead = (uint8_t)((logHead + 1) % LOG_N);
        if (logNum < LOG_N) logNum++;
    }

    void biasFace(int8_t b) { BlazeFace::setBias(b); }

    // ===== public API ===== //
    void setup() {
        stance    = ST_IDLE;
        stanceAt  = millis();
        holdUntil = 0;
        curBias   = 0;
        logHead   = 0;
        logNum    = 0;
        for (uint8_t i = 0; i < EV_COUNT; i++) evLast[i] = 0;
    }

    void update() {
        if (stance == ST_IDLE || stance == ST_STANDBY) return;
        if (!holdUntil) return;
        if ((int32_t)(millis() - holdUntil) < 0) return; // still holding

        // hold expired -> ease back to baseline
        applyStance(ST_IDLE, 0);
    }

    void poke(Stance s, uint32_t holdMs) {
        if (stance == ST_STANDBY) return;
        if (stance == s) {
            holdUntil = millis() + holdMs;
            return;
        }
        // assert from baseline, or HUNGRY takes over a light PROWL
        if (stance == ST_IDLE || (s == ST_HUNGRY && stance == ST_PROWL))
            applyStance(s, holdMs);
    }

    void onStandby(bool sb) {
        if (sb) {
            if (stance == ST_STANDBY) return;
            stance    = ST_STANDBY;
            stanceAt  = millis();
            holdUntil = 0;
            if (curBias) {
                curBias = 0;
                biasFace(0);
            }
        } else if (stance == ST_STANDBY) {
            stance    = ST_IDLE;
            stanceAt  = millis();
            holdUntil = 0;
        }
    }

    void flash(const String& txt, uint32_t ms) {
        if (!BlazeFace::bootDone()) return;        // never cut the boot animation
        if (displayUI.asleep()) return;            // screen off stays off
        if (displayUI.mode == DISPLAY_MODE::PANIC) return; // panic screen wins
        displayUI.flashMsg(txt, ms);
    }

    Stance get() { return stance; }

    const char* stanceName() {
        switch (stance) {
            case ST_PROWL:       return "PROWL";
            case ST_HUNGRY:      return "HUNGRY";
            case ST_UNIMPRESSED: return "UNIMPRESSED";
            case ST_LOCKED:      return "LOCKED";
            case ST_STANDBY:     return "STANDBY";
            default:             return "IDLE";
        }
    }

    uint8_t logCount() { return logNum; }

    const char* logLine(uint8_t i) {
        if (i >= logNum) return "";
        uint8_t start = (uint8_t)((logHead + LOG_N - logNum) % LOG_N);
        uint8_t slot  = (uint8_t)((start + i) % LOG_N);
        return logBuf[slot].c_str();
    }

    void onEvent(Ev e) {
        uint32_t now = millis();

        // stance per event (hold unless noted)
        switch (e) {
            case EV_ATTACK:
            case EV_KILL:        applyStance(ST_HUNGRY, 8000); break;
            case EV_ATTACK_DONE: applyStance(ST_IDLE, 0); break; // feeding over
            case EV_SCAN_DONE:
            case EV_TRACKER_FOUND: applyStance(ST_PROWL, 8000); break;
            case EV_EAPOL:       applyStance(ST_LOCKED, 20000); break;
            case EV_LOGIN_FAIL:
            case EV_TRACKER_LOST: applyStance(ST_UNIMPRESSED, 8000); break;
            case EV_TWIN_START:  applyStance(ST_HUNGRY, 8000); break;
            case EV_PANIC:       applyStance(ST_IDLE, 0); break;
            default: break; // EV_BOOT / EV_TWIN_CREDS: line only
        }

        // line (cooldown, with always-say exceptions)
        bool always = (e == EV_BOOT || e == EV_TWIN_CREDS || e == EV_PANIC);
        if (!always && evLast[e] && (now - evLast[e]) < EV_COOLDOWN[e]) return;
        evLast[e] = now;

        String line = linePick(e);
        prnt("[BLAZE] "); // serial voice
        prntln(line);
        logPush(line);
        flash(line, 2000); // OLED voice (skipped when display off / panic / booting)
    }
}
