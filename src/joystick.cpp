#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "joystick.h"
#include "gear_control.h"

enum JoyDir { JOY_NONE = 0, JOY_RIGHT, JOY_LEFT };

static JoyDir lastJoyState = JOY_NONE;
static JoyDir candidateState = JOY_NONE;
static unsigned long candidateSince = 0;
static unsigned long lastDiagPrintTime = 0;
static unsigned long lastShiftTime = 0;

static JoyDir classifyJoy(int rawX) {
  if (rawX >= JOY_RIGHT_THRESHOLD) return JOY_RIGHT;
  if (rawX <= JOY_LEFT_THRESHOLD)  return JOY_LEFT;
  return JOY_NONE;
}

static void handleJoyDir(JoyDir dir) {
  switch (dir) {
    case JOY_RIGHT:
      DBG_PRINTLN("[joy] Right: gear UP");
      shiftGear(+1);
      break;
    case JOY_LEFT:
      DBG_PRINTLN("[joy] Left: gear DOWN");
      shiftGear(-1);
      break;
    default:
      break;
  }
}

void setupJoystick() {
  pinMode(JOY_VRX_PIN, INPUT);
  pinMode(JOY_VRY_PIN, INPUT);
  analogReadResolution(12);
}

void updateJoystick() {
  unsigned long now = millis();
  int rawX = analogRead(JOY_VRX_PIN);

  if (now - lastDiagPrintTime > 300) {
    lastDiagPrintTime = now;
    int rawY = analogRead(JOY_VRY_PIN);
    DBG_PRINTF("[joy] X raw: %d   Y raw: %d (Y reserved)\n", rawX, rawY);
  }

  JoyDir dir = classifyJoy(rawX);

  if (dir != candidateState) {
    candidateState = dir;
    candidateSince = now;
  }
  bool stable = (now - candidateSince >= (unsigned long)JOY_DEBOUNCE_MS);

  if (stable && candidateState != lastJoyState) {

    lastJoyState = candidateState;
    if (lastJoyState != JOY_NONE && !isGearMoving()) {
      handleJoyDir(lastJoyState);
      lastShiftTime = now;
    }
  } else if (stable && lastJoyState != JOY_NONE && !isGearMoving()
             && (now - lastShiftTime >= (unsigned long)JOY_REPEAT_DELAY_MS)) {

    handleJoyDir(lastJoyState);
    lastShiftTime = now;
  }
}
