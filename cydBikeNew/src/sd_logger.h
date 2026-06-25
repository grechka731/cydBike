#pragma once
#include <stdint.h>
#include "config.h"

struct RideFileEntry {
  int  number;
  char name[24];
};

struct RideFileSummary {
  bool          valid;
  float         distKm;
  unsigned long timeSec;
  float         maxSpeedKmh;
  float         avgSpeedKmh;
  float         elevGainM;
  float         avgCadRpm;
  int           samples;
};

void sdLoggerBegin();
bool sdLoggerAvailable();
const char* sdCurrentFile();
int  sdCurrentRideNumber();

void sdLoggerUpdate();

int  sdListRideFiles(RideFileEntry* out, int maxN);
bool sdReadRideSummary(const char* name, RideFileSummary* out);
int  sdReadRideSeries(const char* name, float* speed, float* alt, int maxPts);
void sdAppendMaintenance(const char* type, double odoKm, uint32_t epoch);
