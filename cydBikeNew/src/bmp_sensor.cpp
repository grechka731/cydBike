#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_BMP085.h>
#include "config.h"
#include "debug.h"
#include "app_settings.h"
#include "bmp_sensor.h"

static Adafruit_BMP085 bmp;
static bool bmpFound = false;

void setupBmpSensor() {
  bmpFound = bmp.begin();
  if (!bmpFound) {
    DBG_PRINTLN("[bmp] BMP180 not found (expected at 0x77).");
  } else {
    DBG_PRINTLN("[bmp] BMP180 initialized.");
  }
}

bool isBmpFound() {
  return bmpFound;
}

BmpData readBmpData() {
  BmpData data;

  if (!bmpFound) {
    data.valid = false;
    data.temperatureC = 0.0f;
    data.pressureHpa = 0.0f;
    data.altitudeM = 0.0f;
    return data;
  }

  data.valid = true;
  data.temperatureC = bmp.readTemperature();

  long pressurePa = bmp.readPressure();
  data.pressureHpa = pressurePa / 100.0f;

  data.altitudeM = 44330.0f * (1.0f - powf(data.pressureHpa / settings().seaLevelHpa, 0.190295f));

  return data;
}
