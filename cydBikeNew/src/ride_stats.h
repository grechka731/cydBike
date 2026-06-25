#pragma once

void rideStatsBegin();
void rideStatsUpdate();

float         rideTripKm();
float         rideOdometerKm();
float         rideMaxSpeedKmh();
float         rideAvgSpeedKmh();
unsigned long rideTimeSec();
float         rideGradePercent();
float         rideCadenceRpm();
float         rideAvgCadenceRpm();
float         rideCurrentPowerW();
float         rideEnergyKcal();

float rideMinTempC();
float rideMaxTempC();
float rideMinPressureHpa();
float rideMaxPressureHpa();
bool  rideEnvValid();

float       rideAltitudeM();
float       rideElevationGainM();
float       ridePressureTrendHpa();
const char* ridePressureTrendLabel();

void rideResetTrip();
void rideResetOdometer();

const float* rideSpeedHistory(int* count, int* head, int* capacity);

const float* rideAltHistory(int* count, int* head, int* capacity);
