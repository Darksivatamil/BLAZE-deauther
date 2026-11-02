#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
  LOVE
};

const uint8_t TOTAL_REACTIONS = 10;

uint8_t currentReaction = NORMAL;

bool blinking = false;

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
  }

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
  // 12. HI!
  // ---------------------------------------------------

  clearScreen();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(46, 10);
  display.print("Hi!");

  display.setTextSize(1);
  display.setCursor(35, 39);
  display.print("I am BLAZE");

  showScreen();

  delay(1800);


  // ---------------------------------------------------
  // 13. BLAZE RETURNS
  // ---------------------------------------------------

  drawReaction(SURPRISED);
  delay(350);

  drawReaction(NORMAL);
  delay(600);
}


// =====================================================
// START NORMAL RANDOM REACTION
// =====================================================

void startReaction(uint8_t reaction) {

  currentReaction = reaction;

  drawReaction(currentReaction);

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
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  unsigned long now = millis();


  // ===================================================
  // CHANGE REACTION
  // ===================================================

  if (
    !blinking &&
    now - reactionStarted >= reactionDuration
  ) {

    uint8_t newReaction;

    do {

      newReaction =
        random(TOTAL_REACTIONS);

    }
    while (
      newReaction == currentReaction
    );

    startReaction(newReaction);

    return;
  }


  // ===================================================
  // AUTO BLINK
  // ===================================================

  if (
    !blinking &&
    now >= nextBlinkTime &&
    reactionDuration > 200 &&
    now - reactionStarted <
      reactionDuration - 200
  ) {

    drawReaction(BLINK);

    blinking = true;

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

    drawReaction(currentReaction);

    blinking = false;

    nextBlinkTime =
      now + random(1500UL, 4501UL);

    return;
  }
}
