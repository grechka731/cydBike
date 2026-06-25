#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <Preferences.h>
#include "config.h"
#include "app_settings.h"

static Preferences prefs;
static AppSettings st;

static int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int defaultGearAngle(int idx) {
  if (idx < 0) idx = 0;
  if (idx < NUM_GEARS) return GEAR_ANGLES[idx];
  return ANGLE_MIN + (ANGLE_MAX - ANGLE_MIN) * idx / (NUM_GEARS_MAX - 1);
}

void settingsBegin() {
  prefs.begin(NVS_APP_NAMESPACE, false);

  st.inverted      = prefs.getBool("inverted", false);
  st.touchSens     = prefs.getInt("touch",  TOUCH_SENS_DEFAULT);
  st.brightness    = prefs.getInt("bright", BRIGHTNESS_DEFAULT);
  st.units         = (uint8_t)prefs.getUChar("units", UNIT_KMH);
  st.numGears      = prefs.getInt("gears",  DEFAULT_NUM_GEARS);
  st.speedWarnKmh  = prefs.getFloat("warn", SPEED_WARN_DEFAULT_KMH);
  st.autoShift     = prefs.getBool("autosh", false);
  st.speedCalibPct = prefs.getInt("calib", SPEED_CALIB_DEFAULT_PCT);
  st.gear2Downshift = prefs.getInt("g2down", GEAR2_DOWNSHIFT_ANGLE);
  st.odometerKm    = prefs.getDouble("odo", 0.0);
  st.seaLevelHpa   = prefs.getFloat("slhpa", SEA_LEVEL_HPA);
  st.tzOffsetMin   = prefs.getInt("tz", CLOCK_TZ_DEFAULT_MIN);

  st.riderMassKg    = prefs.getFloat("mass", RIDER_MASS_DEFAULT_KG);
  st.recMaxSpeedKmh = prefs.getFloat("recspd", 0.0f);
  st.recMaxTripKm   = prefs.getFloat("rectrip", 0.0f);
  st.recMaxElevM    = prefs.getFloat("recelev", 0.0f);
  st.chainLubeKm    = prefs.getInt("chainkm", CHAIN_LUBE_KM_DEFAULT);
  st.serviceKm      = prefs.getInt("servkm", SERVICE_KM_DEFAULT);
  st.chainBaseKm    = prefs.getDouble("chainbase", 0.0);
  st.serviceBaseKm  = prefs.getDouble("servbase", 0.0);
  st.tuneVibRefKmh       = prefs.getFloat("tvref", VIB_REF_KMH);
  st.tuneVibRmsAtRef     = prefs.getFloat("tvrms", VIB_RMS_AT_REF);
  st.tuneVibMinRms       = prefs.getFloat("tvmin", VIB_MIN_RMS);
  st.tuneFuseVibPull     = prefs.getFloat("tvpull", FUSE_VIB_PULL);
  st.tuneMovingKmh       = prefs.getFloat("tvmove", MOVING_KMH);
  st.tuneAutoshiftMinKmh = prefs.getFloat("tashmin", AUTOSHIFT_MIN_KMH);
  st.tuneAutoshiftMaxKmh = prefs.getFloat("tashmax", AUTOSHIFT_MAX_KMH);

  char key[8];
  for (int i = 0; i < NUM_GEARS_MAX; i++) {
    snprintf(key, sizeof(key), "ga%d", i);
    st.gearAngles[i] = clampi(prefs.getInt(key, defaultGearAngle(i)), ANGLE_MIN, ANGLE_MAX);
  }

  st.touchSens     = clampi(st.touchSens,  TOUCH_SENS_MIN,  TOUCH_SENS_MAX);
  st.brightness    = clampi(st.brightness, BRIGHTNESS_MIN,  BRIGHTNESS_MAX);
  if (st.units > UNIT_MPH) st.units = UNIT_KMH;
  st.numGears      = clampi(st.numGears, 2, NUM_GEARS_MAX);
  if (st.speedWarnKmh < 0.0f) st.speedWarnKmh = 0.0f;
  if (st.speedWarnKmh > SPEED_WARN_MAX_KMH) st.speedWarnKmh = SPEED_WARN_MAX_KMH;
  st.speedCalibPct = clampi(st.speedCalibPct, SPEED_CALIB_MIN_PCT, SPEED_CALIB_MAX_PCT);
  st.gear2Downshift = clampi(st.gear2Downshift, ANGLE_MIN, ANGLE_MAX);
  if (st.odometerKm < 0.0) st.odometerKm = 0.0;
  if (st.seaLevelHpa < 900.0f || st.seaLevelHpa > 1100.0f) st.seaLevelHpa = SEA_LEVEL_HPA;
  st.tzOffsetMin = clampi(st.tzOffsetMin, -720, 840);

  if (st.riderMassKg < RIDER_MASS_MIN_KG || st.riderMassKg > RIDER_MASS_MAX_KG) st.riderMassKg = RIDER_MASS_DEFAULT_KG;
  if (st.recMaxSpeedKmh < 0.0f) st.recMaxSpeedKmh = 0.0f;
  if (st.recMaxTripKm < 0.0f) st.recMaxTripKm = 0.0f;
  if (st.recMaxElevM < 0.0f) st.recMaxElevM = 0.0f;
  st.chainLubeKm = clampi(st.chainLubeKm, MAINT_KM_MIN, MAINT_KM_MAX);
  st.serviceKm   = clampi(st.serviceKm, MAINT_KM_MIN, MAINT_KM_MAX);
  if (st.chainBaseKm < 0.0) st.chainBaseKm = 0.0;
  if (st.serviceBaseKm < 0.0) st.serviceBaseKm = 0.0;
  if (st.tuneVibRmsAtRef < 0.01f) st.tuneVibRmsAtRef = VIB_RMS_AT_REF;
  if (st.tuneAutoshiftMaxKmh <= st.tuneAutoshiftMinKmh) st.tuneAutoshiftMaxKmh = st.tuneAutoshiftMinKmh + 1.0f;
}

const AppSettings& settings() { return st; }

void settingsSetInverted(bool v) {
  st.inverted = v;
  prefs.putBool("inverted", v);
}

void settingsSetTouchSens(int v) {
  st.touchSens = clampi(v, TOUCH_SENS_MIN, TOUCH_SENS_MAX);
  prefs.putInt("touch", st.touchSens);
}

void settingsSetBrightness(int v) {
  st.brightness = clampi(v, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
  prefs.putInt("bright", st.brightness);
}

void settingsSetUnits(uint8_t v) {
  st.units = (v > UNIT_MPH) ? UNIT_KMH : v;
  prefs.putUChar("units", st.units);
}

void settingsSetNumGears(int v) {
  st.numGears = clampi(v, 2, NUM_GEARS_MAX);
  prefs.putInt("gears", st.numGears);
}

void settingsSetSpeedWarn(float kmh) {
  if (kmh < 0.0f) kmh = 0.0f;
  if (kmh > SPEED_WARN_MAX_KMH) kmh = SPEED_WARN_MAX_KMH;
  st.speedWarnKmh = kmh;
  prefs.putFloat("warn", kmh);
}

void settingsSetAutoShift(bool v) {
  st.autoShift = v;
  prefs.putBool("autosh", v);
}

void settingsSetSpeedCalibPct(int pct) {
  st.speedCalibPct = clampi(pct, SPEED_CALIB_MIN_PCT, SPEED_CALIB_MAX_PCT);
  prefs.putInt("calib", st.speedCalibPct);
}

void settingsSetGearAngle(int gear, int angle) {
  int idx = gear - 1;
  if (idx < 0 || idx >= NUM_GEARS_MAX) return;
  st.gearAngles[idx] = clampi(angle, ANGLE_MIN, ANGLE_MAX);
  char key[8];
  snprintf(key, sizeof(key), "ga%d", idx);
  prefs.putInt(key, st.gearAngles[idx]);
}

void settingsSetGear2Downshift(int angle) {
  st.gear2Downshift = clampi(angle, ANGLE_MIN, ANGLE_MAX);
  prefs.putInt("g2down", st.gear2Downshift);
}

void settingsSetOdometer(double km) {
  if (km < 0.0) km = 0.0;
  st.odometerKm = km;
  prefs.putDouble("odo", km);
}

void settingsSetSeaLevelHpa(float hpa) {
  if (hpa < 900.0f) hpa = 900.0f;
  if (hpa > 1100.0f) hpa = 1100.0f;
  st.seaLevelHpa = hpa;
  prefs.putFloat("slhpa", hpa);
}

void settingsSetSeaLevelFromAltitude(float knownAltM, float measuredHpa) {
  if (measuredHpa < 300.0f) return;
  float slp = measuredHpa / powf(1.0f - (knownAltM / 44330.0f), 5.255f);
  settingsSetSeaLevelHpa(slp);
}

void settingsSetTzOffsetMin(int minutes) {
  st.tzOffsetMin = clampi(minutes, -720, 840);
  prefs.putInt("tz", st.tzOffsetMin);
}

float toDisplaySpeed(float kmh) {
  return (st.units == UNIT_MPH) ? kmh * KMH_TO_MPH : kmh;
}

float fromDisplaySpeed(float v) {
  return (st.units == UNIT_MPH) ? v / KMH_TO_MPH : v;
}

const char* speedUnitLabel() {
  return (st.units == UNIT_MPH) ? "MPH" : "KM/H";
}

float toDisplayDistance(float km) {
  return (st.units == UNIT_MPH) ? km * KMH_TO_MPH : km;
}

const char* distUnitLabel() {
  return (st.units == UNIT_MPH) ? "mi" : "km";
}

void settingsSetRiderMass(float kg) {
  if (kg < RIDER_MASS_MIN_KG) kg = RIDER_MASS_MIN_KG;
  if (kg > RIDER_MASS_MAX_KG) kg = RIDER_MASS_MAX_KG;
  st.riderMassKg = kg;
  prefs.putFloat("mass", kg);
}

void settingsConsiderRecords(float speedKmh, float tripKm, float elevM) {
  if (speedKmh > st.recMaxSpeedKmh) { st.recMaxSpeedKmh = speedKmh; prefs.putFloat("recspd", speedKmh); }
  if (tripKm   > st.recMaxTripKm)   { st.recMaxTripKm = tripKm;     prefs.putFloat("rectrip", tripKm); }
  if (elevM    > st.recMaxElevM)    { st.recMaxElevM = elevM;       prefs.putFloat("recelev", elevM); }
}

void settingsResetRecords() {
  st.recMaxSpeedKmh = 0.0f;
  st.recMaxTripKm = 0.0f;
  st.recMaxElevM = 0.0f;
  prefs.putFloat("recspd", 0.0f);
  prefs.putFloat("rectrip", 0.0f);
  prefs.putFloat("recelev", 0.0f);
}

void settingsSetChainLubeKm(int km) {
  st.chainLubeKm = clampi(km, MAINT_KM_MIN, MAINT_KM_MAX);
  prefs.putInt("chainkm", st.chainLubeKm);
}

void settingsSetServiceKm(int km) {
  st.serviceKm = clampi(km, MAINT_KM_MIN, MAINT_KM_MAX);
  prefs.putInt("servkm", st.serviceKm);
}

void settingsMarkChainLubed(double odoKm) {
  if (odoKm < 0.0) odoKm = 0.0;
  st.chainBaseKm = odoKm;
  prefs.putDouble("chainbase", odoKm);
}

void settingsMarkServiceDone(double odoKm) {
  if (odoKm < 0.0) odoKm = 0.0;
  st.serviceBaseKm = odoKm;
  prefs.putDouble("servbase", odoKm);
}

float settingsChainKmLeft(double odoKm) {
  return (float)((double)st.chainLubeKm - (odoKm - st.chainBaseKm));
}

float settingsServiceKmLeft(double odoKm) {
  return (float)((double)st.serviceKm - (odoKm - st.serviceBaseKm));
}

bool settingsSetTune(const char* key, float value) {
  if (!key) return false;
  if (!strcmp(key, "vref"))   { if (value < 2.0f) value = 2.0f; if (value > 40.0f) value = 40.0f; st.tuneVibRefKmh = value; prefs.putFloat("tvref", value); return true; }
  if (!strcmp(key, "vrms"))   { if (value < 0.05f) value = 0.05f; if (value > 3.0f) value = 3.0f; st.tuneVibRmsAtRef = value; prefs.putFloat("tvrms", value); return true; }
  if (!strcmp(key, "vmin"))   { if (value < 0.01f) value = 0.01f; if (value > 1.0f) value = 1.0f; st.tuneVibMinRms = value; prefs.putFloat("tvmin", value); return true; }
  if (!strcmp(key, "pull"))   { if (value < 0.05f) value = 0.05f; if (value > 3.0f) value = 3.0f; st.tuneFuseVibPull = value; prefs.putFloat("tvpull", value); return true; }
  if (!strcmp(key, "move"))   { if (value < 0.2f) value = 0.2f; if (value > 5.0f) value = 5.0f; st.tuneMovingKmh = value; prefs.putFloat("tvmove", value); return true; }
  if (!strcmp(key, "ashmin")) { if (value < 2.0f) value = 2.0f; if (value > 30.0f) value = 30.0f; st.tuneAutoshiftMinKmh = value; prefs.putFloat("tashmin", value); return true; }
  if (!strcmp(key, "ashmax")) { if (value < 10.0f) value = 10.0f; if (value > 60.0f) value = 60.0f; st.tuneAutoshiftMaxKmh = value; prefs.putFloat("tashmax", value); return true; }
  return false;
}
