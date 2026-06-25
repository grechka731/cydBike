#pragma once

void  setupMpuSpeed();
void  updateMpuSpeed();
float getSpeedKmh();
float getTripDistanceKm();

float getVibSpeedKmh();
float getVibRms();
float getIntegratorSpeedKmh();
float getSpeedConfidence();
bool  isSpeedMoving();
float getCadenceRpm();
