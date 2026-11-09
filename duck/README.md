# BLAZE BadUSB duck (Raspberry Pi Pico / RP2040)

The ESP8266 in BLAZE **cannot** enumerate as a USB keyboard: its USB port is
a CH340 UART bridge, not a USB device controller. This folder contains the
real BadUSB endpoint: a Raspberry Pi Pico running CircuitPython enumerates as
a HID keyboard. BLAZE sends one command byte over UART and the Pico types the
payload.

```
BLAZE NodeMCU                    Raspberry Pi Pico
-------------                    -----------------
D4 / GPIO2  (UART1 TX)  -------> GP1 (UART1 RX)
D0 / GPIO16 (ACK input) <------- GP2 (presence / ACK output)
GND                     -------> GND
```

Pico GP2 is driven HIGH while the duck is alive. For every command it pulses
LOW for ~60 ms. BLAZE treats the pulse as an ACK. No pulse -> the BADUSB screen
shows `no duck.` and no keystrokes are produced.

## Install

1. Flash CircuitPython for the Pico (`.uf2`) from circuitpython.org.
2. Copy the `adafruit_hid` library folder into `CIRCUITPY/lib/`
   (from the Adafruit CircuitPython Bundle).
3. Copy `code.py` to the root of `CIRCUITPY/`.
4. Wire the three lines above. Common ground is mandatory.
5. In BLAZE: `BADUSB` -> `NORMAL` or `CRITICAL`. Click SELECT again to send
   `0` (stop) and return to the menu.

## Commands

| Byte | Action |
|------|--------|
| `1`  | NORMAL payload (Notepad banner, harmless) |
| `2`  | CRITICAL payload (PowerShell marker file + Notepad, non-destructive lab payload) |
| `0`  | stop: abort between typed chunks, ACK |
| `?`  | ping: ACK only (BLAZE link check) |

## Payloads

The two payloads are deliberately non-destructive. Edit `normal()` and
`critical()` in `code.py` for your own authorized lab engagement. `type_text()`
types in small chunks and polls UART between them, so SELECT again can abort a
long payload.

Only use this against systems you own or have written permission to test.
