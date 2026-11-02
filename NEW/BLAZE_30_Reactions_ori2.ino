#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>

// =====================================================
// BLAZE - ESP8266 + 0.93" SSD1306 128x64 OLED
// SDA -> D2 (GPIO4)
// SCL -> D1 (GPIO5)
// OLED Address -> 0x3C
// =====================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS  0x3C

#define SDA_PIN D2
#define SCL_PIN D1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================================================
// EYE SETTINGS
// =====================================================

const int LEFT_X   = 20;
const int RIGHT_X  = 78;
const int NORMAL_Y = 24;

const int EYE_W = 30;
const int EYE_H = 17;
const int EYE_R = 5;

// =====================================================
// REACTIONS
// =====================================================

enum Reaction {
  NORMAL = 0,
  HAPPY,
  BLINK,
  LOOK_LEFT,
  LOOK_RIGHT,
  SLEEPY,
  SURPRISED,
  ANGRY,
  SAD,
  LOVE,
  EXCITED,
  VERY_HAPPY,
  SUSPICIOUS,
  SHY,
  SCARED,
  BORED,
  THINKING,
  WORRIED,
  FRUSTRATED,
  PANIC,
  CONFUSED,
  CRYING,
  LAUGHING,
  CURIOUS,
  WINK,
  DOUBTFUL,
  PROUD,
  EMBARRASSED,
  DETERMINED,
  RELAXED
};

const uint8_t TOTAL_REACTIONS = 30;

uint8_t currentReaction = NORMAL;

bool blinking = false;
bool promptVisible = false;
uint8_t currentPrompt = 0;
unsigned long promptStarted = 0;
unsigned long nextPromptTime = 0;

unsigned long reactionStarted  = 0;
unsigned long reactionDuration = 0;

unsigned long nextBlinkTime = 0;
unsigned long blinkStarted  = 0;


// =====================================================
// BASIC FUNCTIONS
// =====================================================

void clearScreen() {
  display.clearDisplay();
}

void showScreen() {
  display.display();
}

void eye(int x, int y, int w, int h, int radius) {
  display.fillRoundRect(x, y, w, h, radius, SSD1306_WHITE);
}


// =====================================================
// NORMAL EYES
// =====================================================

void normalEyes() {

  eye(LEFT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
  eye(RIGHT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
}


// =====================================================
// HAPPY
// =====================================================

void happyEyes() {

  eye(LEFT_X, 28, 30, 13, 5);
  eye(RIGHT_X, 28, 30, 13, 5);
}


// =====================================================
// BLINK
// =====================================================

void blinkEyes() {

  display.fillRoundRect(
    LEFT_X,
    31,
    EYE_W,
    4,
    2,
    SSD1306_WHITE
  );

  display.fillRoundRect(
    RIGHT_X,
    31,
    EYE_W,
    4,
    2,
    SSD1306_WHITE
  );
}


// =====================================================
// LOOK LEFT
// =====================================================

void lookLeft() {

  eye(LEFT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
  eye(RIGHT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);

  // Cut right side
  display.fillRect(
    LEFT_X + 16,
    NORMAL_Y,
    14,
    EYE_H,
    SSD1306_BLACK
  );

  display.fillRect(
    RIGHT_X + 16,
    NORMAL_Y,
    14,
    EYE_H,
    SSD1306_BLACK
  );
}


// =====================================================
// LOOK RIGHT
// =====================================================

void lookRight() {

  eye(LEFT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);
  eye(RIGHT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R);

  // Cut left side
  display.fillRect(
    LEFT_X,
    NORMAL_Y,
    14,
    EYE_H,
    SSD1306_BLACK
  );

  display.fillRect(
    RIGHT_X,
    NORMAL_Y,
    14,
    EYE_H,
    SSD1306_BLACK
  );
}


// =====================================================
// SLEEPY
// =====================================================

void sleepyEyes() {

  display.fillRoundRect(
    LEFT_X,
    31,
    EYE_W,
    5,
    2,
    SSD1306_WHITE
  );

  display.fillRoundRect(
    RIGHT_X,
    31,
    EYE_W,
    5,
    2,
    SSD1306_WHITE
  );
}


// =====================================================
// SURPRISED
// =====================================================

void surprisedEyes() {

  eye(17, 18, 35, 28, 9);
  eye(76, 18, 35, 28, 9);
}


// =====================================================
// ANGRY
// =====================================================

void angryEyes() {

  normalEyes();

  // diagonal cuts
  display.fillTriangle(
    LEFT_X,
    NORMAL_Y,
    LEFT_X + EYE_W,
    NORMAL_Y,
    LEFT_X,
    NORMAL_Y + 8,
    SSD1306_BLACK
  );

  display.fillTriangle(
    RIGHT_X,
    NORMAL_Y,
    RIGHT_X + EYE_W,
    NORMAL_Y,
    RIGHT_X + EYE_W,
    NORMAL_Y + 8,
    SSD1306_BLACK
  );
}


// =====================================================
// SAD
// =====================================================

void sadEyes() {

  eye(LEFT_X, 29, 30, 13, 5);
  eye(RIGHT_X, 29, 30, 13, 5);

  display.fillRect(
    LEFT_X,
    29,
    30,
    4,
    SSD1306_BLACK
  );

  display.fillRect(
    RIGHT_X,
    29,
    30,
    4,
    SSD1306_BLACK
  );
}


// =====================================================
// HEART
// =====================================================

void drawHeart(int cx, int cy) {

  display.fillCircle(
    cx - 4,
    cy - 3,
    4,
    SSD1306_BLACK
  );

  display.fillCircle(
    cx + 4,
    cy - 3,
    4,
    SSD1306_BLACK
  );

  display.fillTriangle(
    cx - 8,
    cy - 1,
    cx + 8,
    cy - 1,
    cx,
    cy + 8,
    SSD1306_BLACK
  );
}


// =====================================================
// LOVE
// =====================================================

void loveEyes() {

  normalEyes();

  drawHeart(35, 30);
  drawHeart(93, 30);
}


// =====================================================
// 20 ADDITIONAL EXPRESSIONS (OLED-friendly geometric eyes)
// =====================================================

void drawExtraReaction(uint8_t reaction) {
  switch (reaction) {
    case EXCITED:
      eye(LEFT_X, 17, 30, 27, 9); eye(RIGHT_X, 17, 30, 27, 9); break;
    case VERY_HAPPY:
      display.drawRoundRect(LEFT_X, 28, 30, 12, 5, SSD1306_WHITE);
      display.drawRoundRect(RIGHT_X, 28, 30, 12, 5, SSD1306_WHITE); break;
    case SUSPICIOUS:
      display.fillRoundRect(LEFT_X, 25, 30, 16, 5, SSD1306_WHITE);
      display.fillRoundRect(RIGHT_X, 22, 30, 16, 5, SSD1306_WHITE);
      display.fillTriangle(LEFT_X, 24, LEFT_X + 30, 24, LEFT_X + 30, 30, SSD1306_BLACK);
      display.fillTriangle(RIGHT_X, 21, RIGHT_X + 30, 27, RIGHT_X, 27, SSD1306_BLACK); break;
    case SHY:
      eye(LEFT_X + 4, 28, 22, 12, 6); eye(RIGHT_X + 4, 28, 22, 12, 6);
      display.fillRect(LEFT_X + 4, 28, 22, 5, SSD1306_BLACK);
      display.fillRect(RIGHT_X + 4, 28, 22, 5, SSD1306_BLACK); break;
    case SCARED:
      eye(LEFT_X - 2, 15, 34, 31, 12); eye(RIGHT_X - 2, 15, 34, 31, 12);
      display.fillRoundRect(LEFT_X + 11, 26, 9, 9, 3, SSD1306_BLACK);
      display.fillRoundRect(RIGHT_X + 11, 26, 9, 9, 3, SSD1306_BLACK); break;
    case BORED:
      eye(LEFT_X, 30, 30, 10, 4); eye(RIGHT_X, 30, 30, 10, 4);
      display.fillRect(LEFT_X, 30, 30, 4, SSD1306_BLACK);
      display.fillRect(RIGHT_X, 30, 30, 4, SSD1306_BLACK); break;
    case THINKING:
      eye(LEFT_X, 22, 30, 18, 7); eye(RIGHT_X, 26, 30, 14, 6);
      display.fillRoundRect(LEFT_X + 19, 27, 7, 7, 3, SSD1306_BLACK);
      display.fillRoundRect(RIGHT_X + 19, 29, 7, 7, 3, SSD1306_BLACK); break;
    case WORRIED:
      eye(LEFT_X, 28, 30, 15, 7); eye(RIGHT_X, 28, 30, 15, 7);
      display.fillTriangle(LEFT_X, 27, LEFT_X + 30, 27, LEFT_X, 34, SSD1306_BLACK);
      display.fillTriangle(RIGHT_X, 27, RIGHT_X + 30, 34, RIGHT_X + 30, 27, SSD1306_BLACK); break;
    case FRUSTRATED:
      normalEyes();
      display.drawLine(LEFT_X, 22, LEFT_X + 30, 29, SSD1306_BLACK);
      display.drawLine(RIGHT_X, 29, RIGHT_X + 30, 22, SSD1306_BLACK); break;
    case PANIC:
      eye(LEFT_X - 2, 13, 34, 34, 12); eye(RIGHT_X - 2, 13, 34, 34, 12);
      display.drawRoundRect(LEFT_X + 7, 22, 16, 16, 5, SSD1306_BLACK);
      display.drawRoundRect(RIGHT_X + 7, 22, 16, 16, 5, SSD1306_BLACK); break;
    case CONFUSED:
      eye(LEFT_X, 21, 30, 20, 7); eye(RIGHT_X, 28, 30, 13, 6);
      display.fillRoundRect(LEFT_X + 12, 28, 7, 7, 3, SSD1306_BLACK);
      display.fillRoundRect(RIGHT_X + 12, 30, 7, 7, 3, SSD1306_BLACK); break;
    case CRYING:
      sadEyes();
      display.drawLine(LEFT_X + 7, 43, LEFT_X + 5, 49, SSD1306_WHITE);
      display.drawLine(RIGHT_X + 22, 43, RIGHT_X + 24, 49, SSD1306_WHITE); break;
    case LAUGHING:
      display.drawRoundRect(LEFT_X, 26, 30, 14, 7, SSD1306_WHITE);
      display.drawRoundRect(RIGHT_X, 26, 30, 14, 7, SSD1306_WHITE);
      display.drawLine(LEFT_X + 3, 35, LEFT_X + 27, 35, SSD1306_WHITE);
      display.drawLine(RIGHT_X + 3, 35, RIGHT_X + 27, 35, SSD1306_WHITE); break;
    case CURIOUS:
      eye(LEFT_X, 21, 30, 22, 8); eye(RIGHT_X, 24, 30, 19, 8);
      display.fillRoundRect(LEFT_X + 18, 28, 7, 7, 3, SSD1306_BLACK);
      display.fillRoundRect(RIGHT_X + 19, 29, 7, 7, 3, SSD1306_BLACK); break;
    case WINK:
      display.drawRoundRect(LEFT_X, 27, 30, 13, 6, SSD1306_WHITE);
      eye(RIGHT_X, NORMAL_Y, EYE_W, EYE_H, EYE_R); break;
    case DOUBTFUL:
      eye(LEFT_X, 28, 30, 14, 6); eye(RIGHT_X, 23, 30, 17, 7);
      display.fillRect(LEFT_X, 28, 30, 5, SSD1306_BLACK); break;
    case PROUD:
      display.drawRoundRect(LEFT_X, 21, 30, 18, 8, SSD1306_WHITE);
      display.drawRoundRect(RIGHT_X, 21, 30, 18, 8, SSD1306_WHITE);
      display.drawLine(LEFT_X + 5, 22, LEFT_X + 25, 22, SSD1306_WHITE);
      display.drawLine(RIGHT_X + 5, 22, RIGHT_X + 25, 22, SSD1306_WHITE); break;
    case EMBARRASSED:
      eye(LEFT_X + 3, 27, 24, 14, 6); eye(RIGHT_X + 3, 27, 24, 14, 6);
      display.fillRoundRect(LEFT_X + 12, 30, 7, 7, 3, SSD1306_BLACK);
      display.fillRoundRect(RIGHT_X + 12, 30, 7, 7, 3, SSD1306_BLACK); break;
    case DETERMINED:
      normalEyes();
      display.fillTriangle(LEFT_X, 22, LEFT_X + 30, 22, LEFT_X, 29, SSD1306_BLACK);
      display.fillTriangle(RIGHT_X, 22, RIGHT_X + 30, 29, RIGHT_X + 30, 22, SSD1306_BLACK); break;
    case RELAXED:
      display.drawRoundRect(LEFT_X, 29, 30, 9, 4, SSD1306_WHITE);
      display.drawRoundRect(RIGHT_X, 29, 30, 9, 4, SSD1306_WHITE); break;
  }
}

// =====================================================
// REACTION LABELS (only used for the requested expressions)
// =====================================================

void drawSmallHeart(int x, int y) {
  display.fillCircle(x + 2, y + 2, 2, SSD1306_WHITE);
  display.fillCircle(x + 6, y + 2, 2, SSD1306_WHITE);
  display.fillTriangle(x, y + 3, x + 8, y + 3, x + 4, y + 8, SSD1306_WHITE);
}

void drawReactionLabel(uint8_t reaction) {
  const char *label = NULL;
  switch (reaction) {
    case SLEEPY: label = "Sleepy! zZ"; break;
    case ANGRY: label = "Ahhh!!"; break;
    case SURPRISED: label = "!! Surprised !!"; break;
    case THINKING: label = "Thinking...?"; break;
    case LOVE: break; // Hearts are drawn as pixels for the built-in OLED font.
    default: return;
  }

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  if (reaction == LOVE) {
    display.setCursor(52, 3);
    display.print("Love");
    drawSmallHeart(38, 2);
    drawSmallHeart(82, 2);
  } else {
    int textWidth = strlen(label) * 6;
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 3);
    display.print(label);
  }
}

// =====================================================
// DRAW REACTION
// =====================================================

void drawReaction(uint8_t reaction) {

  clearScreen();

  switch (reaction) {

    case NORMAL:
      normalEyes();
      break;

    case HAPPY:
      happyEyes();
      break;

    case BLINK:
      blinkEyes();
      break;

    case LOOK_LEFT:
      lookLeft();
      break;

    case LOOK_RIGHT:
      lookRight();
      break;

    case SLEEPY:
      sleepyEyes();
      break;

    case SURPRISED:
      surprisedEyes();
      break;

    case ANGRY:
      angryEyes();
      break;

    case SAD:
      sadEyes();
      break;

    case LOVE:
      loveEyes();
      break;

    default:
      drawExtraReaction(reaction);
      break;
  }

  drawReactionLabel(reaction);
  showScreen();
}


// =====================================================
// CUSTOM WAKE-UP EYE
// =====================================================

void drawCustomEyes(
  int leftW,
  int leftH,
  int rightW,
  int rightH
) {

  clearScreen();

  int leftX =
    35 - (leftW / 2);

  int rightX =
    93 - (rightW / 2);

  int leftY =
    32 - (leftH / 2);

  int rightY =
    32 - (rightH / 2);

  display.fillRoundRect(
    leftX,
    leftY,
    leftW,
    leftH,
    min(5, leftH / 2),
    SSD1306_WHITE
  );

  display.fillRoundRect(
    rightX,
    rightY,
    rightW,
    rightH,
    min(5, rightH / 2),
    SSD1306_WHITE
  );

  showScreen();
}


// =====================================================
// LOADING / INITIALIZING SCREEN
// 4 SECONDS
// =====================================================

void initializationScreen() {

  const unsigned long totalTime = 4000;
  const unsigned long startTime = millis();

  while (millis() - startTime < totalTime) {

    unsigned long elapsed =
      millis() - startTime;

    int progress =
      map(elapsed, 0, totalTime, 0, 100);

    int barWidth =
      map(progress, 0, 100, 0, 94);

    clearScreen();

    // Animated orbiting sparks and a moving scan line
    int phase = (elapsed / 70) % SCREEN_WIDTH;
    for (int i = 0; i < 5; ++i) {
      int sx = (phase + i * 27) % SCREEN_WIDTH;
      int sy = 5 + ((i * 11 + elapsed / 90) % 16);
      display.drawPixel(sx, sy, SSD1306_WHITE);
    }
    display.drawFastHLine((elapsed / 18) % SCREEN_WIDTH, 25, 10, SSD1306_WHITE);

    // Top robot box
    display.drawRoundRect(
      24,
      4,
      80,
      18,
      4,
      SSD1306_WHITE
    );

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(40, 10);
    display.print("BLAZE AI");

    // INITIALIZING
    display.setCursor(28, 29);
    display.print("INITIALIZING");

    // Progress border
    display.drawRoundRect(
      15,
      43,
      98,
      12,
      3,
      SSD1306_WHITE
    );

    // Progress fill
    if (barWidth > 0) {

      display.fillRoundRect(
        17,
        45,
        barWidth,
        8,
        2,
        SSD1306_WHITE
      );
    }

    showScreen();

    delay(40);
  }
}


// =====================================================
// HACKER-STYLE "BLAZE here" STARTUP GREETING
// =====================================================

void blazeGreetingAnimation() {
  display.setTextColor(SSD1306_WHITE);
  for (uint8_t frame = 0; frame < 18; ++frame) {
    clearScreen();
    display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(8, 7);
    display.print("root@blaze:~$ boot");
    for (uint8_t col = 0; col < 10; ++col) {
      int x = 8 + col * 12;
      int y = 18 + ((frame * 5 + col * 9) % 34);
      display.drawPixel(x, y, SSD1306_WHITE);
      display.drawPixel(x + 2, y + 7, SSD1306_WHITE);
    }
    display.drawFastHLine(6, 18 + ((frame * 3) % 37), 116, SSD1306_WHITE);
    if (frame >= 5) {
      display.setTextSize(2);
      display.setCursor(34, 24);
      display.print("BLAZE");
    }
    if (frame >= 11) {
      display.setTextSize(1);
      display.setCursor(52, 46);
      display.print("here_");
    }
    showScreen();
    delay(90);
  }
}

// =====================================================
// IMPRESSIVE WAKE-UP
// Combined 6 + 7 + 8 + 9 + 10
// =====================================================

void wakeUpAnimation() {

  // ---------------------------------------------------
  // 1. Sleeping
  // ---------------------------------------------------

  drawReaction(SLEEPY);
  delay(600);


  // ---------------------------------------------------
  // 2. Eyes slowly starting to open
  // ---------------------------------------------------

  drawCustomEyes(30, 7, 30, 7);
  delay(180);

  drawCustomEyes(30, 10, 30, 10);
  delay(180);


  // ---------------------------------------------------
  // 3. ONE EYE OPENS FIRST
  // ---------------------------------------------------

  drawCustomEyes(
    30, 17,
    30, 5
  );

  delay(500);


  // ---------------------------------------------------
  // 4. Second eye opens
  // ---------------------------------------------------

  drawCustomEyes(
    30, 17,
    30, 12
  );

  delay(180);

  drawReaction(NORMAL);

  delay(450);


  // ---------------------------------------------------
  // 5. Confused - look left
  // ---------------------------------------------------

  drawReaction(LOOK_LEFT);
  delay(450);


  // ---------------------------------------------------
  // 6. Look right
  // ---------------------------------------------------

  drawReaction(LOOK_RIGHT);
  delay(450);


  // ---------------------------------------------------
  // 7. Center
  // ---------------------------------------------------

  drawReaction(NORMAL);
  delay(300);


  // ---------------------------------------------------
  // 8. SURPRISED / SHOCKED
  // ---------------------------------------------------

  drawReaction(SURPRISED);
  delay(600);


  // ---------------------------------------------------
  // 9. Back normal
  // ---------------------------------------------------

  drawReaction(NORMAL);
  delay(250);


  // ---------------------------------------------------
  // 10. DOUBLE BLINK
  // ---------------------------------------------------

  drawReaction(BLINK);
  delay(120);

  drawReaction(NORMAL);
  delay(160);

  drawReaction(BLINK);
  delay(120);

  drawReaction(NORMAL);
  delay(300);


  // ---------------------------------------------------
  // 11. STRETCHING EYES
  // Tiny -> Medium -> Big -> Normal
  // ---------------------------------------------------

  drawCustomEyes(
    8, 8,
    8, 8
  );
  delay(100);

  drawCustomEyes(
    16, 12,
    16, 12
  );
  delay(100);

  drawCustomEyes(
    25, 16,
    25, 16
  );
  delay(100);

  drawCustomEyes(
    36, 25,
    36, 25
  );
  delay(180);

  drawReaction(NORMAL);

  delay(350);


  // ---------------------------------------------------
  // 12. HACKER-STYLE BLAZE HERE GREETING
  // ---------------------------------------------------

  blazeGreetingAnimation();


  // ---------------------------------------------------
  // 13. BLAZE RETURNS
  // ---------------------------------------------------

  drawReaction(SURPRISED);
  delay(350);

  drawReaction(NORMAL);
  delay(600);
}


// =====================================================
// TAMIL-ENGLISH CONVERSATION PROMPTS (35 total)
// Mood tags: 0 general, 1 cheerful, 2 concerned, 3 curious, 4 greeting.
// =====================================================
struct Prompt { const char *text; uint8_t mood; };
const Prompt prompts[] = {
  {"Hii da!",4},{"Hii di!",4},{"Enna panringa?",4},{"Eppadi irukkeenga?",4},{"Saptingala?",4},
  {"Innaiku day epdi?",0},{"Nalla irukeengala?",4},{"Pesalama konjam?",0},{"Ungala paatha happy!",1},{"Super-aa irukku!",1},
  {"Semma energy today!",1},{"Sirichite irunga!",1},{"Chinna break venuma?",0},{"Tea saapdalaama?",0},{"Music kekkalaama?",0},
  {"Enna plan inniku?",3},{"Oru pudhu idea irukka?",3},{"Naan help pannava?",0},{"Yedhavadhu venuma?",0},{"Konjam rest edunga.",2},
  {"Tension edukkadheenga.",2},{"Ellam seri aagum!",2},{"Careful-aa irunga.",2},{"Naan inga irukken.",2},{"Idhu epdi work aagum?",3},
  {"Try panni paakalaama?",3},{"Neenga sollunga!",3},{"Wow, nalla irukku!",1},{"Thanks da, friend!",1},{"Ready-aa irukeengala?",0},
  {"BLAZE ready!",1},{"Oru joke sollava?",1},{"Konjam yosikkiren...",3},{"Aama, puriyudhu!",0},{"Take care, okay?",2}
};
const uint8_t PROMPT_COUNT = sizeof(prompts) / sizeof(prompts[0]);

uint8_t reactionMood(uint8_t reaction) {
  switch (reaction) {
    case HAPPY: case EXCITED: case VERY_HAPPY: case LOVE: case LAUGHING: case PROUD: return 1;
    case SAD: case SCARED: case WORRIED: case FRUSTRATED: case PANIC: case CRYING: return 2;
    case SURPRISED: case THINKING: case CONFUSED: case CURIOUS: case DOUBTFUL: case SUSPICIOUS: return 3;
    case ANGRY: return 2;
    case NORMAL: case SHY: case WINK: case EMBARRASSED: return 4;
    default: return 0;
  }
}

uint8_t choosePrompt(uint8_t reaction) {
  uint8_t mood = reactionMood(reaction);
  uint8_t eligible[PROMPT_COUNT];
  uint8_t count = 0;
  for (uint8_t i = 0; i < PROMPT_COUNT; ++i)
    if (prompts[i].mood == 0 || prompts[i].mood == mood) eligible[count++] = i;
  return count ? eligible[random(count)] : 0;
}

// During a prompt, keep a compact version of the current expression above it.
void drawCompactFace(uint8_t reaction) {
  int leftY = 12, rightY = 12;
  int leftH = 15, rightH = 15;
  switch (reaction) {
    case SURPRISED: case SCARED: case PANIC: leftY = rightY = 7; leftH = rightH = 24; break;
    case HAPPY: case VERY_HAPPY: case LAUGHING: leftY = rightY = 18; leftH = rightH = 9; break;
    case SLEEPY: case BORED: leftY = rightY = 20; leftH = rightH = 6; break;
    case SAD: case WORRIED: case CRYING: leftY = rightY = 16; leftH = rightH = 12; break;
    case CONFUSED: case THINKING: case CURIOUS: leftY = 8; rightY = 15; leftH = 19; rightH = 12; break;
    default: break;
  }
  display.fillRoundRect(25, leftY, 28, leftH, 5, SSD1306_WHITE);
  display.fillRoundRect(75, rightY, 28, rightH, 5, SSD1306_WHITE);
  if (reaction == ANGRY || reaction == FRUSTRATED || reaction == DETERMINED) {
    display.fillTriangle(25, leftY, 53, leftY, 25, leftY + 7, SSD1306_BLACK);
    display.fillTriangle(75, rightY, 103, rightY + 7, 103, rightY, SSD1306_BLACK);
  } else if (reaction == LOOK_LEFT || reaction == LOOK_RIGHT || reaction == SUSPICIOUS || reaction == DOUBTFUL) {
    int pupilOffset = (reaction == LOOK_LEFT) ? 3 : 19;
    display.fillRoundRect(25 + pupilOffset, leftY + 4, 7, leftH - 8, 3, SSD1306_BLACK);
    display.fillRoundRect(75 + pupilOffset, rightY + 4, 7, rightH - 8, 3, SSD1306_BLACK);
  } else if (reaction == BLINK || reaction == WINK) {
    display.fillRect(25, leftY + leftH / 2, 28, 3, SSD1306_BLACK);
  }
}

void renderCurrentScene() {
  if (!promptVisible) {
    drawReaction(blinking ? BLINK : currentReaction);
    return;
  }

  clearScreen();
  drawCompactFace(currentReaction);
  drawReactionLabel(currentReaction);
  display.drawFastHLine(8, 39, 112, SSD1306_WHITE);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  const char *message = prompts[currentPrompt].text;
  int x = 8, y = 44;
  display.setCursor(x, y);
  for (const char *c = message; *c; ++c) {
    if (x > 119) { x = 8; y += 9; display.setCursor(x, y); }
    display.write(*c);
    x += 6;
  }
  showScreen();
}

// =====================================================
// START NORMAL RANDOM REACTION
// =====================================================

void startReaction(uint8_t reaction) {

  currentReaction = reaction;

  renderCurrentScene();

  reactionStarted = millis();

  // Reaction stays 3 - 10 seconds
  reactionDuration =
    random(3000UL, 10001UL);

  // Auto blink after 1.5 - 4.5 sec
  nextBlinkTime =
    millis() + random(1500UL, 4501UL);

  blinking = false;
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    Serial.println(
      "OLED not found at 0x3C!"
    );

    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.display();

  Serial.println("BLAZE OLED READY");

  randomSeed(
    analogRead(A0) ^ micros()
  );


  // ===================================================
  // STARTUP SEQUENCE
  // ===================================================

  // 1. 4-second initialization
  initializationScreen();

  // 2. Impressive Blaze wake-up
  wakeUpAnimation();

  // 3. Begin normal life loop
  startReaction(NORMAL);
  nextPromptTime = millis() + random(20000UL, 30001UL);
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  unsigned long now = millis();

  // Prompt start times are 20-30 seconds apart; each stays visible for 10 seconds.
  if (promptVisible && now - promptStarted >= 10000UL) {
    promptVisible = false;
    nextPromptTime = promptStarted + random(20000UL, 30001UL);
    renderCurrentScene();
  } else if (!promptVisible && now >= nextPromptTime) {
    currentPrompt = choosePrompt(currentReaction);
    promptVisible = true;
    promptStarted = now;
    renderCurrentScene();
  }

  // ===================================================
  // CHANGE REACTION
  // ===================================================

  if (
    !blinking &&
    now - reactionStarted >= reactionDuration
  ) {

    // The eight requested major moods have weight 8; all others weight 1.
    // This makes major reactions appear about 74% of selections.
    const uint8_t weighted[] = {
      NORMAL,NORMAL,NORMAL,NORMAL,NORMAL,NORMAL,NORMAL,NORMAL,
      SURPRISED,SURPRISED,SURPRISED,SURPRISED,SURPRISED,SURPRISED,SURPRISED,SURPRISED,
      ANGRY,ANGRY,ANGRY,ANGRY,ANGRY,ANGRY,ANGRY,ANGRY,
      SUSPICIOUS,SUSPICIOUS,SUSPICIOUS,SUSPICIOUS,SUSPICIOUS,SUSPICIOUS,SUSPICIOUS,SUSPICIOUS,
      SCARED,SCARED,SCARED,SCARED,SCARED,SCARED,SCARED,SCARED,
      WORRIED,WORRIED,WORRIED,WORRIED,WORRIED,WORRIED,WORRIED,WORRIED,
      FRUSTRATED,FRUSTRATED,FRUSTRATED,FRUSTRATED,FRUSTRATED,FRUSTRATED,FRUSTRATED,FRUSTRATED,
      PANIC,PANIC,PANIC,PANIC,PANIC,PANIC,PANIC,PANIC,
      HAPPY,BLINK,LOOK_LEFT,LOOK_RIGHT,SLEEPY,SAD,LOVE,EXCITED,VERY_HAPPY,SHY,
      BORED,THINKING,CONFUSED,CRYING,LAUGHING,CURIOUS,WINK,DOUBTFUL,PROUD,EMBARRASSED,
      DETERMINED,RELAXED
    };
    uint8_t newReaction;
    do { newReaction = weighted[random(sizeof(weighted))]; }
    while (newReaction == currentReaction);

    startReaction(newReaction);

    return;
  }


  // ===================================================
  // AUTO BLINK
  // ===================================================

  if (
    !blinking &&
    !promptVisible &&
    now >= nextBlinkTime &&
    reactionDuration > 200 &&
    now - reactionStarted <
      reactionDuration - 200
  ) {

    blinking = true;
    renderCurrentScene();

    blinkStarted = now;

    return;
  }


  // ===================================================
  // FINISH BLINK
  // ===================================================

  if (
    blinking &&
    now - blinkStarted >= 120
  ) {

    blinking = false;
    renderCurrentScene();

    nextBlinkTime =
      now + random(1500UL, 4501UL);

    return;
  }
}
