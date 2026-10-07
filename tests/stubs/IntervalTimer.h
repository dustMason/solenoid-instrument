#pragma once
#include <stdint.h>
struct IntervalTimer {
  void (*callback)() = nullptr;
  uint32_t interval = 0;
  uint8_t irqPriority = 0;
  bool begin(void (*fn)(), uint32_t microseconds) { callback = fn; interval = microseconds; return true; }
  void priority(uint8_t value) { irqPriority = value; }
};
