#pragma once

void setupGearControl();
void shiftGear(int delta);
void setGear(int gear);
int  getCurrentGear();
bool isGearMoving();

void autoShiftUpdate(float speedKmh);

void servoCalMoveTo(int angle);
int  servoCalCurrentAngle();
void servoCalSaveGearAngle(int gear, int angle);

int  getActiveNumGears();
int  gearStoredAngle(int gear);
