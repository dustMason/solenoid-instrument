#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .build
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -I. \
  solenoid_engine.cpp note_periods.cpp tests/engine_test.cpp -o .build/engine-test
.build/engine-test
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -I. -Itests/stubs \
  -D__MK20DX256__ -DUSB_MIDI \
  solenoid_engine.cpp note_periods.cpp tests/adapter_test.cpp -o .build/adapter-test
.build/adapter-test
