#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "config.h"
#include "debug.h"
#include "mpu_sensor.h"

static const uint8_t MPU_ADDR_PRIMARY = 0x68;
static const uint8_t MPU_ADDR_SECONDARY = 0x69;

static const uint8_t REG_PWR_MGMT_1   = 0x6B;
static const uint8_t REG_SMPLRT_DIV   = 0x19;
static const uint8_t REG_CONFIG       = 0x1A;
static const uint8_t REG_GYRO_CONFIG  = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;

static uint8_t mpuAddr = 0;
static bool mpuFound = false;

static const float ACCEL_SCALE = 16384.0f;
static const float GYRO_SCALE  = 131.0f;

static float pitchOffset = 0.0f;
static float rollOffset  = 0.0f;
static float gyroXOffset = 0.0f;
static float gyroYOffset = 0.0f;
static float gyroZOffset = 0.0f;

static bool writeRegister(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

static bool readRegisters(uint8_t addr, uint8_t startReg, uint8_t *buffer, uint8_t length) {
  Wire.beginTransmission(addr);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  uint8_t received = Wire.requestFrom(addr, length);
  if (received != length) {
    return false;
  }
  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = Wire.read();
  }
  return true;
}

static bool probeAddress(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

bool isMpuFound() {
  return mpuFound;
}

static void calibrateMpu() {
  DBG_PRINTLN("[mpu] calibration start...");
  float pitchSum = 0, rollSum = 0, gyroXSum = 0, gyroYSum = 0, gyroZSum = 0;
  const int samples = 150;
  int validSamples = 0;
  delay(200);
  for (int i = 0; i < samples; i++) {
    MpuData data = readMpuData();
    if (data.valid) {
      pitchSum += data.pitchDeg;
      rollSum  += data.rollDeg;
      gyroXSum += data.gyroX_dps;
      gyroYSum += data.gyroY_dps;
      gyroZSum += data.gyroZ_dps;
      validSamples++;
    }
    delay(5);
  }
  if (validSamples > 0) {
    pitchOffset = pitchSum / validSamples;
    rollOffset  = rollSum / validSamples;
    gyroXOffset = gyroXSum / validSamples;
    gyroYOffset = gyroYSum / validSamples;
    gyroZOffset = gyroZSum / validSamples;
    DBG_PRINTLN("[mpu] calibration done");
  } else {
    DBG_PRINTLN("[mpu] calibration failed");
  }
}

void setupMpuSensor() {
  if (probeAddress(MPU_ADDR_PRIMARY)) {
    mpuAddr = MPU_ADDR_PRIMARY;
  } else if (probeAddress(MPU_ADDR_SECONDARY)) {
    mpuAddr = MPU_ADDR_SECONDARY;
  } else {
    mpuFound = false;
    DBG_PRINTLN("[mpu] MPU6050 not responding");
    return;
  }
  bool ok = true;
  ok &= writeRegister(mpuAddr, REG_PWR_MGMT_1, 0x00);
  delay(50);
  ok &= writeRegister(mpuAddr, REG_SMPLRT_DIV, 0x07);
  ok &= writeRegister(mpuAddr, REG_CONFIG, 0x00);
  ok &= writeRegister(mpuAddr, REG_GYRO_CONFIG, 0x00);
  ok &= writeRegister(mpuAddr, REG_ACCEL_CONFIG, 0x00);
  mpuFound = ok;
  if (mpuFound) {
    DBG_PRINTF("[mpu] initialized at 0x%02X\n", mpuAddr);
    calibrateMpu();
  } else {
    DBG_PRINTLN("[mpu] register write failed");
  }
}

MpuData readMpuData() {
  MpuData data;
  if (!mpuFound) {
    data.valid = false;
    data.accelX_g = data.accelY_g = data.accelZ_g = 0.0f;
    data.gyroX_dps = data.gyroY_dps = data.gyroZ_dps = 0.0f;
    data.temperatureC = 0.0f;
    data.pitchDeg = data.rollDeg = 0.0f;
    return data;
  }
  uint8_t raw[14];
  if (!readRegisters(mpuAddr, REG_ACCEL_XOUT_H, raw, 14)) {
    data.valid = false;
    data.accelX_g = data.accelY_g = data.accelZ_g = 0.0f;
    data.gyroX_dps = data.gyroY_dps = data.gyroZ_dps = 0.0f;
    data.temperatureC = 0.0f;
    data.pitchDeg = data.rollDeg = 0.0f;
    return data;
  }
  int16_t accelXraw = (int16_t)((raw[0] << 8) | raw[1]);
  int16_t accelYraw = (int16_t)((raw[2] << 8) | raw[3]);
  int16_t accelZraw = (int16_t)((raw[4] << 8) | raw[5]);
  int16_t tempRaw   = (int16_t)((raw[6] << 8) | raw[7]);
  int16_t gyroXraw  = (int16_t)((raw[8] << 8) | raw[9]);
  int16_t gyroYraw  = (int16_t)((raw[10] << 8) | raw[11]);
  int16_t gyroZraw  = (int16_t)((raw[12] << 8) | raw[13]);
  data.valid = true;
  data.accelX_g = accelXraw / ACCEL_SCALE;
  data.accelY_g = accelYraw / ACCEL_SCALE;
  data.accelZ_g = accelZraw / ACCEL_SCALE;
  data.gyroX_dps = (gyroXraw / GYRO_SCALE) - gyroXOffset;
  data.gyroY_dps = (gyroYraw / GYRO_SCALE) - gyroYOffset;
  data.gyroZ_dps = (gyroZraw / GYRO_SCALE) - gyroZOffset;
  data.temperatureC = tempRaw / 340.0f + 36.53f;
  float rawPitch = atan2(-data.accelX_g, sqrt(data.accelY_g * data.accelY_g + data.accelZ_g * data.accelZ_g)) * 180.0f / PI;
  float rawRoll  = atan2(data.accelY_g, data.accelZ_g) * 180.0f / PI;
  data.pitchDeg = rawPitch - pitchOffset;
  data.rollDeg  = rawRoll - rollOffset;
  return data;
}
