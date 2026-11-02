/* BLAZE - Team DARK
   TRACKER mode - hunt a target AP with live RSSI gauge,
   distance estimate (log-distance path loss), warm/cold
   direction, min-distance memory, auto re-scan, lost-signal
   handling and sparkline history. A target must be picked
   before tracking starts. */

#pragma once

#include <Arduino.h>

namespace BlazeTracker {
    enum Alert { ALERT_NONE,
                 ALERT_LOST,
                 ALERT_FOUND };

    void setup();

    // ===== target pick (mandatory before start) ===== //
    void    beginPick();      // enter target-select state (no tracking yet)
    bool    picking();
    int     pickCount();      // number of APs in the list
    int     pickIndex();      // highlighted row
    String  pickSsid(int i);  // AP ssid at row i
    int     pickRssi(int i);  // AP rssi at row i
    int     pickCh(int i);    // AP channel at row i
    bool    pickSelected(int i); // row i is starred for attack
    void    pickMove(int8_t d);  // move highlight (wraps)
    bool    pickConfirm();    // highlight -> start tracking
    void    pickCancel();     // leave pick state

    // start/stop tracking (listIndex = position in the AP list)
    bool start(int listIndex);
    bool startBest();     // first selected AP, else AP 0
    void stop();
    bool active();

    void update();        // called every loop from BlazeCore

    // button hooks (DisplayUI)
    void selectNext();    // DOWN: switch target to next AP
    void selectPrev();    // UP:   switch target to previous AP
    void selectHold();    // SELECT held >=2s: enter calibrate / cancel
    void calibrateSave(); // SELECT click inside calibrate state
    void calibrateCancel();

    // live state (OLED draw + web)
    bool    calibrating();
    bool    lost();
    bool    hasSignal();
    int     rssi();       // last raw dBm (0 = none yet)
    float   distance();   // meters, -1 = unknown
    float   minDistance();
    float   txPower();
    float   envFactor();
    String  envName();
    void    setEnvFactor(float n);
    void    setEnvName(const String& name);
    void    setTxPower(float t);
    int     direction();  // +1 closer, -1 farther, 0 steady
    String  targetSsid();
    int     targetIndex();
    uint8_t histCount();
    int     histAt(uint8_t i); // 0 = oldest sample
    void    doCalibrate();     // one-shot (web button)

    Alert takeAlert();    // returns + clears LOST/FOUND edge

    String toJson();      // full state for the web UI
}
