#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cli="${ARDUINO_CLI:-arduino-cli}"
options=()
if [[ -n "${ARDUINO_CONFIG:-}" ]]; then options+=(--config-file "$ARDUINO_CONFIG"); fi
mkdir -p .build/sketch/solenoids .build/firmware
# Arduino requires the folder and primary sketch to have the same name.
cp solenoids.ino solenoid_engine.h solenoid_engine.cpp note_periods.cpp .build/sketch/solenoids/
ARDUINO_BUILD_CACHE_PATH="$PWD/.build/arduino-cache" "$cli" "${options[@]}" compile \
  --fqbn "${FQBN:-teensy:avr:teensy31:usb=midi,speed=72,opt=o2std}" \
  --warnings all --build-path "$PWD/.build/arduino-build" --output-dir "$PWD/.build/firmware" \
  "$PWD/.build/sketch/solenoids"
# Deliberately compile only; never upload or open a hardware connection.
