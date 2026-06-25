#pragma once

#include <Arduino.h>

#define DEBUG_ENABLED 0

#if DEBUG_ENABLED
  #define DBG_BEGIN(baud) Serial.begin(baud)
  #define DBG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else

  #define DBG_BEGIN(baud)  do {} while (0)
  #define DBG_PRINT(...)   do {} while (0)
  #define DBG_PRINTLN(...) do {} while (0)
  #define DBG_PRINTF(...)  do {} while (0)
#endif
