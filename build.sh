#!/bin/bash
# BLAZE-deauther build helper
# arduino-cli requires sketch folder name == .ino name, so we rsync
# the project into /tmp/opencode/ESP8266_Deauther_SSD1306 and build there.
set -e
SRC="/media/sivaraman/E/AI PROJECTS/BLAZE-deauther"
DST="/tmp/opencode/ESP8266_Deauther_SSD1306"
rm -rf "$DST"
mkdir -p "$DST"
rsync -a --exclude '.git' --exclude 'NEW' --exclude 'ESP8266_Deauther_SSD1306' --exclude 'build.sh' "$SRC/" "$DST/"
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 "$DST" "$@"
