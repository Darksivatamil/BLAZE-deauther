/* BLAZE - Team DARK
   Loading screen + 26 anime face reactions + startup greeting
   Ported from NEW/blaze_face_features_ori.ino
   (adapted to the deauther ThingPulse OLED library, made non-blocking) */

#include "BlazeFace.h"

#include <Arduino.h>

#include "BlazeCfg.h"

// =====================================================
// DISPLAY ADAPTER (Adafruit-style helpers -> ThingPulse)
// =====================================================

static OLEDDisplay* d = NULL;

#define BF_WHITE OLEDDISPLAY_COLOR::WHITE
#define BF_BLACK OLEDDISPLAY_COLOR::BLACK

static void bfFillRoundRect(int x, int y, int w, int h, int r, OLEDDISPLAY_COLOR c) {
    if (!d) return;
    if (w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    int maxR = (w < h ? w : h) / 2;
    if (r > maxR) r = maxR;

    d->setColor(c);
    if (r == 0) {
        d->fillRect(x, y, w, h);
        return;
    }
    // center bars
    d->fillRect(x + r, y, w - 2 * r, h);
    d->fillRect(x, y + r, w, h - 2 * r);
    // corners
    d->fillCircle(x + r, y + r, r);
    d->fillCircle(x + w - r - 1, y + r, r);
    d->fillCircle(x + r, y + h - r - 1, r);
    d->fillCircle(x + w - r - 1, y + h - r - 1, r);
}

static void bfDrawRoundRect(int x, int y, int w, int h, int r, OLEDDISPLAY_COLOR c) {
    if (!d) return;
    if (w <= 1 || h <= 1) return;
    if (r < 0) r = 0;
    int maxR = (w < h ? w : h) / 2;
    if (r > maxR) r = maxR;

    d->setColor(c);
    if (r == 0) {
        d->drawRect(x, y, w, h);
        return;
    }
    d->drawHorizontalLine(x + r, y, w - 2 * r);
    d->drawHorizontalLine(x + r, y + h - 1, w - 2 * r);
    d->drawVerticalLine(x, y + r, h - 2 * r);
    d->drawVerticalLine(x + w - 1, y + r, h - 2 * r);
    // corner arcs (quarter circles via pixels)
    for (int i = 0; i <= r; i++) {
        int dy = (int)(sqrt((double)r * r - (double)i * i) + 0.5);
        d->setPixel(x + r - i, y + r - dy);
        d->setPixel(x + w - r - 1 + i, y + r - dy);
        d->setPixel(x + r - i, y + h - r - 1 + dy);
        d->setPixel(x + w - r - 1 + i, y + h - r - 1 + dy);
        d->setPixel(x + r - dy, y + r - i);
        d->setPixel(x + w - r - 1 + dy, y + r - i);
        d->setPixel(x + r - dy, y + h - r - 1 + i);
        d->setPixel(x + w - r - 1 + dy, y + h - r - 1 + i);
    }
}

static void bfFillRect(int x, int y, int w, int h, OLEDDISPLAY_COLOR c) {
    if (!d) return;
    d->setColor(c);
    d->fillRect(x, y, w, h);
}

static void bfFillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, OLEDDISPLAY_COLOR c) {
    if (!d) return;
    d->setColor(c);

    // sort by y
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }

    if (y2 == y0) {
        int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
        int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
        d->fillRect(minx, y0, maxx - minx + 1, 1);
        return;
    }

    double d01 = (y1 == y0) ? 0 : (double)(x1 - x0) / (double)(y1 - y0);
    double d02 = (y2 == y0) ? 0 : (double)(x2 - x0) / (double)(y2 - y0);
    double d12 = (y2 == y1) ? 0 : (double)(x2 - x1) / (double)(y2 - y1);

    for (int y = y0; y <= y2; y++) {
        double xa, xb;
        if (y < y1) {
            xa = x0 + d01 * (y - y0);
            xb = x0 + d02 * (y - y0);
        } else {
            xa = x1 + d12 * (y - y1);
            xb = x0 + d02 * (y - y0);
        }
        if (xa > xb) { double t = xa; xa = xb; xb = t; }
        int xA = (int)(xa + 0.5);
        int xB = (int)(xb + 0.5);
        d->fillRect(xA, y, xB - xA + 1, 1);
    }
}

// =====================================================
// EYE SETTINGS  (from NEW/blaze_face_features_ori.ino)
// =====================================================

const int LEFT_X   = 20;
const int RIGHT_X  = 78;
const int NORMAL_Y = 24;

const int EYE_W = 30;
const int EYE_H = 17;
const int EYE_R = 5;

// =====================================================
// REACTIONS (26, ported 1:1 from the reference .ino)
// =====================================================

enum Reaction {
    NORMAL = 0, HAPPY, BLINK, LOOK_LEFT, LOOK_RIGHT, SLEEPY,
    SURPRISED, ANGRY, SAD, LOVE,
    EXCITED, VERY_HAPPY, SUSPICIOUS, SHY, BORED, THINKING,
    WORRIED, FRUSTRATED, CRYING, LAUGHING, CURIOUS, WINK,
    DOUBTFUL, PROUD, DETERMINED, RELAXED
};

const uint8_t TOTAL_REACTIONS = 26;

// short display names per reaction (face label + WEIGHTS menu, plan 3.2)
const char* const REACTION_NAMES[TOTAL_REACTIONS] = {
    "NORMAL", "HAPPY", "BLINK", "LOOK L", "LOOK R", "SLEEPY",
    "SURPRISED", "ANGRY", "SAD", "LOVE",
    "EXCITED", "VERY HAPPY", "SUSPICIOUS", "SHY", "BORED", "THINKING",
    "WORRIED", "FRUSTRATED", "CRYING", "LAUGHING", "CURIOUS", "WINK",
    "DOUBTFUL", "PROUD", "DETERMINED", "RELAXED"
};

// default reaction weights (plan 3.2: runtime-editable, stored in BlazeCfg)
const uint8_t WEIGHT_DEFAULTS[TOTAL_REACTIONS] = {
    14, 10, 12, 6, 6, 5, // NORMAL HAPPY BLINK LOOK_L LOOK_R SLEEPY
    5, 4, 4, 4,          // SURPRISED ANGRY SAD LOVE
    5, 4, 3, 3, 3, 3,    // EXCITED VERY_HAPPY SUSPICIOUS SHY BORED THINKING
    3, 2, 2, 3, 3, 3,    // WORRIED FRUSTRATED CRYING LAUGHING CURIOUS WINK
    2, 2, 3, 4           // DOUBTFUL PROUD DETERMINED RELAXED
};

// =====================================================
// STATE
// =====================================================

namespace BlazeFace {
    // face mode
    uint8_t currentReaction   = NORMAL;
    bool    blinking          = false;
    uint32_t reactionStarted  = 0;
    uint32_t reactionDuration = 0;
    uint32_t nextBlinkTime    = 0;
    uint32_t blinkStarted     = 0;

    // boot phases: loading sparks -> greeting -> wake-up -> face
    enum Phase : uint8_t { PHASE_LOADING, PHASE_WAKE, PHASE_GREETING, PHASE_WAKETAIL, PHASE_FACE };
    Phase    phase        = PHASE_FACE;
    uint32_t phaseStart   = 0;

    // wake-up animation frames
    struct Frame {
        uint8_t  type;   // 0=reaction(a), 1=custom eyes(a,b,c,d), 2=greeting screen
        int      a, b, c, dd;
        uint32_t dur;
    };

    // part 1: sleepy -> eyes open -> look -> surprise -> blink -> stretch -> normal
    const Frame WAKE_FRAMES[] = {
        { 0, SLEEPY,     0, 0, 0, 600  },
        { 1, 30, 7, 30, 7,      180  }, // eyes slowly open
        { 1, 30, 10, 30, 10,    180  },
        { 1, 30, 17, 30, 5,     500  }, // one eye first
        { 1, 30, 17, 30, 12,    180  }, // second eye
        { 0, NORMAL,     0, 0, 0, 450  },
        { 0, LOOK_LEFT,  0, 0, 0, 450  },
        { 0, LOOK_RIGHT, 0, 0, 0, 450  },
        { 0, NORMAL,     0, 0, 0, 300  },
        { 0, SURPRISED,  0, 0, 0, 600  },
        { 0, NORMAL,     0, 0, 0, 250  },
        { 0, BLINK,      0, 0, 0, 120  },
        { 0, NORMAL,     0, 0, 0, 160  },
        { 0, BLINK,      0, 0, 0, 120  },
        { 0, NORMAL,     0, 0, 0, 300  },
        { 1, 8, 8, 8, 8,        100  }, // stretching eyes
        { 1, 16, 12, 16, 12,    100  },
        { 1, 25, 16, 25, 16,    100  },
        { 1, 36, 25, 36, 25,    180  },
        { 0, NORMAL,     0, 0, 0, 350  },
    };
    const uint8_t WAKE_FRAMES_N = sizeof(WAKE_FRAMES) / sizeof(WAKE_FRAMES[0]);

    // part 2 (right after the greeting): surprised -> normal, then the face loop begins
    const Frame WAKE_TAIL[] = {
        { 0, SURPRISED,  0, 0, 0, 350  },
        { 0, NORMAL,     0, 0, 0, 600  },
    };
    const uint8_t WAKE_TAIL_N = sizeof(WAKE_TAIL) / sizeof(WAKE_TAIL[0]);
    uint8_t wakeIdx = 0;

    // greeting sub-phase (root@blaze boot -> BLAZE -> here_)
    uint8_t greetStep = 0;

    // flash message
    String   flashText;
    uint32_t flashUntil = 0;

    // mood bias: filters which reactions are likely
    int8_t   moodBiasVal = 0;

    // runtime reaction weights (plan 3.2) - persisted in BlazeCfg "face.weights"
    uint8_t weights[TOTAL_REACTIONS];
    bool    weightsReady = false;

    void ensureWeights() {
        if (weightsReady) return;
        memcpy(weights, WEIGHT_DEFAULTS, TOTAL_REACTIONS);
        weightsReady = true;
    }

    // ===== helpers ===== //

    // weighted reaction picker (common reactions are more frequent)
    void pickReaction() {
        ensureWeights();

        int total = 0;
        for (uint8_t i = 0; i < TOTAL_REACTIONS; i++) {
            int w = weights[i];
            // mood filter: BRAVE favours bold faces, SAD favours low-energy faces
            if (moodBiasVal >= 2) {
                if (i == SAD || i == SLEEPY || i == BORED || i == CRYING) w = 0;
            } else if (moodBiasVal <= -1) {
                if (i == ANGRY || i == EXCITED || i == LOVE || i == PROUD || i == DETERMINED) w = 0;
            }
            total += w;
        }

        if (total <= 0) { currentReaction = NORMAL; return; }

        int r = random(total);
        int acc = 0;
        for (uint8_t i = 0; i < TOTAL_REACTIONS; i++) {
            int w = weights[i];
            if (moodBiasVal >= 2) {
                if (i == SAD || i == SLEEPY || i == BORED || i == CRYING) w = 0;
            } else if (moodBiasVal <= -1) {
                if (i == ANGRY || i == EXCITED || i == LOVE || i == PROUD || i == DETERMINED) w = 0;
            }
            acc += w;
            if (r < acc) { currentReaction = i; return; }
        }
        currentReaction = NORMAL;
    }

    void startReaction(uint8_t reaction) {
        currentReaction  = reaction;
        reactionStarted  = millis();
        reactionDuration = random(3000UL, 10001UL);   // stays 3 - 10 seconds
        nextBlinkTime    = millis() + random(1500UL, 4501UL);
        blinking         = false;
    }

    // ===== drawing primitives (ported 1:1 from NEW/blaze_face_features_ori.ino) ===== //

    void eye(int x, int y, int w, int h, int radius) {
        bfFillRoundRect(x, y, w, h, radius, BF_WHITE);
    }

    void normalEyes() {
        eye(LEFT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
        eye(RIGHT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
    }

    void happyEyes() {
        eye(LEFT_X, 28, 30, 13, 5);
        eye(RIGHT_X, 28, 30, 13, 5);
    }

    void blinkEyes() {
        bfFillRoundRect(LEFT_X, 31, EYE_W, 4, 2, BF_WHITE);
        bfFillRoundRect(RIGHT_X, 31, EYE_W, 4, 2, BF_WHITE);
    }

    void lookLeft() {
        eye(LEFT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
        eye(RIGHT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
        bfFillRect(LEFT_X + 16, NORMAL_Y, 14, EYE_H, BF_BLACK);
        bfFillRect(RIGHT_X + 16, NORMAL_Y, 14, EYE_H, BF_BLACK);
    }

    void lookRight() {
        eye(LEFT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
        eye(RIGHT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
        bfFillRect(LEFT_X, NORMAL_Y, 14, EYE_H, BF_BLACK);
        bfFillRect(RIGHT_X, NORMAL_Y, 14, EYE_H, BF_BLACK);
    }

    void sleepyEyes() {
        bfFillRoundRect(LEFT_X, 31, EYE_W, 5, 2, BF_WHITE);
        bfFillRoundRect(RIGHT_X, 31, EYE_W, 5, 2, BF_WHITE);
    }

    void surprisedEyes() {
        eye(17, 18, 35, 28, 9);
        eye(76, 18, 35, 28, 9);
    }

    void angryEyes() {
        normalEyes();
        bfFillTriangle(LEFT_X, NORMAL_Y, LEFT_X + EYE_W, NORMAL_Y, LEFT_X, NORMAL_Y + 8, BF_BLACK);
        bfFillTriangle(RIGHT_X, NORMAL_Y, RIGHT_X + EYE_W, NORMAL_Y, RIGHT_X + EYE_W, NORMAL_Y + 8, BF_BLACK);
    }

    void sadEyes() {
        eye(LEFT_X, 29, 30, 13, 5);
        eye(RIGHT_X, 29, 30, 13, 5);
        bfFillRect(LEFT_X, 29, 30, 4, BF_BLACK);
        bfFillRect(RIGHT_X, 29, 30, 4, BF_BLACK);
    }

    void drawHeart(int cx, int cy) {
        d->setColor(BF_BLACK);
        d->fillCircle(cx - 4, cy - 3, 4);
        d->fillCircle(cx + 4, cy - 3, 4);
        bfFillTriangle(cx - 8, cy - 1, cx + 8, cy - 1, cx, cy + 8, BF_BLACK);
    }

    void loveEyes() {
        normalEyes();
        drawHeart(35, 30);
        drawHeart(93, 30);
    }

    // ---- extended reactions (20 more, from the reference .ino) ---- //

    void excitedEyes() {
        eye(LEFT_X, 20, 30, 20, 6);
        eye(RIGHT_X, 20, 30, 20, 6);
        // sparkle pixels
        d->setColor(BF_BLACK);
        d->setPixel(LEFT_X + 8, 26);
        d->setPixel(RIGHT_X + 8, 26);
    }

    void veryHappyEyes() {
        // big upward arcs (filled crescents)
        bfFillRoundRect(LEFT_X, 26, 30, 12, 6, BF_WHITE);
        bfFillRoundRect(RIGHT_X, 26, 30, 12, 6, BF_WHITE);
        bfFillRect(LEFT_X, 32, 30, 6, BF_BLACK);
        bfFillRect(RIGHT_X, 32, 30, 6, BF_BLACK);
    }

    void suspiciousEyes() {
        // half-lidded, shifted gaze
        eye(LEFT_X, 26, 30, 12, 4);
        eye(RIGHT_X, 22, 30, 17, 5);
        bfFillRect(LEFT_X + 14, 26, 16, 12, BF_BLACK);
        bfFillRect(RIGHT_X + 14, 22, 16, 17, BF_BLACK);
    }

    void shyEyes() {
        // small averted eyes + blush dots
        eye(LEFT_X + 2, 27, 24, 12, 5);
        eye(RIGHT_X + 2, 27, 24, 12, 5);
        bfFillRect(LEFT_X + 12, 27, 12, 12, BF_BLACK);
        bfFillRect(RIGHT_X + 12, 27, 12, 12, BF_BLACK);
        d->setColor(BF_WHITE);
        d->setPixel(14, 40);
        d->setPixel(15, 40);
        d->setPixel(112, 40);
        d->setPixel(113, 40);
    }

    void boredEyes() {
        // flat half-lids
        bfFillRoundRect(LEFT_X, 24, EYE_W, 8, 3, BF_WHITE);
        bfFillRoundRect(RIGHT_X, 24, EYE_W, 8, 3, BF_WHITE);
        bfFillRect(LEFT_X, 24, EYE_W, 3, BF_BLACK);
        bfFillRect(RIGHT_X, 24, EYE_W, 3, BF_BLACK);
    }

    void thinkingEyes() {
        // eyes look up-left
        eye(LEFT_X, 22, 30, 14, 5);
        eye(RIGHT_X, 22, 30, 14, 5);
        bfFillRect(LEFT_X + 8, 30, 22, 6, BF_BLACK);
        bfFillRect(RIGHT_X + 8, 30, 22, 6, BF_BLACK);
        bfFillRect(LEFT_X, 22, 30, 4, BF_BLACK);
        bfFillRect(RIGHT_X, 22, 30, 4, BF_BLACK);
    }

    void worriedEyes() {
        // wide eyes, inner brows tilted down
        eye(LEFT_X, 20, 30, 16, 5);
        eye(RIGHT_X, 20, 30, 16, 5);
        bfFillTriangle(LEFT_X, 20, LEFT_X + 10, 20, LEFT_X, 27, BF_BLACK);
        bfFillTriangle(RIGHT_X + EYE_W, 20, RIGHT_X + EYE_W - 10, 20, RIGHT_X + EYE_W, 27, BF_BLACK);
    }

    void frustratedEyes() {
        // angry + squeezed
        eye(LEFT_X, 26, 30, 10, 4);
        eye(RIGHT_X, 26, 30, 10, 4);
        bfFillTriangle(LEFT_X, 26, LEFT_X + EYE_W, 26, LEFT_X, 32, BF_BLACK);
        bfFillTriangle(RIGHT_X, 26, RIGHT_X + EYE_W, 26, RIGHT_X + EYE_W, 32, BF_BLACK);
    }

    void cryingEyes() {
        sadEyes();
        // tear streams
        d->setColor(BF_WHITE);
        d->drawVerticalLine(LEFT_X + 6, 44, 10);
        d->drawVerticalLine(RIGHT_X + 6, 44, 10);
        d->drawVerticalLine(LEFT_X + 7, 46, 8);
        d->drawVerticalLine(RIGHT_X + 7, 46, 8);
    }

    void laughingEyes() {
        // closed happy arcs (^ ^)
        bfFillRoundRect(LEFT_X, 28, 30, 8, 4, BF_WHITE);
        bfFillRoundRect(RIGHT_X, 28, 30, 8, 4, BF_WHITE);
        bfFillRect(LEFT_X, 32, 30, 4, BF_BLACK);
        bfFillRect(RIGHT_X, 32, 30, 4, BF_BLACK);
    }

    void curiousEyes() {
        // one eye bigger
        eye(LEFT_X, 20, 30, 20, 6);
        eye(RIGHT_X + 4, 27, 22, 11, 5);
        bfFillRect(LEFT_X + 10, 20, 20, 20, BF_BLACK);
        bfFillRect(RIGHT_X + 12, 27, 14, 11, BF_BLACK);
    }

    void winkEyes() {
        normalEyes();
        // left eye stays open, right eye winks closed
        bfFillRoundRect(RIGHT_X, 30, EYE_W, 5, 2, BF_WHITE);
        bfFillRect(RIGHT_X, 24, EYE_W, 6, BF_BLACK);
    }

    void doubtfulEyes() {
        // asymmetric lids
        eye(LEFT_X, 24, 30, 12, 5);
        eye(RIGHT_X, 22, 30, 16, 5);
        bfFillRect(LEFT_X, 24, EYE_W, 4, BF_BLACK);
        bfFillRect(RIGHT_X + 12, 22, 18, 16, BF_BLACK);
    }

    void proudEyes() {
        // content half-closed
        bfFillRoundRect(LEFT_X, 26, EYE_W, 11, 5, BF_WHITE);
        bfFillRoundRect(RIGHT_X, 26, EYE_W, 11, 5, BF_WHITE);
        bfFillRect(LEFT_X, 26, EYE_W, 4, BF_BLACK);
        bfFillRect(RIGHT_X, 26, EYE_W, 4, BF_BLACK);
    }

    void determinedEyes() {
        // sharp narrowed eyes
        eye(LEFT_X, 25, 30, 12, 4);
        eye(RIGHT_X, 25, 30, 12, 4);
        bfFillTriangle(LEFT_X, 25, LEFT_X + EYE_W, 25, LEFT_X, 31, BF_BLACK);
        bfFillTriangle(RIGHT_X, 25, RIGHT_X + EYE_W, 25, RIGHT_X + EYE_W, 31, BF_BLACK);
    }

    void relaxedEyes() {
        // soft droopy lids
        bfFillRoundRect(LEFT_X, 27, EYE_W, 10, 4, BF_WHITE);
        bfFillRoundRect(RIGHT_X, 27, EYE_W, 10, 4, BF_WHITE);
        bfFillRect(LEFT_X, 27, EYE_W, 3, BF_BLACK);
        bfFillRect(RIGHT_X, 27, EYE_W, 3, BF_BLACK);
    }

    // ---- reaction label (small text, bottom-right corner) ---- //

    void drawReactionLabel(uint8_t reaction) {
        if (!d) return;
        const char* label = "";
        switch (reaction) {
            case NORMAL:     label = "normal";     break;
            case HAPPY:      label = "happy";      break;
            case BLINK:      label = "blink";      break;
            case LOOK_LEFT:  label = "lookL";      break;
            case LOOK_RIGHT: label = "lookR";      break;
            case SLEEPY:     label = "sleepy";     break;
            case SURPRISED:  label = "surprised";  break;
            case ANGRY:      label = "angry";      break;
            case SAD:        label = "sad";        break;
            case LOVE:       label = "love";       break;
            case EXCITED:    label = "excited";    break;
            case VERY_HAPPY: label = "veryhappy";  break;
            case SUSPICIOUS: label = "suspicious"; break;
            case SHY:        label = "shy";        break;
            case BORED:      label = "bored";      break;
            case THINKING:   label = "thinking";   break;
            case WORRIED:    label = "worried";    break;
            case FRUSTRATED: label = "frustrated"; break;
            case CRYING:     label = "crying";     break;
            case LAUGHING:   label = "laughing";   break;
            case CURIOUS:    label = "curious";    break;
            case WINK:       label = "wink";       break;
            case DOUBTFUL:   label = "doubtful";   break;
            case PROUD:      label = "proud";      break;
            case DETERMINED: label = "determined"; break;
            case RELAXED:    label = "relaxed";    break;
            default: break;
        }
        d->setColor(BF_WHITE);
        d->setFont(ArialMT_Plain_10);
        d->setTextAlignment(TEXT_ALIGN_RIGHT);
        d->drawString(126, 54, label);
        d->setTextAlignment(TEXT_ALIGN_LEFT);
    }

    void drawReaction(uint8_t reaction) {
        if (!d) return;
        d->clear();

        switch (reaction) {
            case NORMAL:     normalEyes();      break;
            case HAPPY:      happyEyes();       break;
            case BLINK:      blinkEyes();       break;
            case LOOK_LEFT:  lookLeft();        break;
            case LOOK_RIGHT: lookRight();       break;
            case SLEEPY:     sleepyEyes();      break;
            case SURPRISED:  surprisedEyes();   break;
            case ANGRY:      angryEyes();       break;
            case SAD:        sadEyes();         break;
            case LOVE:       loveEyes();        break;
            case EXCITED:    excitedEyes();     break;
            case VERY_HAPPY: veryHappyEyes();   break;
            case SUSPICIOUS: suspiciousEyes();  break;
            case SHY:        shyEyes();         break;
            case BORED:      boredEyes();       break;
            case THINKING:   thinkingEyes();    break;
            case WORRIED:    worriedEyes();     break;
            case FRUSTRATED: frustratedEyes();  break;
            case CRYING:     cryingEyes();      break;
            case LAUGHING:   laughingEyes();    break;
            case CURIOUS:    curiousEyes();     break;
            case WINK:       winkEyes();        break;
            case DOUBTFUL:   doubtfulEyes();    break;
            case PROUD:      proudEyes();       break;
            case DETERMINED: determinedEyes();  break;
            case RELAXED:    relaxedEyes();     break;
            default:         normalEyes();      break;
        }
    }

    void drawCustomEyes(int leftW, int leftH, int rightW, int rightH) {
        if (!d) return;
        d->clear();

        int leftX  = 35 - (leftW / 2);
        int rightX = 93 - (rightW / 2);
        int leftY  = 32 - (leftH / 2);
        int rightY = 32 - (rightH / 2);

        bfFillRoundRect(leftX, leftY, leftW, leftH, leftH / 2 < 5 ? leftH / 2 : 5, BF_WHITE);
        bfFillRoundRect(rightX, rightY, rightW, rightH, rightH / 2 < 5 ? rightH / 2 : 5, BF_WHITE);
    }

    // ===== STARTUP GREETING (from blazeGreetingAnimation) ===== //
    // step 0: "root@blaze:~$ boot" terminal line
    // step 1: big "BLAZE" wordmark
    // step 2: "here_" cursor prompt
    void drawGreeting() {
        if (!d) return;
        d->clear();
        d->setColor(BF_WHITE);

        uint32_t elapsed = millis() - phaseStart;

        if (greetStep == 0) {
            // terminal boot line, typed in
            d->setFont(ArialMT_Plain_10);
            d->setTextAlignment(TEXT_ALIGN_LEFT);
            String line = "root@blaze:~$ ";
            int shown = (int)((elapsed / 60));       // ~60ms per char
            if (shown > 4) shown = 4;                // "boot"
            line += String("boot").substring(0, shown);
            if ((millis() / 300) % 2) line += "_";
            d->drawString(2, 24, line);
        } else if (greetStep == 1) {
            // BLAZE wordmark
            d->setFont(ArialMT_Plain_24);
            d->setTextAlignment(TEXT_ALIGN_CENTER);
            d->drawString(64, 16, "BLAZE");
            d->setTextAlignment(TEXT_ALIGN_LEFT);
        } else {
            // here_ prompt
            d->setFont(ArialMT_Plain_16);
            d->setTextAlignment(TEXT_ALIGN_CENTER);
            String t = "here_";
            if ((millis() / 400) % 2) t = "here|";
            d->drawString(64, 22, t);
            d->setTextAlignment(TEXT_ALIGN_LEFT);
        }
    }

    // ===== LOADING SCREEN (orbiting sparks + scan line + progress) ===== //
    void renderLoading() {
        if (!d) return;

        uint32_t elapsed = millis() - phaseStart;
        if (elapsed > 4000) elapsed = 4000;

        int progress = map(elapsed, 0, 4000, 0, 100);
        int barWidth = map(progress, 0, 100, 0, 94);

        d->clear();

        // Top robot box
        bfDrawRoundRect(24, 2, 80, 16, 4, BF_WHITE);
        d->setColor(BF_WHITE);
        d->setFont(ArialMT_Plain_10);
        d->setTextAlignment(TEXT_ALIGN_CENTER);
        d->drawString(64, 5, "BLAZE");

        // ---- orbiting sparks around the box ---- //
        {
            float t = (float)(millis() % 2000) / 2000.0f * 2.0f * PI;
            const int cx = 64, cy = 10;
            const int rx = 52, ry = 12;
            for (uint8_t s = 0; s < 3; s++) {
                float a = t + s * (2.0f * PI / 3.0f);
                int x = cx + (int)(cos(a) * rx);
                int y = cy + (int)(sin(a) * ry);
                d->setColor(BF_WHITE);
                d->setPixel(x, y);
                d->setPixel(x + 1, y);
                d->setPixel(x, y + 1);
            }
        }

        // INITIALIZING - hacker typing reveal
        {
            int n = (elapsed - 500) / 150; // 0 until 500ms, done at ~2450ms
            if (n < 0) n = 0;
            if (n > 13) n = 13;
            String txt = String("INITIALIZING").substring(0, n);
            if (n < 13 && (millis() / 300) % 2) txt += "_";
            d->setTextAlignment(TEXT_ALIGN_LEFT);
            d->drawString(25, 24, txt);
            d->setTextAlignment(TEXT_ALIGN_CENTER);
        }

        // ---- moving scan line across the middle strip ---- //
        {
            int sy = 38;
            int sx = (int)((millis() / 25) % 128);
            d->setColor(BF_WHITE);
            d->drawVerticalLine(sx, sy - 3, 7);
            d->drawVerticalLine((sx + 64) % 128, sy - 3, 7);
        }

        // Progress border
        bfDrawRoundRect(15, 46, 98, 12, 3, BF_WHITE);

        // Progress fill
        if (barWidth > 0) bfFillRoundRect(17, 48, barWidth, 8, 2, BF_WHITE);

        d->setTextAlignment(TEXT_ALIGN_LEFT);
    }

    // ===== PUBLIC ===== //

    void setDisplay(OLEDDisplay* display) {
        d = display;
    }

    void beginLoading() {
        phase      = PHASE_LOADING;
        phaseStart = millis();
        wakeIdx    = 0;
        greetStep  = 0;
    }

    bool bootDone() {
        return phase == PHASE_FACE;
    }

    void startFace() {
        phase = PHASE_FACE;
        startReaction(random(TOTAL_REACTIONS));
    }

    void setMoodBias(int8_t bias) { moodBiasVal = bias; }
    int8_t moodBias() { return moodBiasVal; }

    uint8_t reaction() { return currentReaction; }

    // ===== reaction weights editor (plan 3.2) ===== //
    // click = +1 (wraps 20 -> 0); persisted in BlazeCfg "face.weights"
    uint8_t getWeight(uint8_t idx) {
        if (idx >= TOTAL_REACTIONS) return 0;
        ensureWeights();
        return weights[idx];
    }

    void setWeight(uint8_t idx, uint8_t w) {
        if (idx >= TOTAL_REACTIONS) return;
        ensureWeights();
        weights[idx] = (w > 20) ? 20 : w;
    }

    void bumpWeight(uint8_t idx) {
        if (idx >= TOTAL_REACTIONS) return;
        ensureWeights();
        weights[idx] = (weights[idx] >= 20) ? 0 : weights[idx] + 1;
    }

    void resetWeights() {
        memcpy(weights, WEIGHT_DEFAULTS, TOTAL_REACTIONS);
        weightsReady = true;
    }

    uint8_t weightCount() { return TOTAL_REACTIONS; }

    void saveWeights() {
        ensureWeights();
        String csv;
        csv.reserve(TOTAL_REACTIONS * 3);
        for (uint8_t i = 0; i < TOTAL_REACTIONS; i++) {
            if (i) csv += ",";
            csv += String(weights[i]);
        }
        BlazeCfg::set("face.weights", csv);
    }

    void loadWeights() {
        String csv = BlazeCfg::get("face.weights", "");
        if (csv.length() == 0) { // no saved weights yet -> defaults
            resetWeights();
            return;
        }

        uint8_t tmp[TOTAL_REACTIONS];
        memcpy(tmp, WEIGHT_DEFAULTS, TOTAL_REACTIONS);

        uint8_t idx = 0;
        int     pos = 0;
        while (idx < TOTAL_REACTIONS && pos < (int)csv.length()) {
            int comma = csv.indexOf(',', pos);
            if (comma < 0) comma = csv.length();
            int v = csv.substring(pos, comma).toInt();
            if (v < 0) v = 0;
            if (v > 20) v = 20;
            tmp[idx++] = (uint8_t)v;
            pos = comma + 1;
        }
        memcpy(weights, tmp, TOTAL_REACTIONS);
        weightsReady = true;
    }

    const char* reactionName(uint8_t idx) {
        if (idx >= TOTAL_REACTIONS) return REACTION_NAMES[0];
        return REACTION_NAMES[idx];
    }

    const char* reactionName() {
        return reactionName(currentReaction);
    }

    void flashMessage(const String& txt, uint32_t ms) {
        flashText  = txt;
        flashUntil = millis() + ms;
    }

    bool flashActive() {
        return flashUntil > millis();
    }

    void update() {
        uint32_t now = millis();

        // ----- BOOT PHASES (loading -> wake -> greeting -> wake-tail -> face) -----
        if (phase == PHASE_LOADING) {
            if (now - phaseStart >= 4000) {
                phase       = PHASE_WAKE;   // wake-up eyes first
                phaseStart  = now;
                wakeIdx     = 0;
            }
            return;
        }

        if (phase == PHASE_WAKE) {
            if (now - phaseStart >= WAKE_FRAMES[wakeIdx].dur) {
                wakeIdx++;
                phaseStart = now;
                if (wakeIdx >= WAKE_FRAMES_N) {
                    phase       = PHASE_GREETING; // "root@blaze boot" -> BLAZE -> here_
                    phaseStart  = now;
                    greetStep   = 0;
                }
            }
            return;
        }

        if (phase == PHASE_GREETING) {
            // 3 steps, ~1.1s each
            const uint32_t STEP_MS = 1100;
            if (now - phaseStart >= STEP_MS) {
                greetStep++;
                phaseStart = now;
                if (greetStep > 2) {
                    phase       = PHASE_WAKETAIL; // surprised -> normal
                    phaseStart  = now;
                    wakeIdx     = 0;
                }
            }
            return;
        }

        if (phase == PHASE_WAKETAIL) {
            if (now - phaseStart >= WAKE_TAIL[wakeIdx].dur) {
                wakeIdx++;
                phaseStart = now;
                if (wakeIdx >= WAKE_TAIL_N) {
                    phase = PHASE_FACE;
                    startFace();
                }
            }
            return;
        }

        // ----- FACE MODE (random reactions + auto blink) -----
        if (!blinking && (now - reactionStarted >= reactionDuration)) {
            pickReaction();
            startReaction(currentReaction);
            return;
        }

        if (!blinking && (now >= nextBlinkTime) &&
            (reactionDuration > 200) &&
            (now - reactionStarted < reactionDuration - 200)) {
            blinking    = true;
            blinkStarted = now;
            return;
        }

        if (blinking && (now - blinkStarted >= 120)) {
            blinking      = false;
            // mood scales blink rate: ALERT/brave blink faster, SLEEP slower
            uint32_t lo = 1500, hi = 4501;
            if (moodBiasVal >= 2)      { lo = 600;  hi = 1800; }
            else if (moodBiasVal <= -1) { lo = 3000; hi = 7000; }
            nextBlinkTime = now + random(lo, hi);
            return;
        }
    }

    void renderFace() {
        if (!d) return;

        if (phase == PHASE_LOADING) {
            renderLoading();
            return;
        }

        if (phase == PHASE_GREETING) {
            drawGreeting();
            return;
        }

        if (phase == PHASE_WAKE) {
            const Frame& f = WAKE_FRAMES[wakeIdx];
            if (f.type == 0) drawReaction(f.a);
            else if (f.type == 1) drawCustomEyes(f.a, f.b, f.c, f.dd);
            else drawGreeting();
            return;
        }

        if (phase == PHASE_WAKETAIL) {
            drawReaction(WAKE_TAIL[wakeIdx].a);
            return;
        }

        // PHASE_FACE
        if (blinking) {
            d->clear();
            blinkEyes();
        } else {
            drawReaction(currentReaction);
        }
        drawReactionLabel(currentReaction);
    }

    void renderFlash() {
        if (!d) return;
        d->clear();
        d->setColor(BF_WHITE);
        d->setFont(ArialMT_Plain_16);
        d->setTextAlignment(TEXT_ALIGN_CENTER);
        d->drawString(64, 16, flashText);
        d->setTextAlignment(TEXT_ALIGN_LEFT);
    }
}





