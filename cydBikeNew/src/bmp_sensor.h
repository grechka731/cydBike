#pragma once

struct BmpData {
  bool valid;

  float temperatureC;
  float pressureHpa;
  float altitudeM;
};

void setupBmpSensor();
BmpData readBmpData();
bool isBmpFound();
