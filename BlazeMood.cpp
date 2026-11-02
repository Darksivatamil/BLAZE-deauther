/* BLAZE - Team DARK
   Mood state machine driven by attacks / tracker / idle */

#include "BlazeMood.h"
#include "BlazeFace.h"

namespace BlazeMood {
    static Mood      mood          = MOOD_CALM;
    static uint32_t  moodStart     = 0;
    static uint32_t  holdUntil     = 0; // elevated mood expires after this

    void setMood(Mood m) {
        if (m == mood) return;
        mood      = m;
        moodStart = millis();
    }

    Mood get() { return mood; }

    const char* name() {
        switch (mood) {
            case MOOD_CURIOS: return "CURIOS";
            case MOOD_BRAVE:  return "BRAVE";
            case MOOD_SAD:    return "SAD";
            case MOOD_ALERT:  return "ALERT";
            case MOOD_SLEEP:  return "SLEEP";
            default:          return "CALM";
        }
    }

    void onAttackStart() {
        setMood(MOOD_BRAVE);
        holdUntil = millis() + 8000;
        BlazeFace::flashMessage("BRAVE!", 900);
    }

    void onAttackStop() {
        // brave fades back after the hold window
        if (holdUntil && millis() > holdUntil) setMood(MOOD_CALM);
    }

    void onScanDone() {
        setMood(MOOD_CURIOS);
        holdUntil = millis() + 5000;
    }

    void onTrackerLost() {
        setMood(MOOD_SAD);
        holdUntil = millis() + 6000;
    }

    void onTrackerFound() {
        setMood(MOOD_ALERT);
        holdUntil = millis() + 4000;
    }

    void setup() {
        mood      = MOOD_CALM;
        moodStart = millis();
        holdUntil = 0;
    }

    void update() {
        uint32_t now = millis();

        // hold window expiry -> step down to CALM
        if (holdUntil && now > holdUntil) {
            if (mood == MOOD_BRAVE || mood == MOOD_SAD || mood == MOOD_ALERT || mood == MOOD_CURIOS) {
                setMood(MOOD_CALM);
            }
            holdUntil = 0;
        }

        // idle -> SLEEP after 90s on CALM
        if (mood == MOOD_CALM && (now - moodStart > 90000)) {
            setMood(MOOD_SLEEP);
        }

        // wake from SLEEP on any new activity
        if (mood == MOOD_SLEEP && (holdUntil > now)) {
            setMood(MOOD_CALM);
        }

        // push mood into the face bias (BRAVE=+2, SAD=-1, ALERT=+1, SLEEP=-1, else 0)
        int8_t bias = 0;
        switch (mood) {
            case MOOD_BRAVE: bias = 2;  break;
            case MOOD_SAD:   bias = -1; break;
            case MOOD_ALERT: bias = 1;  break;
            case MOOD_SLEEP: bias = -1; break;
            case MOOD_CURIOS:bias = 0;  break;
            default: break;
        }
        BlazeFace::setMoodBias(bias);
    }
}
