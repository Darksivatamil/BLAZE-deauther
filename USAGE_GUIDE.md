# BLAZE v2.7 — USAGE GUIDE // TEAM DARK

**AUTHORIZED TESTING ONLY.**
BLAZE transmits deauth, beacon, probe and CSA frames, clones access points and
captures handshakes. run it only on networks and clients you own, or on systems
you have explicit written permission to test. every frame carries your radio's
identity. unauthorized use is a crime in most jurisdictions, and BLAZE will not
argue with your lawyer for you.

## QUICK START

```bash
./build.sh                                              # compile (arduino-cli, nodemcuv2)
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:nodemcuv2 \
    /tmp/opencode/ESP8266_Deauther_SSD1306              # flash
```

1. serial at **115200**: `BLAZE OLED READY` → `BLAZE BTN PINS: UP=12 (D6)  DOWN=14 (D5)  A=13 (D7)`
   → `serial up. 'help' if you're lost.` → `STARTED! \o/` (the boot animation runs
   on the OLED while this prints; no version banner — `DEAUTHER_VERSION` compiles empty)
2. click **A** (face → CHAT), then hold **A** (CHAT → MENU). that is the whole
   navigation model. it is not complicated, you are just in a hurry.
3. main menu → `SCAN` → `SCAN AP + ST` → watch it work → **A** returns and stops it.
4. join the AP `BLAZE` (password `&bl@ze1234s678`), open `http://192.168.4.1/`
   (mDNS: `http://deauth.tools/`), log in `BLAZE` / `&bl@ze1234s678`.
5. before anything else: learn PANIC. hold **UP + DOWN** for 1.5 s. it stops everything.

## HARDWARE

| part | wiring |
|---|---|
| board | NodeMCU v2 (ESP8266), fqbn `esp8266:esp8266:nodemcuv2` |
| display | SSD1306 128x64 I2C OLED, address `0x3C`, SDA = GPIO4 (D2), SCL = GPIO5 (D1), flipped |
| UP | GPIO12 (D6), internal pull-up |
| DOWN | GPIO14 (D5), internal pull-up |
| SELECT (A) | GPIO13 (D7), internal pull-up |
| MODE | GPIO0 (D3) — kept pulled up, no behavior in this build. leave it alone. |
| reset | disabled (no free pin) |

- **three buttons. there is no B button** — the back handler exists in code but has
  no pin to read. `[BACK]` and **A** do the job.
- **no LED is configured in this build** — nothing will light, stance colors and all.
  define LED pins in `A_config.h` and `set led on` if you wire one.
- serial: **115200**, prompt `# `. the boot log prints the compiled button pins;
  if they do not match your wiring, fix `A_config.h`, not the guide.

## BUTTONS AND MENUS

| control | click | hold |
|---|---|---|
| UP | menu cursor up · tracker pick up · packet monitor channel +1 · clock minute +1 · probe log scroll · detector findings scroll · previous chat phrase | repeat (clock hold = +10 min) |
| DOWN | the mirror of UP | repeat (clock hold = −10 min) |
| A | activate menu item · send chat phrase · confirm tracker pick · save calibration · leave scan / packet monitor / twin / detector / probe log / vuln / clock / PANIC | 800 ms = the item's hold action (list rows open the detail screen); in CHAT = back to MENU; in TRACKER ≥2 s = enter/leave calibration or cancel a pick; in DETECTOR = prove the selected finding with an active probe |

- **mode cycle**: boot loading → FACE. main-menu item `MODE FACE` → FACE.
  A click on FACE → CHAT. A hold in CHAT → MENU.
- **child menus**: `[BACK]` is the first item, cursor starts on the second. click it.
- screens that print `B = back` are quoting a button you do not have. A does it.
- the display blanks after `displayTimeout` seconds (default 600) — but never on the
  face, tracker, twin, detector, loading or PANIC screens. any press wakes it; a click also acts
  in the same press, an A-hold on a dark screen only wakes (click first, then hold).

## PANIC — HOLD UP + DOWN, 1.5 SECONDS

works on any screen, always. stops the attack, the scan, EAPOL capture, the tracker,
the evil twin, the detector cycle and kick-and-lure. shows `PANIC / ALL STOPPED` for 2.5 s, then back to
the main menu (an **A** click dismisses it early — UP/DOWN only wake it). the
companion says `hands off the keyboard.` and keeps quiet on that screen.

**PANIC stops actions. it does not wipe your selections.** deselect them yourself.

## FEATURES

main menu, in order:
`SCAN` · `MODE FACE` · `TARGETS` · `ATTACK` · `PACKET MONITOR` · `DETECTOR` ·
`TRACKER` · `EVIL TWIN` · `TWIN <style>` · `WEIGHTS` · `RAIN ON/OFF` · `CLOCK`

### SCAN
`SCAN AP + ST` (15 s) · `SCAN APs` · `SCAN Stations` (30 s) · `PROBES <n>` · `VULN <score>`.
progress screen; **A** stops the scan and returns.

### TARGETS
`APs / Stations / Names / SSIDs` lists. click a row = select/deselect (selected rows
carry a `*`). hold **A** on a row = detail screen: encryption, RSSI, channel, MAC,
vendor; the AP detail adds `TRACK`, `SELECT/DESELECT`, `CLONE SSID`, `REMOVE`.
the AP/Stations/Names lists end in `SELECT ALL` / `DESELECT ALL` / `REMOVE ALL`.
the SSID list has none of that: `CLONE APs`, `*RANDOM MODE`, the rows themselves
(click = detail screen, hold = remove) and `REMOVE ALL`. `*RANDOM MODE` rotates
generated beacon names every 10 s from the menu, or on an interval from the CLI.

### ATTACK
- `*DEAUTH` `*BEACON` `*PROBE` `*CSA` — arm toggles, `*` = armed. live packet counts
  on the right while running. toggling while running re-applies immediately.
- `START` / `STOP` — runs whatever is armed, packet rate on the right. auto-stops
  after `attacktimeout` seconds (default 600, 0 = never).
- `KILL` — continuous deauth + deauth-all, no timeout. shows elapsed `m:ss` and pps
  while running. hard cap: `kill.minutes` (default 5, minimum 1), then it stops
  itself. you can forget it running. it cannot forget you.
- `KICK` — kick-and-lure: evil-twin portal on the first selected AP (falls back to
  AP 0) plus timed deauth to push clients onto it. right side reads `twin` when
  idle, the try count while the portal is up (`:PW` once a password lands), `kick`
  during the deauth phase. click again = stop both.
- `CAP` — EAPOL handshake capture on the first selected AP. phase shows `kick`,
  then `REC <n>`; the total stays on the row when idle. file: `/eapol.pcap` (DLT 105).

### EVIL TWIN / TWIN <style>
`EVIL TWIN` runs the whole workflow in one press: it clones the first selected
AP (else AP 0) as an open network, brings the twin up **and** starts a timed
deauth kick so clients get pushed off the real AP onto the clone. no scan yet →
`scan first.` flash; protected/hidden/already-running target → `twin refused.`
it never fails silently. screen: target ssid, `TRIES`, `waiting for victim` /
`trying password...`, and once a submission lands `GOT: <email>` + `PW: <pw>`,
`SELECT = STOP` — leaving the screen stops the twin **and** the kick.
`TWIN <style>` cycles the portal: `matrix` → `router` → `venue` → `google`
(default; or `set twin.template 0|1|2|3`). the google template is a sign-in
page that captures **email + password immediately** — account creds are not a
wifi key, so there is no `WiFi.begin` validation and the twin keeps running.
the wifi templates keep the async check against the real AP (~15 s each).
web: `/twin.json` reports `user` + `password` live. a `/portal.html` in
LittleFS (under 12000 bytes, `%SSID%` placeholder) overrides all four.
5-minute hard timeout, then it stops itself. OwnNetGuard: your own/home
SSID is refused — `that's my house. no.`

### PACKET MONITOR
sniffer graph. header: channel, deauth count in brackets, packet rate. UP/DOWN
change channel. **A** stops it and returns.

### DETECTOR
opens a submenu: `SCAN AIR` (the original hunter), `HIDDEN SSID`, `PROBE DUEL`,
`RIVAL OP`, `TIMELINE`. one cycle: AP scan → 10 s sniff on a channel hop →
analysis. every verdict is measured evidence, graded
`-` INFO (a fact) · `!` LIKELY (a measured anomaly) · `*` CONFIRMED (measured
attack behavior). checks: deauth/disassoc floods (≥5/10 s per source; a source
that is not in the scan = a spoofed radio), beacon spam against the spec's
~10/s, one MAC advertising many SSIDs, beacons claiming a different channel
than the one they were heard on, and one SSID sitting on two BSSIDs (mixed
encryption, a security-IE mismatch or a random MAC = likely clone). while it
runs the screen shows the phase; afterwards the findings window (UP/DOWN
scroll), the selected finding's evidence line, `UP/DN SEL=back`. **A** stops an
early run and returns; findings from a finished run stay until the next cycle.
**A-hold** = prove a `DUP` finding: a directed probe on both of its channels
counting the radios that answer — 2+ answering with mixed security, a different
RSN/WPA security-IE fingerprint, or a DS-channel claim that contradicts where
they were heard = CONFIRMED. identical fingerprints read as roaming, not a
clone. `PROBE DUEL` runs the same cycle with the duel lens, `RIVAL OP` with the
rival lens (LAA deauth/beacon sources, LAA / mixed-security / channel-mismatch
duplicate SSIDs), `HIDDEN SSID` runs the 15 s unmask cycle, `TIMELINE` shows
the probe ring newest-first. own/home SSIDs are never probed. stated limits:
one radio hears one channel at a time, and a deauther that goes quiet outside
the 10 s window is invisible to it.

### HIDDEN SSID
15 s AP scan → 12 s channel-hop listen → 4 s directed kick at the hidden BSSIDs
→ 12 s re-listen. an exact name comes only from an association/reassociation
request or a probe response matched on the BSSID (`=name`). a directed probe
request on a channel with exactly one hidden AP is a candidate (`~name`) — never
sold as fact. conflicting candidates show `?<ambig>`. the evidence line gives
channel, RSSI, BSSID tail and source (`a` assoc, `r` reassoc, `p` probe-resp,
`q` probe-req). **A** stops the run and returns to the detector submenu; a
finished run keeps its results until re-entered.

### PNL HARVEST / VICTIM ID / TIMELINE
all three reuse the existing 32-entry probe ring (run a scan first).
`PNL HARVEST` groups probed SSIDs: clients, probe count, best RSSI, last
channel/age; row 0 is `[BACK]`, **A** opens the selected entry's detail.
`VICTIM ID` lists unique probing clients with vendor (or `rand` for a
randomized/LAA MAC), last SSID, probe count and RSSI; same `[BACK]` + detail
contract. `TIMELINE` sorts the ring newest-first (`12s c6 AABBCC ssid`).
CLI: `intel [hidden|stop|pnl|victim|timeline|status]`; web: `/intel.json`.

### TRACKER
`TRACKER` opens `PICK TARGET` — but only with APs in the list, so run a scan first.
**A** = pick, **A-hold** = cancel. tracking screen: `TGT <ssid>` with
`CLOSER` / `FARTHER` / `STEADY`, dBm + gauge, `DIST` / `MIN`, RSSI sparkline,
`ENV` factor, and `TRACKING` / `SCAN` / `LOST`.
**A-hold ≥2 s** = calibrate: stand 1 m from the target, `CALIBRATE @ 1m`,
**A** = SAVE, hold = CANCEL (2–2.9 s window). stored as `txp` in `/blaze.cfg`.
UP/DOWN switch target. **A** click = back to menu (stops tracking).
the distance is a path-loss estimate. treat it as a direction, not a ruler.

### PROBES <n>
live probe-request log, ring of 32, newest first. starts the sniffer if nothing is
sniffing. UP/DOWN scroll, **A** back (stops the sniff only if this screen started it).

### VULN <score>
score from the weakest encryption seen: open/WEP 90, TKIP 65, mixed 40, WPA2-AES 10
(unknown 25). overall level from that score: ≥80 `CRITICAL`, ≥55 `HIGH`, ≥30 `MEDIUM`,
under 30 `LOW`, `NO DATA` with no APs. per-AP `level` is a different scale — WPA2-AES
reads `PASS` there. shows the worst AP. full report: `/report.json`, `/report.csv`, or
`report [json/csv]` on serial (score, level, ap count, eapol frames, probe requests;
JSON per-AP rows carry ssid/mac/ch/enc/level, the CSV drops mac).

### WEIGHTS
26 face reactions. click = +1 (0–20, wraps to 0), saved on every click as
`face.weights`. `RESET ALL` restores defaults.

### RAIN ON/OFF
matrix rain behind the menu. persisted as `rain`.

### CLOCK
`CLOCK DISPLAY` (big time) and `SET CLOCK`: UP/DOWN nudge the minutes, hold = ±10,
**A** = back. there is no RTC in this build — the hour boots random and nothing in
the UI changes it. fit an `RTC_DS3231` if you actually care what time it is.

## THE COMPANION

- stances: `IDLE` `PROWL` `HUNGRY` `UNIMPRESSED` `LOCKED` `STANDBY` — the same
  strings show up in `/blaze/status` and in the `stance?` answer.
- what moves them: scan/tracking → PROWL · attack, KILL, twin → HUNGRY ·
  handshake or intruder → LOCKED · failed login, lost target, own-net refusal →
  UNIMPRESSED · display off → STANDBY · panic → IDLE.
- lines (two picks per event): boot `team DARK online.` · attack `let's eat.` ·
  kill `kill mode.` · twin creds `keys handed over.` · own net `that's my house. no.` ·
  intruder `someone's touching MY network.` · panic `hands off the keyboard.`
  event cooldowns apply (intruder 30 s); boot, twin-creds and panic always speak.
- idle banter every 60–120 s, only when idle: boot finished, screen on, not PANIC,
  not CHAT. it has no opinion while you are typing at it.
- `WEIGHTS` tunes how often each reaction fires (see above).
- LED, if you wired one: HUNGRY red, LOCKED cyan, PROWL violet, UNIMPRESSED amber,
  STANDBY dark, idle green — scanning blue and attacking red override the stance
  while they run. stock build: nothing lights.

### CHAT
- in: `MODE FACE` → **A** click (FACE → CHAT). out: **A** hold 800 ms (CHAT → MENU).
- phrase wheel of 8: `status?` `report` `uptime?` `stance?` `what do you see?`
  `who's there?` `roast me` `help`. UP/DOWN moves, **A** sends, the wheel advances.
- real answers: `status` `report` `uptime` `stance` `see` `help`.
  keyword rules: attack words → `menus do the killing. chat doesn't.` ·
  password/wpa → `the twin hands them over.` · wifi/network → `scan first. then the twin.` ·
  cute → `wrong device.` · greetings → `say something useful.` ·
  anything else → `try harder.` or one of its siblings.
- 40 characters max. the exchange flashes on the face even when you are elsewhere.
- same brain on serial: `blaze chat <message>`.

## LOYALTY — OWN-NET GUARD + INTRUDER ALERT

- `set blaze.home "<ssid>"` / `get blaze.home` — quote it, the parser is
  quote-aware. default is unset; BLAZE's own AP SSID is always protected anyway.
- your home network can never be selected or attacked: selection guard, attack-start
  guard, outgoing-packet guard. select/attack refusals print
  `[BLAZE] own-net guard: <ssid>`, the outgoing-packet block prints
  `[BLAZE] own-net guard - refused.` (rate-limited to once per 5 s), and either way
  the face flashes `that's my house. no.` or `not your network. mine.`
- intruder alert: while idle, the home SSID appearing in the probe log — or a clone
  of it (same name, different radio) — fires `someone's touching MY network.`
  with a 30 s cooldown.
- the watch stands down if `blaze.home` equals BLAZE's own AP SSID. protecting
  yourself from yourself is not a job.

## WEB UI

nav: `SCAN` `SSIDs` `ATTACK` `CHAT` `TRACK` `TWIN` `DETECT` `SETTINGS` `INFO`.
login `BLAZE` / `&bl@ze1234s678`; wrong password gets `not even close.` on the
login page — the face stays out of it. `POST /blaze/loginfail` exists server-side
and would fire `wrong door.`, but no shipped page ever calls it. dashboard footers
read `BLAZE v2.7 // TEAM DARK`; the login page reads `TEAM DARK // restricted
access terminal` and `v2.7 // unauthorized access is logged`.

```text
GET  /blaze/status    scan attack twin track stance line uptime(seconds)
GET  /blaze/log       persona log (oldest → newest)
GET  /blaze/chat      chat log      POST /blaze/chat {"msg":"..."}
GET  /blaze/tracker   live tracker state   POST /blaze/track {on,target,env,txp,...}
GET  /report.json     /report.csv   vulnerability report
GET  /twin  /twin.json  /detect  /detect.json  /attack.json
GET  /detect?cmd=start&filter=all|duel|rival   /detect?cmd=filter&f=all|duel|rival
GET  /detect?cmd=stop|clear|confirm&i=<idx>
GET  /run?cmd=...     run any CLI command
```

the CHAT *page* ships (`data/web/chat.html`, packed into `webfiles.h`) but the
default serving mode (`webSpiffs off`) registers no route for it — the nav's CHAT
link lands on a 404 until you `set webSpiffs on` and reboot, which copies every
page to LittleFS and serves from there. the chat API works either way.

## SERIAL CLI

`help` prints `[===== BLAZE commands - read them =====]` and the list. it meant it.

```text
scan [<all/aps/stations>] [-t <time>] [-c <continue-time>] [-ch <channel>]
show [selected] [<all/aps/stations/names/ssids>]
select|deselect [<all/aps/stations/names>] [<id>]
attack [beacon] [deauth] [deauthall] [probe] [csa] [nooutput] [-t <timeout>]
attack status [<on/off>]
stop <all/scan/attack/script>
report [json/csv]
detect [start|duel|rival|stop|status|clear|confirm <idx>]
get <setting>            set <setting> <value>
blaze chat <message>
add ssid <ssid> [-wpa2] [-cl <clones>]
enable random <interval> / disable random
save|load [<all/ssids/names/settings>]
screen <on/off>          screen mode <menu/packetmonitor/buttontest/loadingscreen>
led <r> <g> <b>          led <#rrggbb>
send deauth <apMac> <stMac> <rason> <channel>
sysinfo / info / clear / reboot / reset / // comments
```

- tokens are case-insensitive and aliases work (`ap`/`aps`, `-d`, `-ch`);
  `scan wifi` sniffs — the same thing PACKET MONITOR does.
- `set` needs exactly three tokens: `set blaze.home "my home net"`.
- `reset` = settings back to default. `clear` = 100 `#` and 60 blank lines to scroll
  the terminal away, it wipes no log. neither deletes your targets out of spite —
  `remove aps all` does that.
- `chicken` is in the help output for a reason. ignore it.

## SETTINGS THAT MATTER

`get settings` prints everything. keys worth knowing:

```text
attacktimeout      seconds before an attack auto-stops (0 = never, default 600)
deauthspertarget   deauthReason   probesPerSSID   beaconInterval   randomTX   beaconchannel
channel            wifi channel   chtime   minDeauths        (sniffer group)
ssid / password / hidden / ip                                  BLAZE's own access point
webinterface / captivePortal / webSpiffs / lang
serial / serialEcho                                             CLI on the serial port
led / display / displayTimeout              LED, display, blanking (seconds)
autosave / autosavetime
twin.template      0 matrix | 1 router | 2 venue | 3 google (default)   (BLAZE key)
kill.minutes       KILL auto-stop, min 1, default 5  (BLAZE key)
blaze.home         the network BLAZE protects        (BLAZE key)
```

BLAZE keys live in `/blaze.cfg` and are written the moment you set them — `save`
and `load` do not touch them. `get` works on them too.

## BEFORE YOU DEPLOY

- authorized scope, written permission, your name on the paperwork. no exceptions,
  no "just a test".
- set `blaze.home` first. the guard refuses BLAZE's own SSID by default; it cannot
  guess where you live.
- set `kill.minutes` and `attacktimeout`. they exist because you will forget.
- PANIC first: UP + DOWN, 1.5 s. `stop all` from serial works too.
- read `help` when something confuses you. the header already told you to.

`[===== BLAZE commands - read them =====]`
