/* BLAZE - Team DARK
   Loyalty watchdogs - see BlazeLoyal.h (UPGRADE_PLAN 2.3)
   - OwnNetGuard: own/home SSIDs can never become attack targets
     (selection guard + Attack::start() safety net + blockMac() packet guard)
   - IntruderAlert: home SSID in the probe log, or a clone of it in scan
     results (same name, different radio) while idle -> EV_INTRUDER */

#include "BlazeLoyal.h"

#include <ESP8266WiFi.h>
#include "A_config.h"
#include "settings.h"
#include "language.h"
#include "BlazeCfg.h"
#include "BlazePersona.h"
#include "BlazeRecon.h"
#include "Attack.h" // brings externs: ssids, accesspoints, stations (+ names via Accesspoints.h)

extern Attack attack; // the instance lives in the main sketch

namespace BlazeLoyal {
    // ===== serial voice (PROGMEM) ===== //
    const char GUARD_HIT[] PROGMEM     = "[BLAZE] own-net guard: ";
    const char GUARD_REFUSED[] PROGMEM = "[BLAZE] own-net guard - refused.";

    // ===== config ===== //
    static String homeSsid;        // BlazeCfg "blaze.home" ("" = unset)
    static bool   watch = false;   // IntruderAlert armed (home set, not our own AP)

    // ===== packet-level guard ===== //
    static uint8_t  ownMac[6];
    static bool     ownMacSet  = false;
    static uint8_t  homeMac[6];
    static bool     homeMacSet = false;
    static uint32_t blockAt    = 0;   // last blockMac refusal flash

    // ===== IntruderAlert state ===== //
    static uint32_t probeSeen = 0;   // BlazeRecon::probeTotal() at last check
    static uint32_t pollAt    = 0;   // last 2 s scan-list poll
    static uint32_t alertAt   = 0;   // last EV_INTRUDER fire

    // ===== helpers ===== //
    static bool homeIsOwn() {
        if (!homeSsid.length()) return false;
        const char* own = settings::getAccessPointSettings().ssid;
        return own[0] && homeSsid.equalsIgnoreCase(own);
    }

    static void refreshWatch() {
        watch = (homeSsid.length() > 0) && !homeIsOwn();
    }

    static void resolveHomeMac() {
        if (homeMacSet || !homeSsid.length()) return;
        int n = accesspoints.count();
        for (int i = 0; i < n; i++) {
            if (accesspoints.getSSID(i).equalsIgnoreCase(homeSsid)) {
                memcpy(homeMac, accesspoints.getMac(i), 6);
                homeMacSet = true;
                return;
            }
        }
    }

    static bool trip(const String& what) { // refusal: serial line + EV_HOUSE flash
        prnt(GUARD_HIT);
        prntln(what);
        BlazePersona::onEvent(BlazePersona::EV_HOUSE);
        return false;
    }

    static String dropProtected() { // deselect protected targets, return first hit
        String hit;

        int n = accesspoints.count();
        for (int i = 0; i < n; i++) {
            if (!accesspoints.getSelected(i)) continue;
            String s = accesspoints.getSSID(i);
            if (isProtected(s)) {
                accesspoints.deselect(i);
                if (!hit.length()) hit = s;
            }
        }

        n = stations.count();
        for (int i = 0; i < n; i++) {
            if (!stations.getSelected(i)) continue;
            String s = stations.getAPStr(i);
            if (isProtected(s)) {
                stations.deselect(i);
                if (!hit.length()) hit = s;
            }
        }

        n = names.count();
        for (int i = 0; i < n; i++) {
            if (!names.getSelected(i)) continue;
            String s = names.getName(i);
            if (isProtected(s)) {
                names.deselect(i);
                if (!hit.length()) hit = s;
            }
        }

        return hit; // "" = nothing protected was selected
    }

    static bool fireIntruder() {
        if (!watch) return false;
        if (attack.isRunning()) return false;                 // plan: only while idle
        uint32_t now = millis();
        if (alertAt && (now - alertAt) < 30000) return false; // 30 s cooldown
        alertAt = now;
        BlazePersona::onEvent(BlazePersona::EV_INTRUDER);
        return true;
    }

    // ===== public API ===== //

    void setup() {
        homeSsid = BlazeCfg::get("blaze.home", BLAZE_HOME_SSID);
        homeSsid.trim();
        refreshWatch();

        WiFi.softAPmacAddress(ownMac); // effective AP MAC (wifi::begin() already ran)
        ownMacSet = true;

        probeSeen  = BlazeRecon::probeTotal();
        pollAt     = millis();
        alertAt    = 0;
        blockAt    = 0;
        homeMacSet = false;
        resolveHomeMac();
    }

    void update() {
        // --- IntruderAlert: new probe requests since the last check ---
        if (watch) {
            uint32_t total = BlazeRecon::probeTotal();
            if (total > probeSeen) {
                int fresh = (int)(total - probeSeen);
                if (fresh > 32) fresh = 32; // BlazeRecon ring size (PROBE_MAX)
                probeSeen = total;

                for (int i = 0; i < fresh; i++) {
                    String line = BlazeRecon::probeLine(i); // "XX:XX:XX SSID RSSI"
                    int sp = line.indexOf(' ');
                    if (sp <= 0) continue;

                    String rest = line.substring(sp + 1);
                    int lastSp  = rest.lastIndexOf(' ');
                    if (lastSp > 0) rest = rest.substring(0, lastSp); // drop the RSSI token

                    if (rest.equalsIgnoreCase(homeSsid)) {
                        fireIntruder();
                        break;
                    }
                }
            }
        }

        // --- 2 s poll: home-SSID clone check + selection sweep ---
        uint32_t now = millis();
        if ((int32_t)(now - pollAt) < 2000) return;
        pollAt = now;

        if (watch) {
            int n = accesspoints.count();
            for (int i = 0; i < n; i++) {
                if (!accesspoints.getSSID(i).equalsIgnoreCase(homeSsid)) continue;

                uint8_t* mac = accesspoints.getMac(i);
                // same name, different radio = someone cloning our home network
                if (homeMacSet && memcmp(mac, homeMac, 6) != 0) fireIntruder();
                memcpy(homeMac, mac, 6); // track the newest sighting
                homeMacSet = true;
                break;
            }
        }

        // OwnNetGuard sweep: protected targets never stay selected
        String drop = dropProtected();
        if (drop.length()) trip(drop);
    }

    bool isProtected(const String& ssid) {
        if (!ssid.length()) return false;
        const char* own = settings::getAccessPointSettings().ssid;
        if (own[0] && ssid.equalsIgnoreCase(own)) return true;
        if (homeSsid.length() && ssid.equalsIgnoreCase(homeSsid)) return true;
        return false;
    }

    bool allowSelect(const String& ssid) {
        if (!isProtected(ssid)) return true;
        return trip(ssid);
    }

    bool allowAttack(bool useSsids) {
        resolveHomeMac(); // fresh BSSID before any packet goes out

        // beacon/probe payloads: never broadcast a protected name
        if (useSsids) {
            int n = ssids.count();
            for (int i = 0; i < n; i++) {
                String s = ssids.getName(i);
                if (isProtected(s)) return trip(s);
            }
        }

        // selected targets: clean them out, then refuse this start
        String hit = dropProtected();
        if (hit.length()) return trip(hit);

        return true;
    }

    void refused() {
        prntln(GUARD_REFUSED);
        BlazePersona::onEvent(BlazePersona::EV_HOUSE);
    }

    bool blockMac(const uint8_t* mac) {
        if (!mac) return false;

        bool hit = false;
        if (ownMacSet && memcmp(mac, ownMac, 6) == 0) hit = true;
        else if (homeMacSet && memcmp(mac, homeMac, 6) == 0) hit = true;
        if (!hit) return false;

        // blocked unconditionally - the flash is rate-limited so hot paths stay cheap
        uint32_t now = millis();
        if (!blockAt || (now - blockAt) >= 5000) {
            blockAt = now;
            refused();
        }
        return true; // "handled": caller keeps its target rotation moving
    }

    String home() { return homeSsid; }

    void setHome(const String& ssid) {
        homeSsid = ssid;
        homeSsid.trim();
        BlazeCfg::set("blaze.home", homeSsid);
        refreshWatch();

        homeMacSet = false;                    // re-baseline the clone check
        alertAt    = 0;
        probeSeen  = BlazeRecon::probeTotal(); // don't trip on old log lines
        resolveHomeMac();

        String hit = dropProtected(); // selections that just became protected
        if (hit.length()) trip(hit);
    }
}
