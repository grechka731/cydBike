#pragma once

struct MpuData {
  bool valid;
  float accelX_g, accelY_g, accelZ_g;
  float gyroX_dps, gyroY_dps, gyroZ_dps;
  float temperatureC;
  float pitchDeg;
  float rollDeg;
};

void setupMpuSensor();
MpuData readMpuData();
bool isMpuFound();
