#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "config.h"
#include "debug.h"
#include "mpu_speed.h"
#include "bmp_sensor.h"
#include "app_settings.h"

static const uint8_t MPU_ADDR_PRIMARY   = 0x68;
static const uint8_t MPU_ADDR_SECONDARY = 0x69;
static const uint8_t REG_PWR_MGMT_1     = 0x6B;
static const uint8_t REG_SMPLRT_DIV     = 0x19;
static const uint8_t REG_CONFIG         = 0x1A;
static const uint8_t REG_GYRO_CONFIG    = 0x1B;
static const uint8_t REG_ACCEL_CONFIG   = 0x1C;
static const uint8_t REG_ACCEL_XOUT_H   = 0x3B;

static const float ACCEL_LSB_PER_G  = 16384.0f;
static const float GYRO_LSB_PER_DPS = 131.0f;
static const float G_MS2            = 9.81f;

static uint8_t mpuAddr  = 0;
static bool    mpuFound = false;

static float gxOff = 0, gyOff = 0, gzOff = 0;

static float gravAx = 0, gravAy = 0, gravAz = G_MS2;

static float axF = 0;

static float accMean = G_MS2;
static float accVar  = 0.0f;

static float vibLpLow = G_MS2;
static float vibBp    = 0.0f;
static float vibMsq   = 0.0f;
static float vibFloor = 0.0f;
static float vibRms   = 0.0f;
static float vibSpeedKmh = 0.0f;
static float vibConf  = 0.0f;
static float tauVibLP = 0.0f, tauVibHP = 0.0f;

static float cadLpLow = G_MS2;
static float cadBp    = 0.0f;
static float cadMsq   = 0.0f;
static float tauCadLP = 0.0f, tauCadHP = 0.0f;
static int   cadPrevSign = 0;
static unsigned long cadLastCrossMs = 0;
static unsigned long cadPrevCrossMs = 0;
static float cadRpm   = 0.0f;

static bool          baroInit   = false;
static float         altFilt    = 0.0f;
static float         vertVel    = 0.0f;
static unsigned long lastBaroMs = 0;

static float velocity = 0.0f;
static float velInteg = 0.0f;
static float speedKmh = 0.0f;
static float tripKm   = 0.0f;
static float prevVel  = 0.0f;
static bool  moving   = false;

static unsigned long lastStepMicros = 0;
static unsigned long restStartMs    = 0;
static bool          resting        = false;
static unsigned long lastDebugMs    = 0;

static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline float lpAlpha(float tau, float dt) {
  if (tau <= 0.0f) return 1.0f;
  return dt / (tau + dt);
}

static bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

static bool probeAddress(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

struct Sample {
  bool valid;
  float ax, ay, az;
  float gx, gy, gz;
};

static Sample readSample() {
  Sample s;
  s.valid = false;
  s.ax = s.ay = s.az = 0.0f;
  s.gx = s.gy = s.gz = 0.0f;
  if (!mpuFound) return s;

  Wire.beginTransmission(mpuAddr);
  Wire.write(REG_ACCEL_XOUT_H);
  if (Wire.endTransmission(false) != 0) return s;
  uint8_t received = Wire.requestFrom(mpuAddr, (uint8_t)14);
  if (received != 14) return s;

  uint8_t raw[14];
  for (uint8_t i = 0; i < 14; i++) raw[i] = Wire.read();

  int16_t axr = (int16_t)((raw[0]  << 8) | raw[1]);
  int16_t ayr = (int16_t)((raw[2]  << 8) | raw[3]);
  int16_t azr = (int16_t)((raw[4]  << 8) | raw[5]);
  int16_t gxr = (int16_t)((raw[8]  << 8) | raw[9]);
  int16_t gyr = (int16_t)((raw[10] << 8) | raw[11]);
  int16_t gzr = (int16_t)((raw[12] << 8) | raw[13]);

  s.ax = (axr / ACCEL_LSB_PER_G) * G_MS2;
  s.ay = (ayr / ACCEL_LSB_PER_G) * G_MS2;
  s.az = (azr / ACCEL_LSB_PER_G) * G_MS2;
  s.gx = (gxr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  s.gy = (gyr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  s.gz = (gzr / GYRO_LSB_PER_DPS) * DEG_TO_RAD;
  s.valid = true;
  return s;
}

static void updateBaro() {
  unsigned long now = millis();
  if (now - lastBaroMs < MPU_BARO_INTERVAL_MS) return;
  float dtBaro = (now - lastBaroMs) / 1000.0f;
  lastBaroMs = now;

  BmpData bmp = readBmpData();
  if (!bmp.valid) return;

  if (!baroInit) {
    altFilt = bmp.altitudeM;
    baroInit = true;
    vertVel = 0.0f;
    return;
  }

  float newAlt = (1.0f - MPU_BARO_ALT_ALPHA) * altFilt + MPU_BARO_ALT_ALPHA * bmp.altitudeM;
  if (dtBaro > 0.0f) {
    vertVel = (newAlt - altFilt) / dtBaro;
  }
  altFilt = newAlt;
}

static void calibrate() {
  DBG_PRINTLN("[speed] Calibration: keep the bike still for ~4 seconds...");

  float sax = 0, say = 0, saz = 0;
  float sgx = 0, sgy = 0, sgz = 0;
  int n = 0;

  for (int i = 0; i < MPU_CALIB_SAMPLES; i++) {
    Sample s = readSample();
    if (s.valid) {
      sax += s.ax; say += s.ay; saz += s.az;
      sgx += s.gx; sgy += s.gy; sgz += s.gz;
      n++;
    }
    delay(10);
  }

  if (n > 0) {
    gravAx = sax / n;
    gravAy = say / n;
    gravAz = saz / n;
    gxOff = sgx / n;
    gyOff = sgy / n;
    gzOff = sgz / n;
    accMean = sqrtf(gravAx * gravAx + gravAy * gravAy + gravAz * gravAz);
    accVar  = 0.0f;
    DBG_PRINTF("[speed] Calibration done (samples: %d)\n", n);
    DBG_PRINTF("[speed] g vector: %.3f / %.3f / %.3f m/s2\n", gravAx, gravAy, gravAz);
  } else {
    DBG_PRINTLN("[speed] Calibration ERROR: no sensor data.");
  }

  axF = 0.0f;
  velocity = 0.0f;
  velInteg = 0.0f;
  speedKmh = 0.0f;
  prevVel = 0.0f;
  resting = false;
  moving = false;

  vibLpLow = accMean;
  vibBp = 0.0f;
  vibMsq = 0.0f;
  vibFloor = 0.0f;
  vibRms = 0.0f;
  vibSpeedKmh = 0.0f;
  vibConf = 0.0f;
}

void setupMpuSpeed() {
  if (probeAddress(MPU_ADDR_PRIMARY)) {
    mpuAddr = MPU_ADDR_PRIMARY;
  } else if (probeAddress(MPU_ADDR_SECONDARY)) {
    mpuAddr = MPU_ADDR_SECONDARY;
  } else {
    mpuFound = false;
    DBG_PRINTLN("[speed] MPU6050 not responding (0x68/0x69) - speed unavailable.");
    return;
  }

  bool ok = true;
  ok &= writeRegister(REG_PWR_MGMT_1,   0x00);
  delay(50);
  ok &= writeRegister(REG_CONFIG,       MPU_DLPF_CFG);
  ok &= writeRegister(REG_ACCEL_CONFIG, 0x00);
  ok &= writeRegister(REG_GYRO_CONFIG,  0x00);
  ok &= writeRegister(REG_SMPLRT_DIV,   MPU_SMPLRT_DIV);

  mpuFound = ok;
  if (!mpuFound) {
    DBG_PRINTLN("[speed] Failed to write MPU6050 init registers.");
    return;
  }

  tauVibLP = 1.0f / (2.0f * PI * VIB_LP_HZ);
  tauVibHP = 1.0f / (2.0f * PI * VIB_HP_HZ);
  tauCadLP = 1.0f / (2.0f * PI * CAD_LP_HZ);
  tauCadHP = 1.0f / (2.0f * PI * CAD_HP_HZ);

  DBG_PRINTF("[speed] MPU6050 initialized at 0x%02X (200 Hz, DLPF cfg=0x%02X)\n",
                mpuAddr, MPU_DLPF_CFG);
  delay(100);
  calibrate();
  lastStepMicros = micros();
  lastBaroMs = millis();
}

void updateMpuSpeed() {
  if (!mpuFound) return;

  unsigned long nowUs = micros();
  unsigned long elapsedUs = (unsigned long)(nowUs - lastStepMicros);
  if (elapsedUs < MPU_STEP_INTERVAL_US) return;
  lastStepMicros = nowUs;

  float dt = clampf(elapsedUs / 1000000.0f, MPU_DT_MIN, MPU_DT_MAX);

  Sample s = readSample();
  if (!s.valid) return;

  updateBaro();

  float gx = s.gx - gxOff;
  float gy = s.gy - gyOff;
  float gz = s.gz - gzOff;
  float gyroMag = sqrtf(gx * gx + gy * gy + gz * gz);

  float accMag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
  accMean = (1.0f - MPU_VAR_BETA) * accMean + MPU_VAR_BETA * accMag;
  float dMag = accMag - accMean;
  accVar = (1.0f - MPU_VAR_BETA) * accVar + MPU_VAR_BETA * (dMag * dMag);

  if (VIB_ENABLE) {
    float aHP = lpAlpha(tauVibHP, dt);
    float aLP = lpAlpha(tauVibLP, dt);
    vibLpLow += aHP * (accMag - vibLpLow);
    float hp = accMag - vibLpLow;
    vibBp += aLP * (hp - vibBp);
    float aR = lpAlpha(VIB_RMS_TAU, dt);
    vibMsq += aR * (vibBp * vibBp - vibMsq);
    vibRms = sqrtf(vibMsq);

    float vibSignal = vibRms - vibFloor;
    if (vibSignal < 0.0f) vibSignal = 0.0f;

    float vibMin = settings().tuneVibMinRms;
    float vibRef = settings().tuneVibRmsAtRef;
    if (vibSignal < vibMin) {
      vibSpeedKmh = 0.0f;
      vibConf = 0.0f;
    } else {
      float calibGain = settings().speedCalibPct / 100.0f;
      float kVib = (vibRef > 1e-4f) ? (settings().tuneVibRefKmh / vibRef) : 0.0f;
      vibSpeedKmh = clampf(kVib * vibSignal * calibGain, 0.0f, VIB_MAX_KMH);
      vibConf = clampf((vibSignal - vibMin) / (vibRef + 1e-4f), 0.0f, 1.0f);
    }
  }
  bool vibMoving = VIB_ENABLE && ((vibRms - vibFloor) > settings().tuneVibMinRms * 1.5f);

  {
    float aHP = lpAlpha(tauCadHP, dt);
    float aLP = lpAlpha(tauCadLP, dt);
    cadLpLow += aHP * (accMag - cadLpLow);
    float chp = accMag - cadLpLow;
    cadBp += aLP * (chp - cadBp);
    float aR = lpAlpha(CAD_RMS_TAU, dt);
    cadMsq += aR * (cadBp * cadBp - cadMsq);
    float cadAmp = sqrtf(cadMsq);

    unsigned long nowMs = millis();
    if (cadAmp > CAD_MIN_AMP_MS2) {
      int sign = (cadBp > 0.0f) ? 1 : ((cadBp < 0.0f) ? -1 : cadPrevSign);
      if (sign != 0 && cadPrevSign != 0 && sign != cadPrevSign && sign > 0) {

        if (cadPrevCrossMs != 0) {
          unsigned long period = nowMs - cadPrevCrossMs;
          if (period > 200 && period < 4000) {
            float rpm = 60000.0f / (float)period;
            cadRpm = cadRpm * 0.6f + rpm * 0.4f;
          }
        }
        cadPrevCrossMs = nowMs;
        cadLastCrossMs = nowMs;
      }
      if (sign != 0) cadPrevSign = sign;
    }
    if (nowMs - cadLastCrossMs > (unsigned long)CAD_TIMEOUT_MS) {
      cadRpm = 0.0f;
    }
  }

  bool baroMoving = baroInit && (fabsf(vertVel) > MPU_BARO_MOVE_MS);
  bool still = (accVar < MPU_ZUPT_VAR_MAX) &&
               (gyroMag < MPU_ZUPT_GYRO_MAX) &&
               !baroMoving &&
               !vibMoving;

  float cx = gy * gravAz - gz * gravAy;
  float cy = gz * gravAx - gx * gravAz;
  float cz = gx * gravAy - gy * gravAx;
  gravAx -= cx * dt;
  gravAy -= cy * dt;
  gravAz -= cz * dt;

  if (fabsf(accMag - G_MS2) < MPU_GRAV_ACC_TOL) {
    float rate = still ? MPU_GRAV_CORR_RATE_STILL : MPU_GRAV_CORR_RATE;
    float kg = clampf(rate * dt, 0.0f, 1.0f);
    gravAx += kg * (s.ax - gravAx);
    gravAy += kg * (s.ay - gravAy);
    gravAz += kg * (s.az - gravAz);
  }

  float gm = sqrtf(gravAx * gravAx + gravAy * gravAy + gravAz * gravAz);
  if (gm > 1e-3f) {
    float f = G_MS2 / gm;
    float blend = 1.0f + MPU_GRAV_RENORM * (f - 1.0f);
    gravAx *= blend;
    gravAy *= blend;
    gravAz *= blend;
  }

  float linAx = s.ax - gravAx;

  axF = MPU_LPF_ALPHA * linAx + (1.0f - MPU_LPF_ALPHA) * axF;
  float axUse = (fabsf(axF) < MPU_DEADZONE_MS2) ? 0.0f : axF;

  bool zuptActive = false;
  if (still) {
    if (!resting) { resting = true; restStartMs = millis(); }
    if (millis() - restStartMs >= MPU_ZUPT_HOLD_MS) {
      zuptActive = true;

      gxOff += MPU_GYRO_BIAS_BETA * (s.gx - gxOff);
      gyOff += MPU_GYRO_BIAS_BETA * (s.gy - gyOff);
      gzOff += MPU_GYRO_BIAS_BETA * (s.gz - gzOff);

      if (VIB_ENABLE) {
        float aF = lpAlpha(VIB_FLOOR_TAU, dt);
        vibFloor += aF * (vibRms - vibFloor);
      }
    }
  } else {
    resting = false;
  }

  if (zuptActive) {
    velInteg = 0.0f;
  } else {
    velInteg += axUse * dt;
    if (velInteg < 0.0f) velInteg = 0.0f;
    if (velInteg > MPU_MAX_SPEED_MS) velInteg = 0.0f;
  }

  prevVel = velocity;
  if (zuptActive) {
    velocity = 0.0f;
  } else {

    velocity += axUse * dt;
    if (velocity < 0.0f) velocity = 0.0f;

    if (VIB_ENABLE && vibSpeedKmh > 0.0f) {
      float vibMs = vibSpeedKmh / 3.6f;
      float pull = FUSE_VIB_PULL_LOW + (settings().tuneFuseVibPull - FUSE_VIB_PULL_LOW) * vibConf;
      float k = clampf(pull * dt, 0.0f, 1.0f);
      velocity += (vibMs - velocity) * k;
    } else {

      float k = clampf(FUSE_DECAY_NO_VIB * dt, 0.0f, 1.0f);
      velocity -= velocity * k;
    }

    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > MPU_MAX_SPEED_MS) velocity = 0.0f;
  }

  moving = (velocity > 0.3f);
  speedKmh = velocity * 3.6f;

  tripKm += 0.5f * (prevVel + velocity) * dt / 1000.0f;

  if (millis() - lastDebugMs >= 200) {
    lastDebugMs = millis();
    const char* state = "MOVING";
    if (zuptActive)               state = "ZUPT";
    else if (axF < MPU_BRAKE_MS2) state = "BRAKE";
    DBG_PRINTF("[%lu] Speed: %.2f km/h | integ: %.2f | vib: %.2f (rms %.3f, floor %.3f, conf %.2f) | ax: %.3f | vVel: %.2f | %s\n",
                  millis(), speedKmh, velInteg * 3.6f, vibSpeedKmh,
                  vibRms, vibFloor, vibConf, axF, vertVel, state);
  }
}

float getSpeedKmh()           { return speedKmh; }
float getTripDistanceKm()     { return tripKm; }
float getVibSpeedKmh()        { return vibSpeedKmh; }
float getVibRms()             { return vibRms; }
float getIntegratorSpeedKmh() { return velInteg * 3.6f; }
float getSpeedConfidence()    { return vibConf; }
bool  isSpeedMoving()         { return moving; }
float getCadenceRpm()         { return cadRpm; }
