#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "debug.h"
#include "i2c_bus.h"

void setupI2CBus() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  int found = scanI2C(nullptr, 0);
  if (found == 0) {
    DBG_PRINTLN("[i2c] No devices found on the bus.");
  }
}

int scanI2C(uint8_t* out, int maxn) {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (out && found < maxn) out[found] = addr;
      found++;
    }
  }
  return found;
}
