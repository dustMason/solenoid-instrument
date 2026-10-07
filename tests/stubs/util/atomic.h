#pragma once
#include "Arduino.h"
inline uint32_t __get_primask() { return fakeInterruptMask; }
