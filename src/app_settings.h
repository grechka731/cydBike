#pragma once
#include <stdint.h>
#include "config.h"

enum SpeedUnit { UNIT_KMH = 0, UNIT_MPH = 1 };

struct AppSettings {
  bool     inverted;
  int      touchSens;
  int      brightness;
  uint8_t  units;
  int      numGears;
  float    speedWarnKmh;
  bool     autoShift;
  int      speedCalibPct;
  int      gearAngles[NUM_GEARS_MAX];
  int      gear2Downshift;
  double   odometerKm;
  float    seaLevelHpa;
  int      tzOffsetMin;

  float    riderMassKg;

  float    recMaxSpeedKmh;
  float    recMaxTripKm;
  float    recMaxElevM;

  int      chainLubeKm;
  int      serviceKm;
  double   chainBaseKm;
  double   serviceBaseKm;

  float    tuneVibRefKmh;
  float    tuneVibRmsAtRef;
  float    tuneVibMinRms;
  float    tuneFuseVibPull;
  float    tuneMovingKmh;
  float    tuneAutoshiftMinKmh;
  float    tuneAutoshiftMaxKmh;
};

void settingsBegin();
const AppSettings& settings();

void settingsSetInverted(bool v);
void settingsSetTouchSens(int v);
void settingsSetBrightness(int v);
void settingsSetUnits(uint8_t v);
void settingsSetNumGears(int v);
void settingsSetSpeedWarn(float kmh);
void settingsSetAutoShift(bool v);
void settingsSetSpeedCalibPct(int pct);
void settingsSetGearAngle(int gear, int angle);
void settingsSetGear2Downshift(int angle);
void settingsSetOdometer(double km);
void settingsSetSeaLevelHpa(float hpa);
void settingsSetSeaLevelFromAltitude(float knownAltM, float measuredHpa);
void settingsSetTzOffsetMin(int minutes);

void settingsSetRiderMass(float kg);

void settingsConsiderRecords(float speedKmh, float tripKm, float elevM);
void settingsResetRecords();

void settingsSetChainLubeKm(int km);
void settingsSetServiceKm(int km);
void settingsMarkChainLubed(double odoKm);
void settingsMarkServiceDone(double odoKm);
float settingsChainKmLeft(double odoKm);
float settingsServiceKmLeft(double odoKm);

bool settingsSetTune(const char* key, float value);

int  defaultGearAngle(int idx);

float       toDisplaySpeed(float kmh);
float       fromDisplaySpeed(float v);
const char* speedUnitLabel();
float       toDisplayDistance(float km);
const char* distUnitLabel();
