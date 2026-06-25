#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "debug.h"
#include "gear_control.h"
#include "app_settings.h"

static const int LEDC_CHANNEL = 1;
static const int LEDC_FREQ = 50;
static const int LEDC_RES = 14;

static Preferences prefs;
static int currentGear = 1;
static int currentAngle = GEAR_ANGLES[0];
static volatile bool moving = false;
static unsigned long lastAutoShiftMs = 0;

int getActiveNumGears() { return settings().numGears; }
int gearStoredAngle(int gear) {
  int idx = constrain(gear, 1, NUM_GEARS_MAX) - 1;
  return settings().gearAngles[idx];
}

static int angleToDuty(int angle) {
  return map(angle, 0, 180, 410, 2048);
}

static void writeAngle(int angle) {
  angle = constrain(angle, ANGLE_MIN, ANGLE_MAX);
  ledcWrite(LEDC_CHANNEL, angleToDuty(angle));
  currentAngle = angle;
}

static int gearFromAngle(int angle) {
  int best = 1;
  int bestDiff = 100000;
  int n = getActiveNumGears();
  for (int i = 0; i < n; i++) {
    int diff = abs(angle - settings().gearAngles[i]);
    if (diff < bestDiff) {
      bestDiff = diff;
      best = i + 1;
    }
  }
  if (n >= 2 && abs(angle - settings().gear2Downshift) < bestDiff) {
    best = 2;
  }
  return best;
}

static int targetAngleForGear(int gear, bool downshift) {
  if (gear == 2 && downshift) {
    return settings().gear2Downshift;
  }
  return settings().gearAngles[gear - 1];
}

static void moveServoSmooth(int targetAngle, bool upshift) {
  targetAngle = constrain(targetAngle, ANGLE_MIN, ANGLE_MAX);
  moving = true;
  int dir = (targetAngle >= currentAngle) ? 1 : -1;

  int stepDeg   = upshift ? SERVO_STEP_DEG_UP      : SERVO_STEP_DEG_DOWN;
  int stepDelay = upshift ? SERVO_STEP_DELAY_UP_MS : SERVO_STEP_DELAY_DOWN_MS;
  if (stepDeg < 1) stepDeg = 1;
  int degSinceRest = 0;
  int degSinceSave = 0;
  while (currentAngle != targetAngle) {
    int remaining = abs(targetAngle - currentAngle);
    int inc = (remaining < stepDeg) ? remaining : stepDeg;
    writeAngle(currentAngle + dir * inc);
    if (stepDelay > 0) delay(stepDelay);

    degSinceSave += inc;
    if (degSinceSave >= LASTKNOWN_SAVE_EVERY_DEG) {
      prefs.putInt(NVS_KEY_LASTKNOWN, currentAngle);
      degSinceSave = 0;
    }

    if (upshift) {
      degSinceRest += inc;
      if (degSinceRest >= SERVO_REST_EVERY_DEG) {
        delay(SERVO_REST_PAUSE_MS);
        degSinceRest = 0;
      }
    }
  }
  prefs.putInt(NVS_KEY_LASTKNOWN, currentAngle);
  moving = false;
}

void setupGearControl() {
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(SERVO_PIN, LEDC_CHANNEL);

  prefs.begin(NVS_NAMESPACE, false);
  int savedTarget = prefs.getInt(NVS_KEY_TARGET, settings().gearAngles[0]);
  int lastKnown = prefs.getInt(NVS_KEY_LASTKNOWN, savedTarget);

  currentAngle = constrain(lastKnown, ANGLE_MIN, ANGLE_MAX);
  writeAngle(currentAngle);
  delay(300);

  bool downshift = savedTarget < currentAngle;
  moveServoSmooth(savedTarget, !downshift);
  currentGear = gearFromAngle(savedTarget);
}

void setGear(int gear) {
  if (moving) return;
  int n = getActiveNumGears();
  int newGear = constrain(gear, 1, n);
  if (newGear == currentGear) return;
  bool downshift = newGear < currentGear;
  int target = targetAngleForGear(newGear, downshift);
  prefs.putInt(NVS_KEY_TARGET, target);
  DBG_PRINTF("[gear] %d -> %d (angle %d)\n", currentGear, newGear, target);
  moveServoSmooth(target, !downshift);
  currentGear = newGear;
}

void shiftGear(int delta) {
  if (moving) return;
  int n = getActiveNumGears();
  int newGear = constrain(currentGear + delta, 1, n);
  if (newGear == currentGear) return;
  setGear(newGear);
}

int  getCurrentGear() { return currentGear; }
bool isGearMoving()   { return moving; }

void autoShiftUpdate(float speedKmh) {
  if (moving) return;
  unsigned long now = millis();
  if (now - lastAutoShiftMs < AUTOSHIFT_MIN_INTERVAL_MS) return;

  int n = getActiveNumGears();
  if (n < 2) return;

  float aMin = settings().tuneAutoshiftMinKmh;
  float aMax = settings().tuneAutoshiftMaxKmh;
  float span = aMax - aMin;
  if (span < 1.0f) span = 1.0f;
  float frac = (speedKmh - aMin) / span;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  int desired = 1 + (int)roundf(frac * (n - 1));
  if (desired == currentGear) return;

  float boundary = aMin + span * (float)(currentGear - 1) / (n - 1);
  if (desired > currentGear && speedKmh < boundary + AUTOSHIFT_HYST_KMH) return;
  if (desired < currentGear && speedKmh > boundary - AUTOSHIFT_HYST_KMH) return;

  lastAutoShiftMs = now;
  setGear(currentGear + (desired > currentGear ? 1 : -1));
}

void servoCalMoveTo(int angle) {
  moving = true;
  writeAngle(angle);
  moving = false;
}

int servoCalCurrentAngle() { return currentAngle; }

void servoCalSaveGearAngle(int gear, int angle) {
  settingsSetGearAngle(gear, angle);
  if (gear == currentGear) {
    prefs.putInt(NVS_KEY_TARGET, angle);
    prefs.putInt(NVS_KEY_LASTKNOWN, currentAngle);
  }
}
