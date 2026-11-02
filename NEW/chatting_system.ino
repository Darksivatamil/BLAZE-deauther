#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// BLAZE SPLIT-SCREEN CHAT
// ESP8266 + 0.93" SSD1306 128x64 OLED
// =====================================================

// ---------------- OLED ----------------

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_ADDRESS   0x3C

#define SDA_PIN D2
#define SCL_PIN D1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// =====================================================
// SCREEN LAYOUT
// =====================================================

// Top 32 pixels = eyes
// Bottom 32 pixels = chat

#define SPLIT_Y 32


// =====================================================
// EYE POSITION
// =====================================================

#define LEFT_EYE_X   22
#define RIGHT_EYE_X  76

#define NORMAL_EYE_Y  9

#define EYE_WIDTH     30
#define EYE_HEIGHT    14
#define EYE_RADIUS     4


// =====================================================
// EYE STATE
// =====================================================

enum EyeState {
  EYE_NORMAL,
  EYE_SLEEP
};

EyeState eyeState = EYE_NORMAL;


// =====================================================
// BLINK SYSTEM
// =====================================================

bool isBlinking = false;

unsigned long blinkStartTime = 0;
unsigned long nextBlinkTime  = 0;

const unsigned long BLINK_DURATION = 120;


// =====================================================
// SLEEP SYSTEM
// =====================================================

bool isSleeping = false;

unsigned long sleepStartTime = 0;
unsigned long nextSleepTime  = 0;

const unsigned long SLEEP_DURATION = 3000;


// =====================================================
// CHAT
// =====================================================

String chatMessage = "Hello Blaze!";


// =====================================================
// DRAW NORMAL EYES
// =====================================================

void drawNormalEyes() {

  display.fillRoundRect(
    LEFT_EYE_X,
    NORMAL_EYE_Y,
    EYE_WIDTH,
    EYE_HEIGHT,
    EYE_RADIUS,
    SSD1306_WHITE
  );

  display.fillRoundRect(
    RIGHT_EYE_X,
    NORMAL_EYE_Y,
    EYE_WIDTH,
    EYE_HEIGHT,
    EYE_RADIUS,
    SSD1306_WHITE
  );
}


// =====================================================
// DRAW SLEEP EYES
// =====================================================

void drawSleepEyes() {

  display.fillRoundRect(
    LEFT_EYE_X,
    15,
    EYE_WIDTH,
    3,
    1,
    SSD1306_WHITE
  );

  display.fillRoundRect(
    RIGHT_EYE_X,
    15,
    EYE_WIDTH,
    3,
    1,
    SSD1306_WHITE
  );
}


// =====================================================
// DRAW BLINK EYES
// =====================================================

void drawBlinkEyes() {

  display.fillRoundRect(
    LEFT_EYE_X,
    15,
    EYE_WIDTH,
    3,
    1,
    SSD1306_WHITE
  );

  display.fillRoundRect(
    RIGHT_EYE_X,
    15,
    EYE_WIDTH,
    3,
    1,
    SSD1306_WHITE
  );
}


// =====================================================
// DRAW CHAT AREA
// =====================================================

void drawChatArea() {

  // Separator
  display.drawFastHLine(
    0,
    SPLIT_Y,
    SCREEN_WIDTH,
    SSD1306_WHITE
  );


  // Chat text
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(3, 37);

  display.print(chatMessage);
}


// =====================================================
// DRAW COMPLETE SCREEN
// =====================================================

void drawScreen() {

  display.clearDisplay();


  // ===================================================
  // TOP HALF - BLAZE EYES
  // ===================================================

  if (isBlinking) {

    drawBlinkEyes();

  }
  else if (eyeState == EYE_SLEEP) {

    drawSleepEyes();

  }
  else {

    drawNormalEyes();
  }


  // ===================================================
  // BOTTOM HALF - CHAT
  // ===================================================

  drawChatArea();


  // ===================================================
  // UPDATE OLED
  // ===================================================

  display.display();
}


// =====================================================
// SET CHAT MESSAGE
// =====================================================

void setChatMessage(const String &message) {

  chatMessage = message;

  drawScreen();
}


// =====================================================
// START BLINK
// =====================================================

void startBlink() {

  isBlinking = true;

  blinkStartTime = millis();

  drawScreen();
}


// =====================================================
// UPDATE BLINK
// =====================================================

void updateBlink() {

  unsigned long now = millis();


  // -----------------------------------------------
  // Currently blinking
  // -----------------------------------------------

  if (isBlinking) {

    if (
      now - blinkStartTime >= BLINK_DURATION
    ) {

      isBlinking = false;

      // Next blink after 2 - 5 seconds
      nextBlinkTime =
        now + random(2000UL, 5001UL);

      drawScreen();
    }

    return;
  }


  // -----------------------------------------------
  // Start new blink
  // -----------------------------------------------

  if (
    !isSleeping &&
    now >= nextBlinkTime
  ) {

    startBlink();
  }
}


// =====================================================
// START SLEEP
// =====================================================

void startSleep() {

  isSleeping = true;

  eyeState = EYE_SLEEP;

  sleepStartTime = millis();

  // Cancel blink while sleeping
  isBlinking = false;

  drawScreen();
}


// =====================================================
// UPDATE SLEEP
// =====================================================

void updateSleep() {

  unsigned long now = millis();


  // -----------------------------------------------
  // Currently sleeping
  // -----------------------------------------------

  if (isSleeping) {

    if (
      now - sleepStartTime >= SLEEP_DURATION
    ) {

      isSleeping = false;

      eyeState = EYE_NORMAL;

      // Next sleep after 15 - 30 seconds
      nextSleepTime =
        now + random(15000UL, 30001UL);

      // Give a fresh blink schedule
      nextBlinkTime =
        now + random(2000UL, 5001UL);

      drawScreen();
    }

    return;
  }


  // -----------------------------------------------
  // Start sleeping
  // -----------------------------------------------

  if (
    now >= nextSleepTime
  ) {

    startSleep();
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);


  // -----------------------------------------------
  // I2C
  // -----------------------------------------------

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );


  // -----------------------------------------------
  // OLED
  // -----------------------------------------------

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS
    )
  ) {

    Serial.println(
      "OLED not found at 0x3C!"
    );

    while (true) {
      delay(100);
    }
  }


  // -----------------------------------------------
  // Random seed
  // -----------------------------------------------

  randomSeed(
    analogRead(A0) ^ micros()
  );


  // -----------------------------------------------
  // Initial timing
  // -----------------------------------------------

  unsigned long now = millis();

  // First blink after 2 - 5 sec
  nextBlinkTime =
    now + random(2000UL, 5001UL);

  // First sleep after 15 - 30 sec
  nextSleepTime =
    now + random(15000UL, 30001UL);


  // -----------------------------------------------
  // First chat
  // -----------------------------------------------

  chatMessage = "Hello Blaze!";


  // -----------------------------------------------
  // Draw screen
  // -----------------------------------------------

  drawScreen();


  Serial.println("BLAZE SPLIT SCREEN READY");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  updateBlink();

  updateSleep();

}
