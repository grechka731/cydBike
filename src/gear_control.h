#pragma once

void setupGearControl();
void gearControlUpdate();
void shiftGear(int delta);
void setGear(int gear);
int  getCurrentGear();
bool isGearMoving();

void autoShiftUpdate(float speedKmh);

// Smoothly (no jerking) park the servo at the gear with the smallest stored
// angle, and persist that as the saved/last-known angle. Call this when the
// rider is likely done and about to power the device off (e.g. when the
// display goes to sleep after a period of inactivity), so that whenever
// power is actually cut, the next boot has little or nothing to climb back
// up to, avoiding the battery-sag brownout caused by a big move on power-up.
void gearControlEnterParkMode();

void servoCalMoveTo(int angle);
int  servoCalCurrentAngle();
void servoCalSaveGearAngle(int gear, int angle);

int  getActiveNumGears();
int  gearStoredAngle(int gear);
