#include <Arduino.h>
#include <time.h>
#include "app_clock.h"
#include "app_settings.h"

static bool          haveTime = false;
static uint32_t      baseEpoch = 0;
static unsigned long baseMs = 0;

void clockSetEpoch(uint32_t epochUtc) {

  if (epochUtc < 1000000000UL) return;
  baseEpoch = epochUtc;
  baseMs = millis();
  haveTime = true;
}

bool clockHasTime() { return haveTime; }

uint32_t clockNowEpochUtc() {
  if (!haveTime) return 0;
  return baseEpoch + (uint32_t)((millis() - baseMs) / 1000UL);
}

static bool localTm(struct tm* out) {
  if (!haveTime) return false;
  time_t local = (time_t)clockNowEpochUtc() + (time_t)settings().tzOffsetMin * 60;
  gmtime_r(&local, out);
  return true;
}

void clockFormatTime(char* out, int n) {
  struct tm t;
  if (!localTm(&t)) { snprintf(out, n, "--:--"); return; }
  snprintf(out, n, "%02d:%02d", t.tm_hour, t.tm_min);
}

void clockFormatTimeSec(char* out, int n) {
  struct tm t;
  if (!localTm(&t)) { snprintf(out, n, "--:--:--"); return; }
  snprintf(out, n, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
}

void clockFormatDate(char* out, int n) {
  struct tm t;
  if (!localTm(&t)) { snprintf(out, n, "----.--.--"); return; }
  snprintf(out, n, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}
