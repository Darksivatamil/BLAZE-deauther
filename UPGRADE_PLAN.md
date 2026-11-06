# BLAZE UPGRADE PLAN — From Tool to Hacking Device + Hacker Companion

Status: **DECISIONS LOCKED — awaiting go**
Baseline: commit `097ac2f` (working tree clean, build green)

### Owner decisions (final)
1. **NIM AI uplink: DROPPED entirely** — dead `BLAZE_AI_*` config removed from
   `A_config.h`; companion runs on the offline brain only. No HTTPClient/BearSSL.
2. **Face: KEPT** — eyes survive, redrawn angular/crosshair; same 26 reaction
   indices, `face.weights` CSV stays valid.
3. **Sound: REMOVED permanently** — no `BlazeSound`, no riffs, `BLAZE_SOUND_PIN`
   deleted from `A_config.h`. Voice = text only (OLED/web/CLI).

---

## 0. Vision

BLAZE stops being "a deauther with a face on it" and becomes **a hacking device with a
partner riding shotgun**:

- **The device** — a hardened, reliable attack platform (already largely built:
  deauth, CSA, Kick-and-Lure, Evil Twin, BlazeRecon, tracker, panic).
- **The companion** — a personality that talks to you while you work: narrates what
  it sees, celebrates catches, warns you, roasts you, refuses dumb orders.

### Voice bible (applies to EVERY new string, OLED/web/CLI)

| Trait | Means | Example |
|---|---|---|
| **BOLD** | Short, declarative, no hedging, no exclamation-mark glee | `watch this.` / `3 losers on channel 6.` |
| **LOYAL** | Has the owner's back; protective of "our" network; never abandons | `someone's touching MY network.` / `wiped. you owe me.` |
| **RUDE** | Dry contempt toward targets AND toward the owner's mistakes | `wrong password. again.` / `too easy.` |
| **NOT CUTE** | No hearts, no "NOO!!", no anime glee, no pity, no uwu | ~~`NOO!!`~~ → `try harder.` |

Rules of thumb:
- Lines ≤ 32 chars (fits 128 px OLED, font 10 ≈ 21 chars/line → 2 lines max).
- Lowercase for taunts, UPPERCASE for alerts (`INTRUDER` vs `still here.`).
- Never cute-sad: disappointment is flat, not tearful.
- Evil Twin portal copy is **exempt** — it must stay believable to victims.
  Rudeness is BLAZE's voice *to us*, not to the mark.

---

## 1. Current-state audit (grounded facts)

### 1.1 What already exists (reused, not rebuilt)
- Attack core: burst-scheduled deauth, CSA, Kick-and-Lure, EvilTwin (3 templates),
  BlazeRecon (EAPOL/probe/vuln report), BlazeTracker, panic (hold UP+DOWN 1.5 s).
- Web UI: dark matrix/glitch theme (`data/web/style.css`) — already on-brand.
- Terminal-style boot: `boot_` → `BLAZE` → `here_` (`BlazeFace.cpp:595-616`) — on-brand.
- FX: matrix rain (on-brand), confetti (cute — re-skin).
- Weights editor: 26 reactions persisted as `face.weights` CSV (`BlazeFace.cpp:710-748`).

### 1.2 Dead assets (cleanup list — decided)
| Asset | Where | Action |
|---|---|---|
| NVIDIA NIM AI config (host/path/model/key) | `A_config.h:73-77` | **DELETE** (NIM dropped) |
| Internet uplink creds `BLAZE_UPLINK_SSID/PASS` | `A_config.h:81-83` | **DELETE** (no uplink needed offline-only) |
| Sound pin `BLAZE_SOUND_PIN 16` | `A_config.h:69` | **DELETE** (sound removed permanently) |
| Split-screen chat prototype | `NEW/chatting_system.ino` (475 lines) | **REUSE as prior art** for CHAT mode layout |
| CHAT display mode | comment promises `FACE → CHAT → MENU` (`DisplayUI.cpp:996`, `DisplayUI.h:151`) | **BUILD IT** — mode missing from enum (`DisplayUI.h:76-91`) |

### 1.3 The cute inventory (exact strings/behaviors to eliminate)
| Current | Where | Replacement direction |
|---|---|---|
| `NOO!!` flash after failed web login | `DisplayUI.cpp:1100` | `try harder.` |
| `BRAVE!` flash | `BlazeMood.cpp:34` | `let's eat.` |
| `KILL!` flash | `BlazeCore.cpp:72` | **KEEP** (already on-brand) |
| Reaction labels `LOVE, SHY, CRYING, VERY HAPPY, HAPPY, EXCITED, SAD, WORRIED, LAUGHING` | `BlazeFace.cpp:143-147` | rename to `LOCKED ON, SCOFF, HOLLOW, SMUG, GRIN, HUNGRY, FLAT, PARANOID, CACKLE` (same indices — `face.weights` CSV stays valid) |
| `drawHeart` / `loveEyes` (heart eyes) | `BlazeFace.cpp:338,345` | crosshair-style `LOCKED ON` eyes |
| `cryingEyes`, `shyEyes` | `BlazeFace.cpp:425,378` | flat/hollow redraws |
| Mood names `CURIOS/SAD/BRAVE/SLEEP` | `BlazeMood.cpp:22-27` | stances: `PROWL/UNIMPRESSED/HUNGRY/STANDBY` (CALM→IDLE, ALERT→LOCKED) |
| Confetti on attack end | `BlazeCore.cpp:60`, drawn `DisplayUI.cpp:1235` | re-skin as **GLITCH burst** (screen-tear), keep `BlazeFx` API |
| Greeting text | `BlazeFace.cpp` startup greeting | audit → terminal-flavored, no cutesy text |

Everything cute is **renamed/redrawn in place** — the plumbing (mood state machine,
reaction weights, flash messages, FX API) survives, so risk stays low.

---

## 2. Architecture — what gets built

```
                    ┌─────────────────────────────┐
   events ─────────▶│  BlazePersona  (new)        │──▶ OLED flash / CHAT log /
   (attack, scan,   │  stance engine + line tables│    web /blaze/log / CLI
    victim, intruder│  (reworks BlazeMood)         │──▶ BlazeFace bias (reactions)
    login fail …)   └─────────────┬───────────────┘
                                  │
   user text ──────▶┌─────────────▼───────────────┐
   (buttons/web/CLI)│  BlazeChat    (new)          │
                    │  DISPLAY_MODE::CHAT          │
                    │  offline brain (keyword)     │
                    │  + canned phrase wheel       │
                    └─────────────────────────────┘
```

Text-only companion: **no sound module, no AI uplink** (both dropped by decision).

### 2.1 `BlazePersona` — the voice (new `BlazePersona.cpp/.h`)
- **Stance engine**: reworks `BlazeMood`'s 6 states → `IDLE / PROWL / HUNGRY /
  UNIMPRESSED / LOCKED / STANDBY`. Same triggers (attack start/stop, scan done,
  alert), same bias feed into `BlazeFace::setMoodBias()` — but renamed + rude.
- **Line tables** in PROGMEM: event → 4–8 candidate lines, weighted-random pick,
  per-event cooldown (no repeat < 20 s), last-line ring buffer (16 × ~56 B ≈ 900 B RAM).
- **Events**: `BOOT, SCAN_DONE, TARGET_SET, ATTACK_START, ATTACK_STOP, VICTIM_CAUGHT,
  TWIN_START, TWIN_CREDS, LOGIN_FAIL, KILL, PANIC, INTRUDER, IDLE_BANTER (60–120 s
  random), CHAT_REPLY`.
- **Channels**: OLED flash (`flashMsg` reuse), CHAT log, new `/blaze/log` endpoint,
  CLI (`prnt`), LED color.
- ~4.5 KB PROGMEM for tables.

### 2.2 `BlazeChat` — the CHAT mode (new `BlazeChat.cpp/.h` + `DisplayUI` wiring)
- New `DISPLAY_MODE::CHAT` in `DisplayUI.h:76`; `cycleMode()` delivers the promised
  **FACE → CHAT → MENU** loop (`DisplayUI.cpp:996`).
- Layout (from `NEW/chatting_system.ino` prior art): **top 32 px** = stance eyes,
  **bottom 32 px** = chat log (2 lines, ArialMT_Plain_10, `blaze>` prefix).
- Input, three ways:
  1. **On-device**: UP/DOWN scroll a canned phrase wheel (`status?`, `who's there?`,
     `roast me`, `what do you see?`, `report`), A sends.
  2. **Web**: new `data/web/chat.html` + `/js/chat.js`, POST `/blaze/chat`.
  3. **CLI**: `blaze chat <msg>`.
- **Offline brain**: keyword table → persona reply + query answers (`status`,
  `report`, `stance`, `uptime`). Chat-driven *attacks* are **v1-excluded**
  (commands stay in menus — a rude companion must not be a remote-trigger
  accident waiting to happen).

### 2.3 Loyalty watchdogs (the "LOYAL" made concrete)
1. **OwnNetGuard** — refuse any attack selection matching the device's own AP SSID
   or the configured home SSID (new `BlazeCfg` key `blaze.home`, set via CLI/
   settings — replaces the deleted `BLAZE_UPLINK_*`): flash `that's my house. no.`
2. **IntruderAlert** — match `blaze.home` SSID in probe log or scan results while
   idle → stance `LOCKED` + `INTRUDER` flash + line
   `someone's touching MY network.` Cooldown 30 s. (Reuses BlazeRecon probe log —
   no new radio work.)

### 2.4 Web companion surface
- New `data/web/chat.html` + `js/chat.js`: chat box + live persona log
  (poll `/blaze/log`), stance chip.
- Extend `/blaze/status` (`wifi.cpp:396-404`): `stance`, `lastLine`, `uptime`
  (keep existing keys — `blaze.js` depends on them; verify `mood` key consumers).
- Regenerate `webfiles.h` via `webtools.py` (never hand-edit the gz blob).
- Login page copy → rude (`not even close.`), portal templates unchanged.

---

## 3. Phases (each ends with `./build.sh` green + size delta recorded)

| # | Phase | Contents | Acceptance |
|---|---|---|---|
| **0** | Cleanup | Delete dead config (`BLAZE_AI_*`, `BLAZE_UPLINK_*`, `BLAZE_SOUND_PIN`) from `A_config.h` | build green; no dangling refs |
| **1** | Persona + de-cute | `BlazePersona` (rework BlazeMood→stances), line tables, all §1.3 renames/redraws, confetti→GLITCH, greet audit | grep audit: no banned strings left; `face.weights` CSV still loads; build green |
| **2** | CHAT mode | `DISPLAY_MODE::CHAT`, canned wheel, offline brain, `/blaze/chat` + `/blaze/log`, `chat.html`, CLI `blaze chat` | FACE→CHAT→MENU cycle works; web msg appears on OLED; offline replies in voice |
| **3** | Loyalty | OwnNetGuard + IntruderAlert | own-SSID attack refused; home-SSID probe triggers INTRUDER once/30 s |
| **4** | Polish | web login copy, CLI banner, LED↔stance sync (`led.h`), tracker/vuln copy pass | tone consistent across OLED/web/CLI/LED |
| **5** | Ship | `USAGE_GUIDE.md` (rewritten in voice), flash + serial verification sequence | guide complete; on-device checklist run when board available |

### Hard rules (every phase)
1. **No new `IRAM_ATTR`** — IRAM is at 93 % (61035/65536). New code lives in flash.
2. **Attack paths stay non-blocking** — no call > 50 ms in update loop.
3. **All new strings PROGMEM** (`F()` / `const char[] PROGMEM` tables).
4. **Panic must remain absolute** — add: cancel chat render + persona banter,
   then existing wipe.
5. Size budget: flash < 90 % (943 KB), static RAM < 60 %, idle heap > 20 KB.
   IRAM unchanged.
6. Web edits: source in `data/web/` → `python3 webtools.py` → commit `webfiles.h`.
7. `functions.h` stays main-`ino`-only; `prnt/prntln` via `language.h` externs.

---

## 4. Size budget estimate

| Addition | Flash | RAM |
|---|---|---|
| Persona lines + stance rework | ~6 KB | ~1 KB |
| Chat mode + offline brain | ~8 KB | ~1 KB |
| Web chat page (gz) | ~4 KB | — |
| Loyalty watchdogs | ~3 KB | ~0.2 KB |
| Dead-config deletion | −1 KB | — |
| **Total** | **~20 KB** → ≈ 823 KB ≈ **79 %** | static ≈ 55 % |

Comfortable headroom — no TLS, no sound, no new libraries.

---

## 5. Pending work carried over from the stop

1. **`USAGE_GUIDE.md`** — was halted mid-write; rewritten in Phase 5 in the new
   voice (setup, buttons, panic, all features, companion usage, authorized-testing
   notice).
2. **Flash + serial verification** — blocked on hardware (no `/dev/ttyUSB*`,
   `arduino-cli board list` empty). Sequence when board is plugged in:
   ```
   ./build.sh
   arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:nodemcuv2 \
       /tmp/opencode/ESP8266_Deauther_SSD1306
   # then serial: boot → stance lines → scan → attack → panic → chat round-trip
   ```
3. **NIM key**: config deleted from `A_config.h`, but the key remains in git
   history — rotate it at the NVIDIA console if that account still matters.

---

## 6. Decisions (locked)

| Decision | Outcome |
|---|---|
| AI uplink (NIM) | **DROPPED** — dead config deleted; offline brain only; no HTTPClient/BearSSL in the project |
| Face | **KEPT** — eyes redrawn angular/crosshair; 26 reaction indices unchanged, `face.weights` CSV valid |
| Sound | **REMOVED permanently** — no sound module; `BLAZE_SOUND_PIN` deleted; voice is text-only |
