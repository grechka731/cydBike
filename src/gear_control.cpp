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

// Power-saving servo-release state. When the servo is idle we stop sending PWM
// pulses so it de-energizes and stops drawing holding current from the battery.
static bool servoEngaged = false;
static unsigned long releaseDeadlineMs = 0;

// Timestamp of the last time the servo was de-energized.
// Used to enforce SERVO_REENGAGE_COOLDOWN_MS before the next move.
static unsigned long lastReleaseMs = 0;

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
  servoEngaged = true;
  // Push the idle-release deadline forward on every write so we never release
  // mid-move; the servo only relaxes after holding the final position for
  // SERVO_HOLD_AFTER_MOVE_MS.
  releaseDeadlineMs = millis() + SERVO_HOLD_AFTER_MOVE_MS;
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

// Find the gear (1-based) and angle with the smallest stored angle among
// the currently active gears - i.e. the lowest/easiest gear.
static int minAngleGear(int* outAngle) {
  int n = getActiveNumGears();
  int bestGear = 1;
  int bestAngle = settings().gearAngles[0];
  for (int i = 1; i < n; i++) {
    if (settings().gearAngles[i] < bestAngle) {
      bestAngle = settings().gearAngles[i];
      bestGear = i + 1;
    }
  }
  if (outAngle) *outAngle = bestAngle;
  return bestGear;
}

static void moveServoSmooth(int targetAngle, bool upshift) {
  targetAngle = constrain(targetAngle, ANGLE_MIN, ANGLE_MAX);
  moving = true;
  int dir = (targetAngle >= currentAngle) ? 1 : -1;

  int stepDeg   = upshift ? SERVO_STEP_DEG_UP      : SERVO_STEP_DEG_DOWN;
  int stepDelay = upshift ? SERVO_STEP_DELAY_UP_MS : SERVO_STEP_DELAY_DOWN_MS;
  if (stepDeg < 1) stepDeg = 1;

  // --- Re-engage cooldown ---
  // If the servo was recently de-energized, wait before the first pulse
  // so the battery rail has time to recover from whatever drained it.
  if (!servoEngaged && lastReleaseMs > 0) {
    unsigned long elapsed = millis() - lastReleaseMs;
    if (elapsed < SERVO_REENGAGE_COOLDOWN_MS) {
      delay(SERVO_REENGAGE_COOLDOWN_MS - elapsed);
    }
  }

  int degSinceRest = 0;
  int degSinceSave = 0;
  int stepCount = 0;   // for soft-start tracking

  while (currentAngle != targetAngle) {
    int remaining = abs(targetAngle - currentAngle);
    int inc = (remaining < stepDeg) ? remaining : stepDeg;
    writeAngle(currentAngle + dir * inc);

    // --- Soft-start: extra delay on the first few steps ---
    // When the servo starts from rest it draws the highest inrush current.
    // Extra pause here spreads that spike across time.
    int extraDelay = (stepCount < SERVO_SOFTSTART_STEPS) ? SERVO_SOFTSTART_EXTRA_MS : 0;
    stepCount++;

    if (stepDelay + extraDelay > 0) delay(stepDelay + extraDelay);

    degSinceSave += inc;
    if (degSinceSave >= LASTKNOWN_SAVE_EVERY_DEG) {
      prefs.putInt(NVS_KEY_LASTKNOWN, currentAngle);
      degSinceSave = 0;
    }

    if (upshift || SERVO_REST_ON_DOWNSHIFT) {
      degSinceRest += inc;
      if (degSinceRest >= SERVO_REST_EVERY_DEG) {
        delay(SERVO_REST_PAUSE_MS);   // let the battery rail recover between current spikes
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
  int lastKnown   = prefs.getInt(NVS_KEY_LASTKNOWN, savedTarget);
  int restAngle   = constrain(lastKnown, ANGLE_MIN, ANGLE_MAX);

  // Let the supply rail (display + ESP32 already powered) stabilize before the
  // servo pulls its first current spike, so we don't brown out at boot.
  delay(SERVO_BOOT_SETTLE_MS);

  // While de-energized (no PWM since the last shutdown), the derailleur's
  // own spring/cable tension can relax the arm back towards the lowest gear
  // - away from whatever angle we last remembered (restAngle). If we then
  // engaged the PWM with a single direct write at restAngle, the servo would
  // have to snap forward, in one un-eased jump, from wherever it had really
  // relaxed to. That single hard jump - right after the splash screen, and
  // worst when restAngle is 2nd/3rd gear - is what was browning out the
  // battery at boot.
  // Fix: assume the arm relaxed to the lowest-gear angle, and gently climb
  // back up to restAngle using the same paced/soft-start stepping as a
  // normal shift, so the current ramps up smoothly no matter where the arm
  // really settled.
  int lowAngle;
  minAngleGear(&lowAngle);
  currentAngle = lowAngle;     // assumed starting point; no PWM written yet
  if (restAngle != lowAngle) {
    moveServoSmooth(restAngle, true);   // ease up gently, like an upshift
  } else {
    writeAngle(restAngle);
  }

  currentGear = gearFromAngle(restAngle);
  // Keep the saved target in sync with where we actually are, so nothing
  // tries to "catch up" to a stale, higher target later.
  prefs.putInt(NVS_KEY_TARGET, restAngle);
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

// Call this every loop(): once the servo has held its position long enough,
// stop the PWM pulses so the servo relaxes and stops drawing holding current.
// This is the main "let the battery rest" mechanism.
void gearControlUpdate() {
  if (!SERVO_RELEASE_WHEN_IDLE) return;
  if (moving) return;
  if (servoEngaged && (long)(millis() - releaseDeadlineMs) >= 0) {
    ledcWrite(LEDC_CHANNEL, 0);   // 0 duty => no pulses => servo de-energizes
    servoEngaged = false;
    lastReleaseMs = millis();     // record when we released, for cooldown on next move
  }
}

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

// Called once when the rider has stopped and the display goes to sleep
// (see SLEEP_TIMEOUT_MS) - i.e. the device is likely about to be switched
// off. We smoothly (no jerking) move the servo to the gear with the
// smallest stored angle and persist it as both the saved target *and* the
// last-known angle. That way, whenever power is actually cut, the servo is
// already resting at the gear that needs the least movement/current on the
// next boot, instead of having to climb back up to whatever gear was last
// selected while riding - which is what was causing the battery to sag and
// brown out both the servo and the CYD on power-up.
void gearControlEnterParkMode() {
  if (moving) return;
  int parkAngle;
  int parkGear = minAngleGear(&parkAngle);

  if (currentAngle != parkAngle) {
    bool downshift = parkAngle < currentAngle;
    moveServoSmooth(parkAngle, !downshift);
  }
  currentGear = parkGear;
  prefs.putInt(NVS_KEY_TARGET, parkAngle);
  prefs.putInt(NVS_KEY_LASTKNOWN, parkAngle);
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