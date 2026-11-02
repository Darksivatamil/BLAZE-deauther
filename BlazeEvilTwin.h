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
    uint8_t attempts();
    bool    validatingNow();

    // web: called from wifi.cpp routes when twin is active
    bool     servePortal();   // true if twin active (server should show portal)
    void     submitPassword(const String& pw); // store + kick async validation
    String   portalHtml();    // phishing page HTML

    // portal templates (plan 3.4): 0 matrix, 1 router, 2 venue
    int         templateId();
    void        setTemplate(int id);
    int         cycleTemplate();       // 0 -> 1 -> 2 -> 0 (persists)
    const char* templateName();
}
