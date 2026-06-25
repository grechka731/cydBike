#include <Arduino.h>
#include "config.h"
#include "app_settings.h"
#include "gear_control.h"
#include "joystick.h"
#include "i2c_bus.h"
#include "bmp_sensor.h"
#include "mpu_sensor.h"
#include "mpu_speed.h"
#include "ride_stats.h"
#include "display_ui.h"
#include "sd_logger.h"
#include "net_telemetry.h"

static unsigned long lastDisplayUpdate = 0;
static const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 200;

void setup() {
  settingsBegin();

  setupDisplay();
  drawCalibrationSplash();
  setupGearControl();
  setupJoystick();

  setupI2CBus();
  setupBmpSensor();
  setupMpuSensor();
  setupMpuSpeed();

  rideStatsBegin();

#if FEATURE_SD
  sdLoggerBegin();
#endif
#if FEATURE_WIFI
  netBegin();
#endif

#if SHOW_I2C_DIAGNOSTIC
  drawBootDiagnostics();
#endif

  drawStaticLayout();
  updateDisplay();
}

void loop() {
  updateJoystick();
  updateMpuSpeed();
  rideStatsUpdate();

  if (settings().autoShift) {
    autoShiftUpdate(getSpeedKmh());
  }

#if FEATURE_WIFI
  netUpdate();
#endif
#if FEATURE_SD
  sdLoggerUpdate();
#endif

  pollDisplayTouch();

  unsigned long now = millis();
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
}
