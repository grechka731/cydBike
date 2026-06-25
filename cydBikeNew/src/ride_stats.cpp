#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "ride_stats.h"
#include "app_settings.h"
#include "mpu_speed.h"
#include "mpu_sensor.h"
#include "bmp_sensor.h"

static double        tripKm      = 0.0;
static double        odoKm       = 0.0;
static double        pendingOdo  = 0.0;
static float         maxSpeed    = 0.0f;
static double        movingSec   = 0.0;
static float         grade       = 0.0f;

static double        cadSum      = 0.0;
static double        cadTime     = 0.0;
static float         powerW      = 0.0f;
static double        energyJ     = 0.0;
static float         prevSpeedMs = 0.0f;
static bool          prevSpeedValid = false;
static unsigned long lastRecMs   = 0;

static bool          envValid    = false;
static float         minTemp     = 0.0f, maxTemp = 0.0f;
static float         minPress    = 0.0f, maxPress = 0.0f;

static bool          altValid    = false;
static float         altM        = 0.0f;
static float         lastClimbAlt = 0.0f;
static double        elevGainM   = 0.0;
static bool          pressInit   = false;
static float         pressBaseline = 0.0f;
static float         pressNow    = 0.0f;

static unsigned long lastMs      = 0;
static unsigned long lastHistMs  = 0;
static unsigned long lastAltHistMs = 0;

static float         hist[SPEED_HIST_LEN];
static int           histCount   = 0;
static int           histHead    = 0;

static float         altHist[ALT_HIST_LEN];
static int           altHistCount = 0;
static int           altHistHead  = 0;

static const double  ODO_SAVE_KM = 0.05;

static const float   CLIMB_HYST_M = 1.0f;

void rideStatsBegin() {
  odoKm = settings().odometerKm;
  lastMs = millis();
  lastHistMs = lastMs;
  lastAltHistMs = lastMs;
  lastRecMs = lastMs;
  for (int i = 0; i < SPEED_HIST_LEN; i++) hist[i] = 0.0f;
  for (int i = 0; i < ALT_HIST_LEN; i++)   altHist[i] = 0.0f;
}

static void pushHistory(float kmh) {
  hist[histHead] = kmh;
  histHead = (histHead + 1) % SPEED_HIST_LEN;
  if (histCount < SPEED_HIST_LEN) histCount++;
}

static void pushAltHistory(float m) {
  altHist[altHistHead] = m;
  altHistHead = (altHistHead + 1) % ALT_HIST_LEN;
  if (altHistCount < ALT_HIST_LEN) altHistCount++;
}

void rideStatsUpdate() {
  unsigned long now = millis();
  float dt = (now - lastMs) / 1000.0f;
  lastMs = now;
  if (dt <= 0.0f) return;
  if (dt > 1.0f) dt = 0.0f;

  float v = getSpeedKmh();

  if (dt > 0.0f) {
    double dKm = (double)v / 3600.0 * dt;
    tripKm += dKm;
    odoKm  += dKm;
    pendingOdo += dKm;
    if (v > settings().tuneMovingKmh) movingSec += dt;
  }

  if (v > maxSpeed) maxSpeed = v;

  {
    float g0 = 9.80665f;
    float gf = grade / 100.0f;
    float denom = sqrtf(1.0f + gf * gf);
    float sinT = gf / denom;
    float cosT = 1.0f / denom;
    float mass = settings().riderMassKg;
    float vMs = v / 3.6f;
    float a = 0.0f;
    if (prevSpeedValid && dt > 0.0f) a = (vMs - prevSpeedMs) / dt;
    prevSpeedMs = vMs;
    prevSpeedValid = true;
    float fRoll = POWER_CRR * mass * g0 * cosT;
    float fGrav = mass * g0 * sinT;
    float fAero = 0.5f * POWER_RHO * POWER_CDA * vMs * vMs;
    float fAcc  = mass * a;
    float pMech = (fRoll + fGrav + fAero + fAcc) * vMs;
    if (vMs < 0.3f) pMech = 0.0f;
    if (pMech < 0.0f) pMech = 0.0f;
    if (pMech > 2000.0f) pMech = 2000.0f;
    powerW = powerW * 0.7f + pMech * 0.3f;
    if (dt > 0.0f) energyJ += (double)(powerW / HUMAN_EFFICIENCY) * dt;
    float cad = getCadenceRpm();
    if (dt > 0.0f && cad > 0.0f && v > settings().tuneMovingKmh) {
      cadSum += (double)cad * dt;
      cadTime += dt;
    }
  }

  if (pendingOdo >= ODO_SAVE_KM) {
    settingsSetOdometer(odoKm);
    pendingOdo = 0.0;
  }

  MpuData m = readMpuData();
  if (m.valid) {
    float g = tanf(m.pitchDeg * (float)PI / 180.0f) * 100.0f;
    if (g > 60.0f) g = 60.0f;
    if (g < -60.0f) g = -60.0f;
    grade = grade * 0.9f + g * 0.1f;
  }

  BmpData b = readBmpData();
  if (b.valid) {
    if (!envValid) {
      envValid = true;
      minTemp = maxTemp = b.temperatureC;
      minPress = maxPress = b.pressureHpa;
    } else {
      if (b.temperatureC < minTemp) minTemp = b.temperatureC;
      if (b.temperatureC > maxTemp) maxTemp = b.temperatureC;
      if (b.pressureHpa  < minPress) minPress = b.pressureHpa;
      if (b.pressureHpa  > maxPress) maxPress = b.pressureHpa;
    }

    pressNow = b.pressureHpa;

    if (!altValid) {
      altValid = true;
      altM = b.altitudeM;
      lastClimbAlt = b.altitudeM;
    } else {
      altM = altM * 0.9f + b.altitudeM * 0.1f;
    }

    if (altM > lastClimbAlt + CLIMB_HYST_M) {
      elevGainM += (altM - lastClimbAlt);
      lastClimbAlt = altM;
    } else if (altM < lastClimbAlt) {
      lastClimbAlt = altM;
    }

    if (dt > 0.0f) {
      if (!pressInit) {
        pressInit = true;
        pressBaseline = pressNow;
      } else {
        float a = dt / (BARO_TREND_TAU_S + dt);
        pressBaseline += (pressNow - pressBaseline) * a;
      }
    }
  }

  if (now - lastHistMs >= SPEED_HIST_INTERVAL_MS) {
    lastHistMs = now;
    pushHistory(v);
  }
  if (altValid && now - lastAltHistMs >= ALT_HIST_INTERVAL_MS) {
    lastAltHistMs = now;
    pushAltHistory(altM);
  }

  if (now - lastRecMs >= 15000) {
    lastRecMs = now;
    settingsConsiderRecords(maxSpeed, (float)tripKm, (float)elevGainM);
  }
}

float rideTripKm()      { return (float)tripKm; }
float rideOdometerKm()  { return (float)odoKm; }
float rideMaxSpeedKmh() { return maxSpeed; }

float rideAvgSpeedKmh() {
  if (movingSec < 1.0) return 0.0f;
  return (float)(tripKm / (movingSec / 3600.0));
}

unsigned long rideTimeSec() { return (unsigned long)movingSec; }
float rideGradePercent()    { return grade; }
float rideCadenceRpm()      { return getCadenceRpm(); }
float rideAvgCadenceRpm()   { return (cadTime < 1.0) ? 0.0f : (float)(cadSum / cadTime); }
float rideCurrentPowerW()   { return powerW; }
float rideEnergyKcal()      { return (float)(energyJ / 4184.0); }

float rideMinTempC()        { return minTemp; }
float rideMaxTempC()        { return maxTemp; }
float rideMinPressureHpa()  { return minPress; }
float rideMaxPressureHpa()  { return maxPress; }
bool  rideEnvValid()        { return envValid; }

float rideAltitudeM()       { return altValid ? altM : 0.0f; }
float rideElevationGainM()  { return (float)elevGainM; }
float ridePressureTrendHpa(){ return pressInit ? (pressNow - pressBaseline) : 0.0f; }

const char* ridePressureTrendLabel() {
  if (!pressInit) return "--";
  float t = pressNow - pressBaseline;
  if (t >=  BARO_TREND_THRESH_HPA) return "RISING";
  if (t <= -BARO_TREND_THRESH_HPA) return "FALLING";
  return "STEADY";
}

void rideResetTrip() {
  settingsConsiderRecords(maxSpeed, (float)tripKm, (float)elevGainM);
  tripKm    = 0.0;
  maxSpeed  = 0.0f;
  movingSec = 0.0;
  cadSum    = 0.0;
  cadTime   = 0.0;
  powerW    = 0.0f;
  energyJ   = 0.0;
  prevSpeedValid = false;
  envValid  = false;
  histCount = 0;
  histHead  = 0;
  for (int i = 0; i < SPEED_HIST_LEN; i++) hist[i] = 0.0f;

  altHistCount = 0;
  altHistHead  = 0;
  for (int i = 0; i < ALT_HIST_LEN; i++) altHist[i] = 0.0f;
  elevGainM = 0.0;
  lastClimbAlt = altM;
}

void rideResetOdometer() {
  odoKm = 0.0;
  pendingOdo = 0.0;
  settingsSetOdometer(0.0);
}

const float* rideSpeedHistory(int* count, int* head, int* capacity) {
  if (count)    *count = histCount;
  if (head)     *head = histHead;
  if (capacity) *capacity = SPEED_HIST_LEN;
  return hist;
}

const float* rideAltHistory(int* count, int* head, int* capacity) {
  if (count)    *count = altHistCount;
  if (head)     *head = altHistHead;
  if (capacity) *capacity = ALT_HIST_LEN;
  return altHist;
}
