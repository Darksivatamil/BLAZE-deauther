/* This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#include "DisplayUI.h"

#include "settings.h"
#include "BlazeTracker.h"
#include "BlazeFx.h"
#include "BlazePersona.h"
#include "BlazeChat.h"
#include "BlazeEvilTwin.h"
#include "BlazeRecon.h"
#include "BlazeDetector.h"

// ===== adjustable ===== //
void DisplayUI::configInit() {
    // initialize display
    display.init();

    /*
       In case of a compiler (conversion char/uint8_t) error,
       make sure to have version 4 of the display library installed
       https://github.com/ThingPulse/esp8266-oled-ssd1306/releases/tag/4.0.0
     */
    display.setFont(DejaVu_Sans_Mono_12);

    display.setContrast(255);

    if (FLIP_DIPLAY) display.flipScreenVertically();

    display.clear();
    display.display();
}

void DisplayUI::configOn() {
    display.displayOn();
}

void DisplayUI::configOff() {
    display.displayOff();
}

void DisplayUI::updatePrefix() {
    display.clear();
}

void DisplayUI::updateSuffix() {
    display.display();
}

void DisplayUI::drawString(int x, int y, String str) {
    display.drawString(x, y, replaceUtf8(str, String(QUESTIONMARK)));
}

void DisplayUI::drawString(int row, String str) {
    drawString(0, row * lineHeight, str);
}

void DisplayUI::drawLine(int x1, int y1, int x2, int y2) {
    display.drawLine(x1, y1, x2, y2);
}

// ====================== //


DisplayUI::DisplayUI() {}

DisplayUI::~DisplayUI() {}


void DisplayUI::setup() {
    configInit();
    prntln("BLAZE OLED READY"); // startup message (NEW/loading+blaze_face.ino)
    setupButtons();
    buttonTime = currentTime;

    // compiled button pins (proves what the firmware listens on)
    prnt("BLAZE BTN PINS: UP=");
    prnt((int)BUTTON_UP);
    prnt(" (D6)  DOWN=");
    prnt((int)BUTTON_DOWN);
    prnt(" (D5)  A=");
    prnt((int)BUTTON_A);
    prntln(" (D7)");

#ifdef RTC_DS3231
    bool h12;
    bool PM_time;
    clock.setClockMode(false);
    clockHour   = clock.getHour(h12, PM_time);
    clockMinute = clock.getMinute();
#else // ifdef RTC_DS3231
    clockHour   = random(12);
    clockMinute = random(60);
#endif // ifdef RTC_DS3231

    // ===== MENUS ===== //

    // MAIN MENU - hacker-styled mode launcher
    // SELECT activates, SELECT-hold on [BACK] returns (standard loop)
    createMenu(&mainMenu, NULL, [this]() {
        addMenuNode(&mainMenu, []() { return String("SCAN"); }, &scanMenu);   // SCAN
        addMenuNode(&mainMenu, []() { return String("MODE FACE"); }, [this]() { // FACE reactions
            mode = DISPLAY_MODE::BLAZE_FACE;
            BlazeFace::startFace();
        });
        addMenuNode(&mainMenu, []() { return String("TARGETS"); }, &showMenu); // APs / stations / names / ssids
        addMenuNode(&mainMenu, []() { return String("ATTACK"); }, &attackMenu);// ATTACK
        addMenuNode(&mainMenu, []() { return String("PACKET MONITOR"); }, [this]() { // sniffer
            scan.start(SCAN_MODE_SNIFFER, 0, SCAN_MODE_OFF, 0, false, wifi_channel);
            mode = DISPLAY_MODE::PACKETMONITOR;
        });
        addMenuNode(&mainMenu, []() { return String("DETECTOR"); }, [this]() { // deauther/beacon hunter
            if (BlazeDetector::start()) {
                detectSel = 0;
                mode = DISPLAY_MODE::DETECT;
                restoreMenuFont();
                buttonTime = currentTime;
            } else {
                prntln(String(F("DETECTOR: air busy")));
                flashMsg(String(F("air busy.")), 1500);
            }
        });
        addMenuNode(&mainMenu, []() { return String("TRACKER"); }, [this]() { // pick target -> track
            startTracker(-1);
        });
        addMenuNode(&mainMenu, []() { return String("EVIL TWIN"); }, [this]() { // twin
            startTwin(-1);
        });
        addMenuNode(&mainMenu, []() { // TWIN <style> (plan 3.4): cycle portal template
            return String("TWIN ") + String(BlazeEvilTwin::templateName());
        }, []() {
            BlazeEvilTwin::cycleTemplate();
        });
        addMenuNode(&mainMenu, []() { return String("WEIGHTS"); }, &weightMenu); // FACE WEIGHTS (plan 3.2)
        addMenuNode(&mainMenu, []() { return String("RAIN ") + String(BlazeFx::rainEnabled() ? "ON" : "OFF"); }, []() { // fx
            BlazeFx::toggleRain();
        });
        addMenuNode(&mainMenu, D_CLOCK, &clockMenu); // CLOCK

#ifdef HIGHLIGHT_LED
        addMenuNode(&mainMenu, D_LED, [this]() {     // LED
            highlightLED = !highlightLED;
            digitalWrite(HIGHLIGHT_LED, highlightLED);
        });
#endif // ifdef HIGHLIGHT_LED
    });

    // SCAN MENU
    createMenu(&scanMenu, &mainMenu, [this]() {
        addMenuNode(&scanMenu, D_SCAN_APST, [this]() { // SCAN AP + ST
            scan.start(SCAN_MODE_ALL, 15000, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::LOADSCAN;
        });
        addMenuNode(&scanMenu, D_SCAN_AP, [this]() { // SCAN AP
            scan.start(SCAN_MODE_APS, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::LOADSCAN;
        });
        addMenuNode(&scanMenu, D_SCAN_ST, [this]() { // SCAN ST
            scan.start(SCAN_MODE_STATIONS, 30000, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::LOADSCAN;
        });
        addMenuNode(&scanMenu, [this]() { // PROBE LOG (plan 4.5)
            return leftRight("PROBES", (String)BlazeRecon::probeCount(), maxLen - 1);
        }, [this]() {
            // live log: sniff (channel hopping) if nothing is sniffing yet
            if (!scan.isSniffing()) {
                scan.start(SCAN_MODE_SNIFFER, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
                probeLogSniff = true;
            } else {
                probeLogSniff = false;
            }
            probeScroll = 0;
            mode        = DISPLAY_MODE::PROBELOG;
            restoreMenuFont();
        });
        addMenuNode(&scanMenu, [this]() { // VULNERABILITY SCORE (plan 4.6)
            return leftRight("VULN", String(BlazeRecon::riskScore()), maxLen - 1);
        }, [this]() {
            mode = DISPLAY_MODE::VULN;
            restoreMenuFont();
        });
    });

    // SHOW MENU
    createMenu(&showMenu, &mainMenu, [this]() {
        addMenuNode(&showMenu, [this]() { // Accesspoints 0 [0]
            return leftRight(str(D_ACCESSPOINTS), (String)accesspoints.count(), maxLen - 1);
        }, &apListMenu);
        addMenuNode(&showMenu, [this]() { // Stations 0 [0]
            return leftRight(str(D_STATIONS), (String)stations.count(), maxLen - 1);
        }, &stationListMenu);
        addMenuNode(&showMenu, [this]() { // Names 0 [0]
            return leftRight(str(D_NAMES), (String)names.count(), maxLen - 1);
        }, &nameListMenu);
        addMenuNode(&showMenu, [this]() { // SSIDs 0
            return leftRight(str(D_SSIDS), (String)ssids.count(), maxLen - 1);
        }, &ssidListMenu);
    });

    // AP LIST MENU
    createMenu(&apListMenu, &showMenu, [this]() {
        // add APs to list
        int c = accesspoints.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&apListMenu, [i]() {
                return b2a(accesspoints.getSelected(i)) + accesspoints.getSSID(i);
            }, [this, i]() {
                accesspoints.getSelected(i) ? accesspoints.deselect(i) : accesspoints.select(i);
            }, [this, i]() {
                selectedID = i;
                changeMenu(&apMenu);
            });
        }
        addMenuNode(&apListMenu, D_SELECT_ALL, [this]() { // SELECT ALL
            accesspoints.selectAll();
            changeMenu(&apListMenu);
        });
        addMenuNode(&apListMenu, D_DESELECT_ALL, [this]() { // DESELECT ALL
            accesspoints.deselectAll();
            changeMenu(&apListMenu);
        });
        addMenuNode(&apListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            accesspoints.removeAll();
            goBack();
        });
    });

    // STATION LIST MENU
    createMenu(&stationListMenu, &showMenu, [this]() {
        // add stations to list
        int c = stations.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&stationListMenu, [i]() {
                return b2a(stations.getSelected(i)) +
                (stations.hasName(i) ? stations.getNameStr(i) : stations.getMacVendorStr(i));
            }, [this, i]() {
                stations.getSelected(i) ? stations.deselect(i) : stations.select(i);
            }, [this, i]() {
                selectedID = i;
                changeMenu(&stationMenu);
            });
        }

        addMenuNode(&stationListMenu, D_SELECT_ALL, [this]() { // SELECT ALL
            stations.selectAll();
            changeMenu(&stationListMenu);
        });
        addMenuNode(&stationListMenu, D_DESELECT_ALL, [this]() { // DESELECT ALL
            stations.deselectAll();
            changeMenu(&stationListMenu);
        });
        addMenuNode(&stationListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            stations.removeAll();
            goBack();
        });
    });

    // NAME LIST MENU
    createMenu(&nameListMenu, &showMenu, [this]() {
        // add device names to list
        int c = names.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&nameListMenu, [i]() {
                return names.getSelectedStr(i) + names.getName(i);
            }, [this, i]() {
                names.getSelected(i) ? names.deselect(i) : names.select(i);
            }, [this, i]() {
                selectedID = i;
                changeMenu(&nameMenu);
            });
        }
        addMenuNode(&nameListMenu, D_SELECT_ALL, [this]() { // SELECT ALL
            names.selectAll();
            changeMenu(&nameListMenu);
        });
        addMenuNode(&nameListMenu, D_DESELECT_ALL, [this]() { // DESELECT ALL
            names.deselectAll();
            changeMenu(&nameListMenu);
        });
        addMenuNode(&nameListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            names.removeAll();
            goBack();
        });
    });

    // SSID LIST MENU
    createMenu(&ssidListMenu, &showMenu, [this]() {
        addMenuNode(&ssidListMenu, D_CLONE_APS, [this]() { // CLONE APs
            ssids.cloneSelected(true);
            changeMenu(&ssidListMenu);
            ssids.save(false);
        });
        addMenuNode(&ssidListMenu, [this]() {
            return b2a(ssids.getRandom()) + str(D_RANDOM_MODE); // *RANDOM MODE
        }, [this]() {
            if (ssids.getRandom()) ssids.disableRandom();
            else ssids.enableRandom(10);
            changeMenu(&ssidListMenu);
        });

        // add ssids to list
        int c = ssids.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&ssidListMenu, [i]() {
                return ssids.getName(i).substring(0, ssids.getLen(i));
            }, [this, i]() {
                selectedID = i;
                changeMenu(&ssidMenu);
            }, [this, i]() {
                ssids.remove(i);
                changeMenu(&ssidListMenu);
                ssidListMenu.selected = i;
            });
        }

        addMenuNode(&ssidListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            ssids.removeAll();
            goBack();
        });
    });

    // AP MENU
    createMenu(&apMenu, &apListMenu, [this]() {
        addMenuNode(&apMenu, [this]() {
            return accesspoints.getSelectedStr(selectedID)  + accesspoints.getSSID(selectedID); // *<ssid>
        }, [this]() {
            accesspoints.getSelected(selectedID) ? accesspoints.deselect(selectedID) : accesspoints.select(selectedID);
        });
        addMenuNode(&apMenu, [this]() {
            return str(D_ENCRYPTION) + accesspoints.getEncStr(selectedID);
        }, NULL);                                                                          // Encryption: -/WPA2
        addMenuNode(&apMenu, [this]() {
            return str(D_RSSI) + (String)accesspoints.getRSSI(selectedID);
        }, NULL);                                                                          // RSSI: -90
        addMenuNode(&apMenu, [this]() {
            return str(D_CHANNEL) + (String)accesspoints.getCh(selectedID);
        }, NULL);                                                                          // Channel: 11
        addMenuNode(&apMenu, [this]() {
            return accesspoints.getMacStr(selectedID);
        }, NULL);                                                                          // 00:11:22:00:11:22
        addMenuNode(&apMenu, [this]() {
            return str(D_VENDOR) + accesspoints.getVendorStr(selectedID);
        }, NULL);                                                                          // Vendor: INTEL
        addMenuNode(&apMenu, []() { return String("TRACK"); }, [this]() {                  // TRACK this AP
            startTracker(selectedID);
        });
        addMenuNode(&apMenu, [this]() {
            return accesspoints.getSelected(selectedID) ? str(D_DESELECT) : str(D_SELECT); // SELECT/DESELECT
        }, [this]() {
            accesspoints.getSelected(selectedID) ? accesspoints.deselect(selectedID) : accesspoints.select(selectedID);
        });
        addMenuNode(&apMenu, D_CLONE, [this]() { // CLONE
            ssids.add(accesspoints.getSSID(selectedID), accesspoints.getEnc(selectedID) != ENC_TYPE_NONE, 60, true);
            changeMenu(&showMenu);
            ssids.save(false);
        });
        addMenuNode(&apMenu, D_REMOVE, [this]() { // REMOVE
            accesspoints.remove(selectedID);
            apListMenu.list->remove(apListMenu.selected);
            goBack();
        });
    });

    // STATION MENU
    createMenu(&stationMenu, &stationListMenu, [this]() {
        addMenuNode(&stationMenu, [this]() {
            return stations.getSelectedStr(selectedID) +
            (stations.hasName(selectedID) ? stations.getNameStr(selectedID) : stations.getMacVendorStr(selectedID)); // <station
            // name>
        }, [this]() {
            stations.getSelected(selectedID) ? stations.deselect(selectedID) : stations.select(selectedID);
        });
        addMenuNode(&stationMenu, [this]() {
            return stations.getMacStr(selectedID);
        }, NULL);                                             // 00:11:22:00:11:22
        addMenuNode(&stationMenu, [this]() {
            return str(D_VENDOR) + stations.getVendorStr(selectedID);
        }, NULL);                                             // Vendor: INTEL
        addMenuNode(&stationMenu, [this]() {
            return str(D_AP) + stations.getAPStr(selectedID); // AP: someAP
        }, [this]() {
            int apID = accesspoints.find(stations.getAP(selectedID));

            if (apID >= 0) {
                selectedID = apID;
                changeMenu(&apMenu);
            }
        });
        addMenuNode(&stationMenu, [this]() {
            return str(D_PKTS) + String(*stations.getPkts(selectedID));
        }, NULL);                                                                      // Pkts: 12
        addMenuNode(&stationMenu, [this]() {
            return str(D_CHANNEL) + String(stations.getCh(selectedID));
        }, NULL);                                                                      // Channel: 11
        addMenuNode(&stationMenu, [this]() {
            return str(D_SEEN) + stations.getTimeStr(selectedID);
        }, NULL);                                                                      // Seen: <1min

        addMenuNode(&stationMenu, [this]() {
            return stations.getSelected(selectedID) ? str(D_DESELECT) : str(D_SELECT); // SELECT/DESELECT
        }, [this]() {
            stations.getSelected(selectedID) ? stations.deselect(selectedID) : stations.select(selectedID);
        });
        addMenuNode(&stationMenu, D_REMOVE, [this]() { // REMOVE
            stations.remove(selectedID);
            stationListMenu.list->remove(stationListMenu.selected);
            goBack();
        });
    });

    // NAME MENU
    createMenu(&nameMenu, &nameListMenu, [this]() {
        addMenuNode(&nameMenu, [this]() {
            return names.getSelectedStr(selectedID) + names.getName(selectedID); // <station name>
        }, [this]() {
            names.getSelected(selectedID) ? names.deselect(selectedID) : names.select(selectedID);
        });
        addMenuNode(&nameMenu, [this]() {
            return names.getMacStr(selectedID);
        }, NULL);                                                                   // 00:11:22:00:11:22
        addMenuNode(&nameMenu, [this]() {
            return str(D_VENDOR) + names.getVendorStr(selectedID);
        }, NULL);                                                                   // Vendor: INTEL
        addMenuNode(&nameMenu, [this]() {
            return str(D_AP) + names.getBssidStr(selectedID);
        }, NULL);                                                                   // AP: 00:11:22:00:11:22
        addMenuNode(&nameMenu, [this]() {
            return str(D_CHANNEL) + (String)names.getCh(selectedID);
        }, NULL);                                                                   // Channel: 11

        addMenuNode(&nameMenu, [this]() {
            return names.getSelected(selectedID) ? str(D_DESELECT) : str(D_SELECT); // SELECT/DESELECT
        }, [this]() {
            names.getSelected(selectedID) ? names.deselect(selectedID) : names.select(selectedID);
        });
        addMenuNode(&nameMenu, D_REMOVE, [this]() { // REMOVE
            names.remove(selectedID);
            nameListMenu.list->remove(nameListMenu.selected);
            goBack();
        });
    });

    // SSID MENU
    createMenu(&ssidMenu, &ssidListMenu, [this]() {
        addMenuNode(&ssidMenu, [this]() {
            return ssids.getName(selectedID).substring(0, ssids.getLen(selectedID));
        }, NULL);                                                   // SSID
        addMenuNode(&ssidMenu, [this]() {
            return str(D_ENCRYPTION) + ssids.getEncStr(selectedID); // WPA2
        }, [this]() {
            ssids.setWPA2(selectedID, !ssids.getWPA2(selectedID));
        });
        addMenuNode(&ssidMenu, D_REMOVE, [this]() { // REMOVE
            ssids.remove(selectedID);
            ssidListMenu.list->remove(ssidListMenu.selected);
            goBack();
        });
    });

    // ATTACK MENU
    createMenu(&attackMenu, &mainMenu, [this]() {
        addMenuNode(&attackMenu, [this]() { // *DEAUTH 0/0
            if (attack.isRunning()) return leftRight(b2a(deauthSelected) + str(D_DEAUTH),
                                                     (String)attack.getDeauthPkts() + SLASH +
                                                     (String)attack.getDeauthMaxPkts(), maxLen - 1);
            else return leftRight(b2a(deauthSelected) + str(D_DEAUTH), (String)scan.countSelected(), maxLen - 1);
        }, [this]() { // deauth
            deauthSelected = !deauthSelected;

            if (attack.isRunning()) {
                attack.start(beaconSelected, deauthSelected, false, probeSelected, csaSelected, true,
                             settings::getAttackSettings().timeout * 1000);
            }
        });
        addMenuNode(&attackMenu, [this]() { // *BEACON 0/0
            if (attack.isRunning()) return leftRight(b2a(beaconSelected) + str(D_BEACON),
                                                     (String)attack.getBeaconPkts() + SLASH +
                                                     (String)attack.getBeaconMaxPkts(), maxLen - 1);
            else return leftRight(b2a(beaconSelected) + str(D_BEACON), (String)ssids.count(), maxLen - 1);
        }, [this]() { // beacon
            beaconSelected = !beaconSelected;

            if (attack.isRunning()) {
                attack.start(beaconSelected, deauthSelected, false, probeSelected, csaSelected, true,
                             settings::getAttackSettings().timeout * 1000);
            }
        });
        addMenuNode(&attackMenu, [this]() { // *PROBE 0/0
            if (attack.isRunning()) return leftRight(b2a(probeSelected) + str(D_PROBE),
                                                     (String)attack.getProbePkts() + SLASH +
                                                     (String)attack.getProbeMaxPkts(), maxLen - 1);
            else return leftRight(b2a(probeSelected) + str(D_PROBE), (String)ssids.count(), maxLen - 1);
        }, [this]() { // probe
            probeSelected = !probeSelected;

            if (attack.isRunning()) {
                attack.start(beaconSelected, deauthSelected, false, probeSelected, csaSelected, true,
                             settings::getAttackSettings().timeout * 1000);
            }
        });
        addMenuNode(&attackMenu, [this]() { // *CSA 0/0 - channel switch announcement
            return leftRight(b2a(csaSelected) + "CSA", (String)scan.countSelected(), maxLen - 1);
        }, [this]() { // csa toggle
            csaSelected = !csaSelected;

            if (attack.isRunning()) {
                attack.start(beaconSelected, deauthSelected, false, probeSelected, csaSelected, true,
                             settings::getAttackSettings().timeout * 1000);
            }
        });
        addMenuNode(&attackMenu, [this]() { // START
            return leftRight(str(attack.isRunning() ? D_STOP_ATTACK : D_START_ATTACK),
                             attack.getPacketRate() > 0 ? (String)attack.getPacketRate() : String(), maxLen - 1);
        }, [this]() {
            if (attack.isRunning()) attack.stop();
            else attack.start(beaconSelected, deauthSelected, false, probeSelected, csaSelected, true,
                              settings::getAttackSettings().timeout * 1000);
        });
        addMenuNode(&attackMenu, [this]() { // KILL flood: continuous deauth-all (no timeout)
            if (attack.isRunning() && attack.isKill()) {
                // 4.7: show elapsed mm:ss + delivered pps so a KILL run is measurable
                uint32_t s = attack.getElapsed() / 1000;
                String elapsed = String(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + String(s % 60);
                return leftRight("KILL " + elapsed, (String)attack.getPacketRate(), maxLen - 1);
            }
            return leftRight("KILL", "flood", maxLen - 1);
        }, [this]() {
            if (attack.isRunning()) attack.stop();
            else attack.start(false, true, true, false, false, true, 0); // deauth + deauthAll, continuous
        });
        addMenuNode(&attackMenu, [this]() { // KICK & LURE (plan 4.3)
            // self-heal: if both sides already stopped (timeout / capture done),
            // show idle even when the flag is still set
            bool klLive = kickLureActive && (BlazeEvilTwin::active() || attack.isRunning());
            String right;
            if (klLive && BlazeEvilTwin::active()) {
                right = String(BlazeEvilTwin::attempts());
                if (BlazeEvilTwin::captured().length()) right += ":PW";
                else right += ":...";
            } else if (klLive) {
                right = "kick";
            } else {
                right = "twin";
            }
            return leftRight("KICK", right, maxLen - 1);
        }, [this]() { // toggle kick-and-lure
            bool klLive = kickLureActive && (BlazeEvilTwin::active() || attack.isRunning());
            if (klLive) {
                kickLureActive = false;
                BlazeEvilTwin::stop();
                if (attack.isRunning()) attack.stop();
                return;
            }

            // start: evil twin portal on the first selected AP + timed deauth
            // kick that forces clients off the real AP onto the twin
            int t = -1;
            for (int i = 0; i < accesspoints.count(); i++) {
                if (accesspoints.getSelected(i)) { t = i; break; }
            }
            if (t < 0 && accesspoints.count() > 0) t = 0;
            if (t < 0) return; // no target AP

            if (!BlazeEvilTwin::start(t)) return;
            kickLureActive = true;
            attack.start(false, true, false, false, false, true,
                         settings::getAttackSettings().timeout * 1000);
        });
        addMenuNode(&attackMenu, [this]() { // CAPTURE (plan 4.4): EAPOL handshake capture
            String right;
            if (BlazeRecon::capturing()) {
                right = (BlazeRecon::capturePhase() == 1) ? String("kick")
                                                          : String("REC ") + String(BlazeRecon::eapolFrames());
            } else {
                right = String(BlazeRecon::eapolTotal());
            }
            return leftRight("CAP", right, maxLen - 1);
        }, [this]() { // toggle capture on the first selected AP
            if (BlazeRecon::capturing()) {
                BlazeRecon::stopCapture();
                return;
            }
            int t = -1;
            for (int i = 0; i < accesspoints.count(); i++) {
                if (accesspoints.getSelected(i)) { t = i; break; }
            }
            if (t < 0 && accesspoints.count() > 0) t = 0;
            if (t < 0) return; // no target AP
            BlazeRecon::startCapture(t);
        });
    });

    // FACE WEIGHTS MENU (plan 3.2)
    // click a reaction = +1 weight (0..20, wraps); saved to BlazeCfg on every click
    createMenu(&weightMenu, &mainMenu, [this]() {
        for (uint8_t i = 0; i < BlazeFace::weightCount(); i++) {
            addMenuNode(&weightMenu, [this, i]() {
                return leftRight(String(BlazeFace::reactionName(i)),
                                 String(BlazeFace::getWeight(i)), maxLen - 1);
            }, [i]() {
                BlazeFace::bumpWeight(i);
                BlazeFace::saveWeights();
            });
        }
        addMenuNode(&weightMenu, []() { return String("RESET ALL"); }, []() {
            BlazeFace::resetWeights();
            BlazeFace::saveWeights();
        });
    });

    // CLOCK MENU
    createMenu(&clockMenu, &mainMenu, [this]() {
        addMenuNode(&clockMenu, D_CLOCK_DISPLAY, [this]() { // CLOCK
            mode = DISPLAY_MODE::CLOCK_DISPLAY;
            display.setFont(ArialMT_Plain_24);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
        });
        addMenuNode(&clockMenu, D_CLOCK_SET, [this]() { // CLOCK SET TIME
            mode = DISPLAY_MODE::CLOCK;
            display.setFont(ArialMT_Plain_24);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
        });
    });

    // ===================== //

    // set current menu to main menu
    changeMenu(&mainMenu);
    enabled   = true;
    startTime = currentTime;
}

#ifdef HIGHLIGHT_LED
void DisplayUI::setupLED() {
    pinMode(HIGHLIGHT_LED, OUTPUT);
    digitalWrite(HIGHLIGHT_LED, HIGH);
    highlightLED = true;
}

#endif // ifdef HIGHLIGHT_LED

void DisplayUI::update(bool force) {
    if (!enabled) return;

#ifdef ENABLE_DEBUG
    // raw pin trace: 1 = idle (pull-up), 0 = pressed -> separates wiring faults from software
    {
        static uint32_t rawTime     = 0;
        static int      lastRaw[3]  = { -1, -1, -1 };
        if (currentTime >= rawTime) {
            rawTime = currentTime + 200;
            int raw[3] = {
                (BUTTON_UP < 255)   ? digitalRead(BUTTON_UP)   : -1,
                (BUTTON_DOWN < 255) ? digitalRead(BUTTON_DOWN) : -1,
                (BUTTON_A < 255)    ? digitalRead(BUTTON_A)    : -1
            };
            if (raw[0] != lastRaw[0] || raw[1] != lastRaw[1] || raw[2] != lastRaw[2]) {
                lastRaw[0] = raw[0];
                lastRaw[1] = raw[1];
                lastRaw[2] = raw[2];
                Serial.printf("RAW BTN: UP(D6)=%d DOWN(D5)=%d A(D7)=%d\n", raw[0], raw[1], raw[2]);
            }
        }
    }
#endif // ifdef ENABLE_DEBUG

    checkPanic(); // global UP+DOWN hold = emergency stop (works on any screen)

    up->update();
    down->update();
    a->update();
    b->update();

    draw(force);

    // BLAZE: never time out the loading / face / tracker / twin / detector / panic screens
    if (mode == DISPLAY_MODE::LOADING ||
        mode == DISPLAY_MODE::BLAZE_FACE ||
        mode == DISPLAY_MODE::TRACKER ||
        mode == DISPLAY_MODE::EVILTWIN ||
        mode == DISPLAY_MODE::DETECT ||
        mode == DISPLAY_MODE::PANIC) {
        buttonTime = currentTime;
        return;
    }

    uint32_t timeout = settings::getDisplaySettings().timeout * 1000;
    if (timeout == 0) timeout = 15000; // safety: never blank the menu instantly

    if (currentTime > timeout) {
        if (!tempOff) {
            if (buttonTime < currentTime - timeout) off();
        } else {
            if (buttonTime > currentTime - timeout) on();
        }
    }
}

void DisplayUI::on() {
    if (enabled) {
        configOn();
        tempOff    = false;
        buttonTime = currentTime; // update a button time to keep display on
        BlazePersona::onStandby(false);
        prntln(D_MSG_DISPLAY_ON);
    } else {
        prntln(D_ERROR_NOT_ENABLED);
    }
}

void DisplayUI::off() {
    if (enabled) {
        configOff();
        tempOff = true;
        BlazePersona::onStandby(true);
        prntln(D_MSG_DISPLAY_OFF);
    } else {
        prntln(D_ERROR_NOT_ENABLED);
    }
}

void DisplayUI::setupButtons() {
    up   = new ButtonPullup(BUTTON_UP);
    down = new ButtonPullup(BUTTON_DOWN);
    a    = new ButtonPullup(BUTTON_A);
    b    = new ButtonPullup(BUTTON_B);
    // D3 (BUTTON_MODE) removed from the UI - keep the pin pulled up, no behavior
    pinMode(BUTTON_MODE, INPUT_PULLUP);

    // === BUTTON UP (D5) - menu / tracker / packet monitor / clock === //
    up->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        prntln("BTN UP");
        if (tempOff) on(); // wake AND act in the same press

        if (mode == DISPLAY_MODE::LOADING ||
            mode == DISPLAY_MODE::BLAZE_FACE ||
            mode == DISPLAY_MODE::EVILTWIN ||
            mode == DISPLAY_MODE::PANIC) {
            return; // up/down disabled on the face / twin / panic screens
        }

        if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
            if (currentMenu->selected > 0) currentMenu->selected--;
            else currentMenu->selected = currentMenu->list->size() - 1;
        } else if (mode == DISPLAY_MODE::TRACKER) {       // pick: scroll up | track: switch target AP
            if (BlazeTracker::picking()) BlazeTracker::pickMove(-1);
            else BlazeTracker::selectPrev();
        } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
            scan.setChannel(wifi_channel + 1);
        } else if (mode == DISPLAY_MODE::CLOCK) {         // when in clock, change time
            setTime(clockHour, clockMinute + 1, clockSecond);
        } else if (mode == DISPLAY_MODE::PROBELOG) {      // scroll probe list up
            if (probeScroll > 0) probeScroll--;
        } else if (mode == DISPLAY_MODE::CHAT) {          // phrase wheel
            BlazeChat::move(-1);
        } else if (mode == DISPLAY_MODE::DETECT) {        // scroll findings up
            int maxSel = BlazeDetector::findingCount() - 1;
            if (maxSel < 0) maxSel = 0;
            if (detectSel > maxSel) detectSel = maxSel;
            if (detectSel > 0) detectSel--;
        }
    });

    up->setOnHolding([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (tempOff) on();
        if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
            if (currentMenu->selected > 0) currentMenu->selected--;
            else currentMenu->selected = currentMenu->list->size() - 1;
        } else if (mode == DISPLAY_MODE::TRACKER) {
            if (BlazeTracker::picking()) BlazeTracker::pickMove(-1);
            else BlazeTracker::selectPrev();
        } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
            scan.setChannel(wifi_channel + 1);
        } else if (mode == DISPLAY_MODE::CLOCK) {         // when in clock, change time
            setTime(clockHour, clockMinute + 10, clockSecond);
        } else if (mode == DISPLAY_MODE::PROBELOG) {      // scroll probe list up
            if (probeScroll > 0) probeScroll--;
        } else if (mode == DISPLAY_MODE::CHAT) {          // phrase wheel (hold-repeat)
            BlazeChat::move(-1);
        } else if (mode == DISPLAY_MODE::DETECT) {        // scroll findings up (hold-repeat)
            int maxSel = BlazeDetector::findingCount() - 1;
            if (maxSel < 0) maxSel = 0;
            if (detectSel > maxSel) detectSel = maxSel;
            if (detectSel > 0) detectSel--;
        }
    }, buttonDelay);

    // === BUTTON DOWN (D6) - menu / tracker / packet monitor / clock === //
    down->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        prntln("BTN DOWN");
        if (tempOff) on(); // wake AND act in the same press

        if (mode == DISPLAY_MODE::LOADING ||
            mode == DISPLAY_MODE::BLAZE_FACE ||
            mode == DISPLAY_MODE::EVILTWIN ||
            mode == DISPLAY_MODE::PANIC) {
            return; // up/down disabled on the face / twin / panic screens
        }

        if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
            if (currentMenu->selected < currentMenu->list->size() - 1) currentMenu->selected++;
            else currentMenu->selected = 0;
        } else if (mode == DISPLAY_MODE::TRACKER) {       // pick: scroll down | track: switch target AP
            if (BlazeTracker::picking()) BlazeTracker::pickMove(1);
            else BlazeTracker::selectNext();
        } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
            scan.setChannel(wifi_channel - 1);
        } else if (mode == DISPLAY_MODE::CLOCK) {         // when in clock, change time
            setTime(clockHour, clockMinute - 1, clockSecond);
        } else if (mode == DISPLAY_MODE::PROBELOG) {      // scroll probe list down
            int maxScroll = BlazeRecon::probeCount() - 1;
            if (probeScroll < maxScroll) probeScroll++;
        } else if (mode == DISPLAY_MODE::CHAT) {          // phrase wheel
            BlazeChat::move(1);
        } else if (mode == DISPLAY_MODE::DETECT) {        // scroll findings down
            int maxSel = BlazeDetector::findingCount() - 1;
            if (maxSel < 0) maxSel = 0;
            if (detectSel < maxSel) detectSel++;
        }
    });

    down->setOnHolding([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (tempOff) on();
        if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
            if (currentMenu->selected < currentMenu->list->size() - 1) currentMenu->selected++;
            else currentMenu->selected = 0;
        } else if (mode == DISPLAY_MODE::TRACKER) {
            if (BlazeTracker::picking()) BlazeTracker::pickMove(1);
            else BlazeTracker::selectNext();
        } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
            scan.setChannel(wifi_channel - 1);
        } else if (mode == DISPLAY_MODE::CLOCK) {         // when in clock, change time
            setTime(clockHour, clockMinute - 10, clockSecond);
        } else if (mode == DISPLAY_MODE::PROBELOG) {      // scroll probe list down
            int maxScroll = BlazeRecon::probeCount() - 1;
            if (probeScroll < maxScroll) probeScroll++;
        } else if (mode == DISPLAY_MODE::CHAT) {          // phrase wheel (hold-repeat)
            BlazeChat::move(1);
        } else if (mode == DISPLAY_MODE::DETECT) {        // scroll findings down (hold-repeat)
            int maxSel = BlazeDetector::findingCount() - 1;
            if (maxSel < 0) maxSel = 0;
            if (detectSel < maxSel) detectSel++;
        }
    }, buttonDelay);

    // === BUTTON A / SELECT (D7) - menu hold restored, mode switching stays click-only === //
    // menu: click = activate item, hold 800ms = item's hold action (opens AP/station details)
    // tracker: hold >=2s = enter calibrate / cancel (click inside calibrate = save)
    a->setOnHolding([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (tempOff) return; // wake happens through the click/timeout path

        if (mode == DISPLAY_MODE::MENU) {
            if (currentMenu->list->get(currentMenu->selected).hold) {
                currentMenu->list->get(currentMenu->selected).hold();
            }
        } else if (mode == DISPLAY_MODE::TRACKER) {
            uint32_t held = a->getPushTime();
            if (BlazeTracker::picking()) {
                // pick step: hold SELECT = cancel back to menu
                if (held >= 2000 && held < 2900) {
                    BlazeTracker::pickCancel();
                    mode = DISPLAY_MODE::MENU;
                    restoreMenuFont();
                }
            } else if (held >= 2000 && held < 2900) {
                BlazeTracker::selectHold(); // fires once per press
            }
        } else if (mode == DISPLAY_MODE::CHAT) {
            cycleMode(); // A hold: leave CHAT -> MENU (click sends instead)
        } else if (mode == DISPLAY_MODE::DETECT) {
            // active probe: hold SELECT = prove the selected finding
            if (BlazeDetector::findingCount() > 0) {
                if (!BlazeDetector::confirm(detectSel)) flashMsg(String(F("cant prove.")), 1500);
            }
        }
    }, 800);

    // === BUTTON A / SELECT (D7) - click: menu activate, tracker exit/save === //
    a->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        prntln("BTN A");
        if (tempOff) on(); // wake AND act in the same press

        switch (mode) {
            case DISPLAY_MODE::MENU:
                if (currentMenu->list->get(currentMenu->selected).click) {
                    currentMenu->list->get(currentMenu->selected).click();
                }
                break;

            case DISPLAY_MODE::PACKETMONITOR:
            case DISPLAY_MODE::LOADSCAN:
                scan.stop();
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            case DISPLAY_MODE::CLOCK:
            case DISPLAY_MODE::CLOCK_DISPLAY:
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            // tracker pick: SELECT = confirm highlighted AP and start tracking
            case DISPLAY_MODE::TRACKER:
                if (BlazeTracker::picking()) {
                    if (BlazeTracker::pickConfirm()) {
                        restoreMenuFont();
                        buttonTime = currentTime;
                    } else {
                        // nothing to track (empty list) - leave pick
                        mode = DISPLAY_MODE::MENU;
                        restoreMenuFont();
                    }
                } else if (BlazeTracker::calibrating()) {
                    BlazeTracker::calibrateSave();
                } else {
                    BlazeTracker::stop();
                    mode = DISPLAY_MODE::MENU;
                    restoreMenuFont();
                    buttonTime = currentTime;
                }
                break;

            // twin: re-click SELECT stops
            case DISPLAY_MODE::EVILTWIN:
                BlazeEvilTwin::stop();
                if (kickLureActive) { // leaving the screen also ends the kick
                    kickLureActive = false;
                    if (attack.isRunning()) attack.stop();
                }
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                buttonTime = currentTime;
                break;

            // detector: SELECT stops the run / restarts it when idle
            case DISPLAY_MODE::DETECT:
                if (BlazeDetector::running() || BlazeDetector::confirming()) {
                    BlazeDetector::stop();
                } else if (BlazeDetector::start()) {
                    detectSel = 0;
                } else {
                    flashMsg(String(F("air busy.")), 1500);
                }
                buttonTime = currentTime;
                break;

            // recon screens: SELECT = back to menu
            case DISPLAY_MODE::PROBELOG:
                if (probeLogSniff && scan.isSniffing()) scan.stop();
                probeLogSniff = false;
                mode          = DISPLAY_MODE::MENU;
                restoreMenuFont();
                buttonTime = currentTime;
                break;

            case DISPLAY_MODE::VULN:
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                buttonTime = currentTime;
                break;

            // BLAZE screens: click = next mode (FACE -> CHAT -> MENU -> FACE ...)
            case DISPLAY_MODE::LOADING:
            case DISPLAY_MODE::BLAZE_FACE:
                cycleMode();
                break;

            // CHAT: click = send the highlighted phrase (hold = leave)
            case DISPLAY_MODE::CHAT:
                BlazeChat::send();
                break;

            // PANIC: any press returns to the menu
            case DISPLAY_MODE::PANIC:
                cycleModeToMenu();
                break;

            default:
                break;
        }
    });

    // === BUTTON B (back) === //
    b->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        prntln("BTN B");
        if (tempOff) on(); // wake AND act in the same press

        switch (mode) {
            case DISPLAY_MODE::MENU:
                goBack();
                break;

            case DISPLAY_MODE::PACKETMONITOR:
            case DISPLAY_MODE::LOADSCAN:
                scan.stop();
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            case DISPLAY_MODE::CLOCK:
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            case DISPLAY_MODE::TRACKER:
                if (BlazeTracker::picking()) BlazeTracker::pickCancel();
                else BlazeTracker::stop();
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            case DISPLAY_MODE::EVILTWIN:
                BlazeEvilTwin::stop();
                if (kickLureActive) { // leaving the screen also ends the kick
                    kickLureActive = false;
                    if (attack.isRunning()) attack.stop();
                }
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            case DISPLAY_MODE::DETECT:
                BlazeDetector::stop();
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            case DISPLAY_MODE::PROBELOG:
                if (probeLogSniff && scan.isSniffing()) scan.stop();
                probeLogSniff = false;
                mode          = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

            case DISPLAY_MODE::VULN:
                mode = DISPLAY_MODE::MENU;
                restoreMenuFont();
                break;

                // BLAZE screens: B = back to menu
                case DISPLAY_MODE::LOADING:
                case DISPLAY_MODE::BLAZE_FACE:
                case DISPLAY_MODE::CHAT:
                case DISPLAY_MODE::PANIC:
                    cycleModeToMenu();
                    break;
        }
    });
}

// ===== BLAZE: SELECT(D7) click: FACE -> CHAT -> MENU ; "MODE" menu item: MENU -> FACE ===== //
void DisplayUI::cycleMode() {
    scrollCounter = 0;
    scrollTime    = currentTime;
    buttonTime    = currentTime;

    switch (mode) {
        case DISPLAY_MODE::LOADING:
        case DISPLAY_MODE::INTRO:
            mode = DISPLAY_MODE::BLAZE_FACE;
            BlazeFace::startFace();
            break;

        case DISPLAY_MODE::BLAZE_FACE:
            mode = DISPLAY_MODE::CHAT; // FACE -> CHAT
            break;

        case DISPLAY_MODE::CHAT:
            cycleModeToMenu(); // CHAT -> MENU (A hold or B)
            break;

        default: // MENU, CLOCK, PACKETMONITOR, ... -> back to BLAZE face loop
            if (mode == DISPLAY_MODE::PACKETMONITOR || mode == DISPLAY_MODE::LOADSCAN) scan.stop();
            if (mode == DISPLAY_MODE::PROBELOG) {
                if (probeLogSniff && scan.isSniffing()) scan.stop();
                probeLogSniff = false;
            }
            mode = DISPLAY_MODE::BLAZE_FACE;
            BlazeFace::startFace();
            break;
    }
}

void DisplayUI::cycleModeToMenu() {
    mode = DISPLAY_MODE::MENU;
    blazeFlash = false;
    restoreMenuFont();
    buttonTime = currentTime;
}

void DisplayUI::restoreMenuFont() {
    display.setFont(DejaVu_Sans_Mono_12);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
}

// ===== BLAZE: KILL / PANIC - global emergency stop (hold UP + DOWN) ===== //
// Detected in update(): holding both UP and DOWN for 1.5s on any screen
// stops every running action and shows the PANIC screen.
void DisplayUI::checkPanic() {
    static uint32_t holdStart = 0;
    static bool     holding   = false;
    static bool     fired     = false;

    bool both = (BUTTON_UP < 255) && (BUTTON_DOWN < 255) &&
                digitalRead(BUTTON_UP) == LOW && digitalRead(BUTTON_DOWN) == LOW;

    if (both) {
        if (fired) return; // already triggered - wait for release
        if (!holding) {
            holding   = true;
            holdStart = currentTime;
        } else if (currentTime - holdStart >= 1500) {
            fired   = true;
            holding = false;
            panicStop();
        }
    } else {
        holding = false;
        fired   = false;
    }
}

// stop everything at once, then show the PANIC screen briefly
void DisplayUI::panicStop() {
    if (attack.isRunning()) attack.stop();
    if (scan.isScanning())  scan.stop();
    if (BlazeRecon::capturing()) BlazeRecon::stopCapture();
    BlazeTracker::stop();
    BlazeEvilTwin::stop();
    BlazeDetector::stop();
    kickLureActive = false;
    prntln("PANIC: all attacks stopped");

    restoreMenuFont();
    panicUntil = currentTime + 2500; // auto-dismiss back to the menu
    mode       = DISPLAY_MODE::PANIC;
    buttonTime = currentTime;
    if (tempOff) on();

    BlazePersona::onEvent(BlazePersona::EV_PANIC); // line only - flash skips the panic screen
}

void DisplayUI::drawPanic() {
    restoreMenuFont();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 6, "PANIC");
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 38, "ALL STOPPED");
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    if (currentTime > panicUntil) {
        currentMenu         = &mainMenu;
        mainMenu.selected   = 0;
        mode                = DISPLAY_MODE::MENU;
        restoreMenuFont();
    }
}

// ===== BLAZE: refused login -> persona interjection ===== //
void DisplayUI::flashNoLogin() {
    BlazePersona::onEvent(BlazePersona::EV_LOGIN_FAIL);
}

// ===== BLAZE: short flash message on the face screen ===== //
void DisplayUI::flashMsg(const String& txt, uint32_t ms) {
    if (!enabled) return;

    if (tempOff) on();

    if (!blazeFlash) blazeFlashPrev = mode;
    blazeFlash = true;

    mode = DISPLAY_MODE::BLAZE_FACE;
    BlazeFace::startFace(); // fresh phase FACE (skips loading/boot)
    BlazeFace::flashMessage(txt, ms);
}

// ===== BLAZE: enter tracker mode ===== //
// idx < 0 -> open the target picker (must choose an AP before tracking)
// idx >= 0 -> track that AP directly
void DisplayUI::startTracker(int idx) {
    if (idx >= 0) {
        if (!BlazeTracker::start(idx)) return;
    } else {
        BlazeTracker::beginPick();
        if (BlazeTracker::pickCount() <= 0) return; // no APs scanned yet
    }

    mode = DISPLAY_MODE::TRACKER;
    restoreMenuFont();
    buttonTime = currentTime;
}

// ===== BLAZE: enter evil twin mode ===== //
void DisplayUI::startTwin(int idx) {
    int t = idx;
    int c = accesspoints.count();
    if (t < 0 || t >= c) {
        t = -1;
        for (int i = 0; i < c; i++) {
            if (accesspoints.getSelected(i)) { t = i; break; }
        }
        if (t < 0 && c > 0) t = 0;
    }
    if (t < 0) { // nothing to clone - tell the operator instead of doing nothing
        prntln(String(F("TWIN: no APs scanned")));
        flashMsg(String(F("scan first.")), 2000);
        return;
    }
    if (!BlazeEvilTwin::start(t)) { // refused (protected/hidden/already active)
        prntln(String(F("TWIN: start refused")));
        flashMsg(String(F("twin refused.")), 2000);
        return;
    }

    // full workflow: push clients off the real AP onto the twin (same kick as KICK&LURE)
    kickLureActive = true;
    attack.start(false, true, false, false, false, true,
                 settings::getAttackSettings().timeout * 1000);

    mode = DISPLAY_MODE::EVILTWIN;
    restoreMenuFont();
    buttonTime = currentTime;
}

String DisplayUI::getChannel() {
    String ch = String(wifi_channel);

    if (ch.length() < 2) ch = ' ' + ch;
    return ch;
}

void DisplayUI::draw(bool force) {
    if (force || ((currentTime - drawTime > drawInterval) && currentMenu)) {
        drawTime = currentTime;

        updatePrefix();

#ifdef RTC_DS3231
        bool h12;
        bool PM_time;
        clockHour   = clock.getHour(h12, PM_time);
        clockMinute = clock.getMinute();
        clockSecond = clock.getSecond();
#else // ifdef RTC_DS3231
        if (currentTime - clockTime >= 1000) {
            setTime(clockHour, clockMinute, ++clockSecond);
            clockTime += 1000;
        }
#endif // ifdef RTC_DS3231

        switch (mode) {
            case DISPLAY_MODE::BUTTON_TEST:
                drawButtonTest();
                break;

            case DISPLAY_MODE::MENU:
                drawMenu();
                break;

            case DISPLAY_MODE::LOADSCAN:
                drawLoadingScan();
                break;

            case DISPLAY_MODE::PACKETMONITOR:
                drawPacketMonitor();
                break;

            case DISPLAY_MODE::INTRO:
                if (!scan.isScanning() && (currentTime - startTime >= screenIntroTime)) {
                    mode = DISPLAY_MODE::MENU;
                }
                drawIntro();
                break;
            case DISPLAY_MODE::CLOCK:
            case DISPLAY_MODE::CLOCK_DISPLAY:
                drawClock();
                break;
            case DISPLAY_MODE::RESETTING:
                drawResetting();
                break;

            // ===== BLAZE ===== //
            case DISPLAY_MODE::LOADING:
                drawBlazeLoading();
                break;
            case DISPLAY_MODE::BLAZE_FACE:
                drawBlazeFace();
                break;
            case DISPLAY_MODE::CHAT:
                BlazeChat::render(&display);
                break;
            case DISPLAY_MODE::TRACKER:
                if (BlazeTracker::picking()) drawTrackerPick();
                else drawTracker();
                break;
            case DISPLAY_MODE::EVILTWIN:
                drawEvilTwin();
                break;
            case DISPLAY_MODE::DETECT:
                drawDetect();
                break;
            case DISPLAY_MODE::PROBELOG:
                drawProbeLog();
                break;
            case DISPLAY_MODE::VULN:
                drawVuln();
                break;
            case DISPLAY_MODE::PANIC:
                drawPanic();
                break;
        }

        // glitch screen-tear overlay (attack finished) on top of the scene
        if (BlazeFx::glitchActive()) BlazeFx::glitchDraw(&display);

        updateSuffix();
    }
}

// ===== BLAZE draw functions ===== //
void DisplayUI::drawBlazeLoading() {
    if (BlazeFace::flashActive()) {
        BlazeFace::renderFlash();
    } else {
        BlazeFace::renderFace(); // loading screen + wake-up animation
    }

    if (BlazeFace::bootDone() && !blazeFlash) {
        mode = DISPLAY_MODE::BLAZE_FACE;
    }
}

void DisplayUI::drawBlazeFace() {
    if (blazeFlash) {
        BlazeFace::renderFlash();
        if (!BlazeFace::flashActive()) {
            // flash over - return to the mode we came from
            blazeFlash = false;
            DISPLAY_MODE prev = blazeFlashPrev;
            mode = prev;
            if (prev == DISPLAY_MODE::MENU) restoreMenuFont();
            if (prev == DISPLAY_MODE::LOADING && !BlazeFace::bootDone()) mode = DISPLAY_MODE::LOADING;
        }
        return;
    }

    BlazeFace::renderFace();
}

// ===== BLAZE: TRACKER - pick a target AP before tracking starts ===== //
void DisplayUI::drawTrackerPick() {
    restoreMenuFont();

    int c = BlazeTracker::pickCount();

    // header: title + count
    drawString(0, leftRight(String("PICK TARGET"), String(c), maxLen - 1));

    if (c <= 0) {
        drawString(2, center("no APs found", maxLen));
        drawString(3, center("run a SCAN first", maxLen));
        drawString(4, center("B = back", maxLen));
        return;
    }

    // list: 3 rows, highlight centered when possible
    const uint8_t visible = 3;
    int sel = BlazeTracker::pickIndex();
    int row = sel - 1; // keep highlight on the middle row
    if (row < 0) row = 0;
    if (row > c - visible) row = c - visible;
    if (row < 0) row = 0;

    for (uint8_t k = 0; k < visible; k++) {
        int i = row + k;
        if (i >= c) break;

        String ssid = BlazeTracker::pickSsid(i);
        if (ssid.length() > 12) ssid = ssid.substring(0, 12);

        String line;
        if (i == sel) line = String('>') + ssid;
        else          line = String(' ') + ssid;

        // right side: rssi + channel
        String right = String(BlazeTracker::pickRssi(i)) + " " + String(BlazeTracker::pickCh(i));

        drawString(1 + k, leftRight(line, right, maxLen - 1));
    }

    // footer hints
    drawString(4, center("A=pick  HOLD=back", maxLen));
}

// ===== BLAZE: TRACKER - hunt an AP with gauge / distance / sparkline ===== //
void DisplayUI::drawTracker() {
    restoreMenuFont();

    String ssid = BlazeTracker::targetSsid();
    if (ssid.length() > 12) ssid = ssid.substring(0, 12);

    // ---- calibrate state ---- //
    if (BlazeTracker::calibrating()) {
        drawString(0, String("TGT ") + ssid);
        drawString(1, center("CALIBRATE @ 1m", maxLen));
        drawString(2, String("TXP ") + String(BlazeTracker::txPower(), 1));
        drawString(3, center("A = SAVE", maxLen));
        drawString(4, center("HOLD = CANCEL", maxLen));
        return;
    }

    // row 0: target + warm/cold direction
    {
        const char* arrow = "STEADY";
        if (BlazeTracker::direction() > 0) arrow = "CLOSER";
        else if (BlazeTracker::direction() < 0) arrow = "FARTHER";
        drawString(0, leftRight(String("TGT ") + ssid, String(arrow), maxLen - 1));
    }

    // row 1: raw signal + gauge bar
    if (BlazeTracker::hasSignal()) {
        drawString(1, String(BlazeTracker::rssi()) + "dBm");
        int q = constrain(BlazeTracker::rssi(), -90, -30);
        display.drawProgressBar(50, 15, 76, 7, map(q, -90, -30, 0, 100));
    } else {
        drawString(1, scan.isScanning() ? String("SCANNING...") : String("--dBm"));
    }

    // row 2: distance estimate
    {
        float d = BlazeTracker::distance();
        String dstr = (d > 0 ? String(d, 1) : String("--")) + "m";
        drawString(2, leftRight(String("DIST ") + dstr,
                                String("MIN ") + (BlazeTracker::minDistance() > 0 ? String(BlazeTracker::minDistance(), 1) : String("--")),
                                maxLen - 1));
    }

    // row 3: rssi history sparkline
    {
        uint8_t n = BlazeTracker::histCount();
        if (n >= 2) {
            int prevX = -1;
            int prevY = -1;
            for (uint8_t i = 0; i < n; i++) {
                int v = constrain(BlazeTracker::histAt(i), -95, -30);
                int x = (int)i * 127 / (n - 1);
                int y = map(v, -95, -30, 55, 37); // stronger = higher
                if (prevX >= 0) display.drawLine(prevX, prevY, x, y);
                else display.drawVerticalLine(x, y, 1);
                prevX = x;
                prevY = y;
            }
        } else {
            drawString(3, center("no signal. move.", maxLen));
        }
    }

    // row 4: env factor + status
    {
        String status;
        if (BlazeTracker::lost()) status = ((currentTime / 400) % 2) ? String("LOST!") : String("LOST");
        else if (scan.isScanning()) status = String("SCAN");
        else status = String("TRACKING");
        drawString(4, leftRight(String("ENV ") + String(BlazeTracker::envFactor(), 1), status, maxLen - 1));
    }
}

// ===== BLAZE: EVIL TWIN - open AP + captive portal ===== //
void DisplayUI::drawEvilTwin() {
    restoreMenuFont();
    String ssid = BlazeEvilTwin::targetSsid();
    if (ssid.length() > 14) ssid = ssid.substring(0, 14);

    drawString(0, String("TWIN ") + ssid);

    String user = BlazeEvilTwin::capturedUser();
    String pw   = BlazeEvilTwin::captured();

    if (pw.length() > 0) {
        if (user.length() > 0) {
            // google template: email + password captured immediately
            if (user.length() > 13) user = user.substring(0, 13);
            drawString(1, String(F("GOT: ")) + user);
            if (pw.length() > 14) pw = pw.substring(0, 14);
            drawString(2, String(F("PW: ")) + pw);
            drawString(3, String(F("TRIES ")) + String(BlazeEvilTwin::attempts()));
        } else {
            if (pw.length() > 12) pw = pw.substring(0, 12);
            drawString(1, String(F("GOT: ")) + pw);
            drawString(2, center("password found!", maxLen));
            drawString(3, center("open network", maxLen));
        }
    } else if (BlazeEvilTwin::validatingNow()) {
        drawString(1, center("trying password...", maxLen));
        drawString(2, String("TRIES ") + String(BlazeEvilTwin::attempts()));
        drawString(3, center("open network", maxLen));
    } else {
        drawString(1, center("waiting for victim", maxLen));
        drawString(2, String("TRIES ") + String(BlazeEvilTwin::attempts()));
        drawString(3, center("open network", maxLen));
    }

    drawString(4, center("SELECT = STOP", maxLen));
}

// ===== BLAZE: DETECTOR - deauther / fake-beacon hunter ===== //
void DisplayUI::drawDetect() {
    restoreMenuFont();

    // row 0: header + live status
    String status;
    if (BlazeDetector::confirming()) status = String(F("proving"));
    else if (BlazeDetector::running()) {
        int p = BlazeDetector::phase();
        status = (p == 1) ? String(F("scan")) : (p == 2 ? String(F("sniff")) : String(F("work")));
    } else {
        status = String(F("n=")) + String(BlazeDetector::findingCount());
    }
    drawString(0, leftRight(String(F("DETECTOR")), status, maxLen));

    // running / active proof: the listen screen (never blanks mid-run)
    if (BlazeDetector::running() || BlazeDetector::confirming()) {
        if (BlazeDetector::confirming()) drawString(1, center("proving target", maxLen));
        else drawString(1, center("listening to air", maxLen));
        drawString(2, String(F("ch ")) + String(wifi_channel));
        drawString(3, center("SELECT = stop", maxLen));
        drawString(4, String(F("phase ")) + String(BlazeDetector::phase()));
        return;
    }

    int n = BlazeDetector::findingCount();

    // idle + clean air: nothing to show but the honest verdict
    if (n <= 0) {
        drawString(1, center("clean air.", maxLen));
        drawString(2, center("nothing heard", maxLen));
        drawString(3, center("SELECT = scan", maxLen));
        drawString(4, center("no targets", maxLen));
        return;
    }

    // idle + findings: 3-line window around the selection
    if (detectSel > n - 1) detectSel = n - 1;
    if (detectSel < 0) detectSel = 0;

    int scroll = 0;
    if (n > 3) {
        scroll = detectSel - 1;
        if (scroll > n - 3) scroll = n - 3;
        if (scroll < 0) scroll = 0;
    }

    for (int k = 0; k < 3; k++) {
        int    i = scroll + k;
        String line;
        if (i < n) {
            line = (i == detectSel ? String('>') : String(' ')) + BlazeDetector::findingLine(i);
            if (line.length() > maxLen) line = line.substring(0, maxLen);
        }
        drawString(1 + k, line);
    }

    // row 4: measured evidence behind the selected verdict (always shown)
    String detail = BlazeDetector::findingDetail(detectSel);
    if (detail.length() > maxLen) detail = detail.substring(0, maxLen);
    drawString(4, detail);
}

// ===== BLAZE: probe-request log screen (plan 4.5) ===== //
void DisplayUI::drawProbeLog() {
    restoreMenuFont();

    int n = BlazeRecon::probeCount();
    drawString(0, "PROBES " + String(n) + " tot " + String(BlazeRecon::probeTotal()));

    if (n == 0) {
        drawString(2, center("waiting for probes", maxLen));
        drawString(4, center("B = back", maxLen));
        return;
    }

    // clamp scroll: 3 visible rows (1..3), header row 0, footer row 4
    int maxScroll = n - 3;
    if (maxScroll < 0) maxScroll = 0;
    if (probeScroll > maxScroll) probeScroll = maxScroll;
    if (probeScroll < 0) probeScroll = 0;

    for (int r = 0; r < 3; r++) {
        int i = probeScroll + r;
        if (i >= n) break;
        drawString(r + 1, BlazeRecon::probeLine(i));
    }

    drawString(4, center("UP/DN B=back", maxLen));
}

// ===== BLAZE: vulnerability score screen (plan 4.6) ===== //
void DisplayUI::drawVuln() {
    restoreMenuFont();

    drawString(0, "VULN SCORE " + String(BlazeRecon::riskScore()));
    drawString(1, center(String(BlazeRecon::riskLevel()), maxLen));

    int w = BlazeRecon::worstAp();
    if (w >= 0) {
        String ssid = accesspoints.getSSID(w);
        if (ssid.length() > 16) ssid = ssid.substring(0, 16);
        drawString(2, center("worst:", maxLen));
        drawString(3, center(ssid, maxLen));
    } else {
        drawString(2, center("no APs. scan now.", maxLen));
    }

    drawString(4, center("B = back", maxLen));
}

void DisplayUI::drawButtonTest() {
    drawString(0, str(D_UP) + b2s(up->read()));
    drawString(1, str(D_DOWN) + b2s(down->read()));
    drawString(2, str(D_A) + b2s(a->read()));
    drawString(3, str(D_B) + b2s(b->read()));
}

void DisplayUI::drawMenu() {
    restoreMenuFont(); // BLAZE: chat/flash screens use other fonts
    String tmp;
    int    tmpLen;
    int    row = (currentMenu->selected / 5) * 5;

    // BLAZE: matrix rain background (text strips are cleared below)
    bool rain = BlazeFx::rainEnabled();
    if (rain) BlazeFx::rainDraw(&display);

    // correct selected if it's off
    if (currentMenu->selected < 0) currentMenu->selected = 0;
    else if (currentMenu->selected >= currentMenu->list->size()) currentMenu->selected = currentMenu->list->size() - 1;

    // draw menu entries
    for (int i = row; i < currentMenu->list->size() && i < row + 5; i++) {
        tmp    = currentMenu->list->get(i).getStr();
        tmpLen = tmp.length();

        // horizontal scrolling
        if ((currentMenu->selected == i) && (tmpLen >= maxLen)) {
            tmp = tmp + tmp;
            tmp = tmp.substring(scrollCounter, scrollCounter + maxLen - 1);

            if (((scrollCounter > 0) && (scrollTime < currentTime - scrollSpeed)) || ((scrollCounter == 0) && (scrollTime < currentTime - scrollSpeed * 4))) {
                scrollTime = currentTime;
                scrollCounter++;
            }

            if (scrollCounter > tmpLen) scrollCounter = 0;
        }

        tmp = (currentMenu->selected == i ? CURSOR : SPACE) + tmp;

        if (rain) {
            // punch a black strip behind the text so the rain stays readable
            int w = display.getStringWidth(tmp) + 3;
            if (w > screenWidth) w = screenWidth;
            display.setColor(BLACK);
            display.fillRect(0, (i - row) * 12, w, 12);
            display.setColor(WHITE);
        }

        drawString(0, (i - row) * 12, tmp);
    }
}

void DisplayUI::drawLoadingScan() {
    String percentage;

    if (scan.isScanning()) {
        percentage = String(scan.getPercentage()) + '%';
    } else {
        percentage = str(DSP_SCAN_DONE);
    }

    drawString(0, leftRight(str(DSP_SCAN_FOR), scan.getMode(), maxLen));
    drawString(1, leftRight(str(DSP_APS), String(accesspoints.count()), maxLen));
    drawString(2, leftRight(str(DSP_STS), String(stations.count()), maxLen));
    drawString(3, leftRight(str(DSP_PKTS), String(scan.getPacketRate()) + str(DSP_S), maxLen));
    drawString(4, percentage);

    // BLAZE: walking dot progress track (bottom right strip)
    {
        bool scanning = scan.isScanning();
        int  walker;

        if (!scanning) {
            walker = 126; // done - parked at the end
        } else {
            int pct = scan.getPercentage();
            if (pct > 0) {
                walker = 68 + pct * 58 / 100;
            } else {
                // AP scans report 0% - sweep ping-pong so the dot stays alive
                int t = (currentTime / 60) % 118;
                walker = 68 + (t < 59 ? t : 118 - t);
            }
        }

        // ticks lit up to the walker
        for (int x = 68; x <= 126; x += 8) {
            if (scanning && x > walker) break;
            display.drawVerticalLine(x, 60, 3);
        }

        // the walker itself (blinks while scanning)
        if (!scanning || ((currentTime / 250) % 2)) {
            display.fillRect(walker - 1, 59, 3, 4);
        }
    }
}

void DisplayUI::drawPacketMonitor() {
    double scale = scan.getScaleFactor(sreenHeight - lineHeight - 2);

    String headline = leftRight(str(D_CH) + getChannel() + String(' ') + String('[') + String(scan.deauths) + String(']'), String(scan.getPacketRate()) + str(D_PKTS), maxLen);

    drawString(0, 0, headline);

    if (scan.getMaxPacket() > 0) {
        int i = 0;
        int x = 0;
        int y = 0;

        while (i < SCAN_PACKET_LIST_SIZE && x < screenWidth) {
            y = (sreenHeight-1) - (scan.getPackets(i) * scale);
            i++;

            // Serial.printf("%d,%d -> %d,%d\n", x, (sreenHeight-1), x, y);
            drawLine(x, (sreenHeight-1), x, y);
            x++;

            // Serial.printf("%d,%d -> %d,%d\n", x, (sreenHeight-1), x, y);
            drawLine(x, (sreenHeight-1), x, y);
            x++;
        }
        // Serial.println("---------");
    }
}

void DisplayUI::drawIntro() {
    drawString(0, center(str(D_INTRO_0), maxLen));
    drawString(1, center(str(D_INTRO_1), maxLen));
    drawString(2, center(str(D_INTRO_2), maxLen));
    drawString(3, center(DEAUTHER_VERSION, maxLen));
    if (scan.isScanning()) {
        if (currentTime - startTime >= screenIntroTime+4500) drawString(4, left(str(D_SCANNING_3), maxLen));
        else if (currentTime - startTime >= screenIntroTime+3000) drawString(4, left(str(D_SCANNING_2), maxLen));
        else if (currentTime - startTime >= screenIntroTime+1500) drawString(4, left(str(D_SCANNING_1), maxLen));
        else if (currentTime - startTime >= screenIntroTime) drawString(4, left(str(D_SCANNING_0), maxLen));
    }
}

void DisplayUI::drawClock() {
    String clockTime = String(clockHour);

    clockTime += ':';
    if (clockMinute < 10) clockTime += '0';
    clockTime += String(clockMinute);

    display.drawString(64, 20, clockTime);
}

void DisplayUI::drawResetting() {
    drawString(2, center(str(D_RESETTING), maxLen));
}

void DisplayUI::clearMenu(Menu* menu) {
    while (menu->list->size() > 0) {
        menu->list->remove(0);
    }
}

void DisplayUI::changeMenu(Menu* menu) {
    if (menu) {
        // only open list menu if it has nodes
        if (((menu == &apListMenu) && (accesspoints.count() == 0)) ||
            ((menu == &stationListMenu) && (stations.count() == 0)) ||
            ((menu == &nameListMenu) && (names.count() == 0))) {
            return;
        }

        if (currentMenu) clearMenu(currentMenu);
        currentMenu           = menu;
        currentMenu->selected = 0;
        buttonTime            = currentTime;

        if (selectedID < 0) selectedID = 0;

        if (currentMenu->parentMenu) {
            addMenuNode(currentMenu, D_BACK, currentMenu->parentMenu); // add [BACK]
            currentMenu->selected = 1;
        }

        if (currentMenu->build) currentMenu->build();
    }
}

void DisplayUI::goBack() {
    if (currentMenu->parentMenu) changeMenu(currentMenu->parentMenu);
}

void DisplayUI::createMenu(Menu* menu, Menu* parent, std::function<void()>build) {
    menu->list       = new SimpleList<MenuNode>;
    menu->parentMenu = parent;
    menu->selected   = 0;
    menu->build      = build;
}

void DisplayUI::addMenuNode(Menu* menu, std::function<String()>getStr, std::function<void()>click,
                            std::function<void()>hold) {
    menu->list->add(MenuNode{ getStr, click, hold });
}

void DisplayUI::addMenuNode(Menu* menu, std::function<String()>getStr, std::function<void()>click) {
    addMenuNode(menu, getStr, click, NULL);
}

void DisplayUI::addMenuNode(Menu* menu, std::function<String()>getStr, Menu* next) {
    addMenuNode(menu, getStr, [this, next]() {
        changeMenu(next);
    });
}

void DisplayUI::addMenuNode(Menu* menu, const char* ptr, std::function<void()>click) {
    addMenuNode(menu, [ptr]() {
        return str(ptr);
    }, click);
}

void DisplayUI::addMenuNode(Menu* menu, const char* ptr, Menu* next) {
    addMenuNode(menu, [ptr]() {
        return str(ptr);
    }, next);
}

void DisplayUI::setTime(int h, int m, int s) {
    if (s >= 60) {
        s = 0;
        m++;
    }

    if (m >= 60) {
        m = 0;
        h++;
    }

    if (h >= 24) {
        h = 0;
    }

    if (s < 0) {
        s = 59;
        m--;
    }

    if (m < 0) {
        m = 59;
        h--;
    }

    if (h < 0) {
        h = 23;
    }

    clockHour   = h;
    clockMinute = m;
    clockSecond = s;

#ifdef RTC_DS3231
    clock.setHour(clockHour);
    clock.setMinute(clockMinute);
    clock.setSecond(clockSecond);
#endif // ifdef RTC_DS3231
}