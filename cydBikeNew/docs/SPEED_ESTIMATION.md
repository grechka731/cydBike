# Speed Estimation

This project estimates road speed from the MPU6050 only — there is no wheel/Hall
sensor. That puts a hard limit on accuracy, so the goal is a *useful, drift-free*
estimate rather than a precise one.

## Why a single accelerometer is not enough

At constant velocity the longitudinal acceleration is zero, so there is nothing to
integrate, and any residual bias integrates into unbounded drift. Tilt makes it
worse: a sustained acceleration is hard to distinguish from a small change in the
gravity direction. A plain "integrate the accelerometer" approach therefore reads
roughly zero at cruise.

## The fused approach

Five complementary sources each cover another's weakness:

| Method | Responsible for | Weakness (covered by others) |
|---|---|---|
| **A. Gravity-vector complementary filter** (gyro rotates the gravity estimate, accelerometer slowly corrects it) | clean longitudinal acceleration without tilt contamination | gives no speed by itself |
| **B. Longitudinal-acceleration integrator** | acceleration & braking (fast dynamics) | drifts over distance |
| **C. Vibration estimator** (6-30 Hz band-pass + RMS) | cruising speed, drift-free | needs calibration; silent on a perfectly smooth surface |
| **D. Zero-velocity update** (variance / gyro / vibration) | snapping to zero when stopped, re-zeroing bias | - |
| **E. Barometer (BMP180)** | a slope means you are really moving (suppresses false ZUPT) | slow |

B and C are fused complementarily: the integrator is the fast prediction, and the
vibration estimate is the slow correction that cancels drift.

## Key tuning trade-off

While moving, the gravity-vector correction is intentionally **slow**
(`MPU_GRAV_CORR_RATE`), otherwise the filter "eats" sustained acceleration (because
at |a| close to g, acceleration looks like tilt). At rest the correction is **fast**
(`MPU_GRAV_CORR_RATE_STILL`) so the reference re-settles quickly.

## Vibration calibration (one time, required)

Without this step the cruising speed is only in arbitrary units.

1. Flash the firmware and ride at a steady, known speed (for example 20 km/h from a
   phone GPS).
2. Read the `rms` value shown in the bottom-left corner of the display at that speed.
3. Put that averaged `rms` into `VIB_RMS_AT_REF` in `config.h`, and the speed into
   `VIB_REF_KMH`. Re-flash.

You can further tune `VIB_HP_HZ` / `VIB_LP_HZ` for your frame, wheels and tires.

## Telemetry getters

Useful if you want to surface more on screen:

- `getSpeedKmh()` - fused speed (km/h)
- `getTripDistanceKm()` - accumulated trip distance
- `getVibSpeedKmh()` - vibration-only speed
- `getIntegratorSpeedKmh()` - integrator-only speed
- `getVibRms()` - current band-passed vibration RMS
- `getSpeedConfidence()` - vibration-estimate confidence, 0..1 (drives the on-screen bar)
- `isSpeedMoving()` - moving / stopped
- `getCadenceRpm()` - experimental pedalling-cadence estimate from low-frequency frame oscillation

The vibration gain is additionally scaled at runtime by the **Speed calib** menu
setting (`speedCalibPct`, 50-200 %), so you can fine-tune calibration on the bike
without reflashing.

## Cadence (experimental)

Cadence is derived from a 0.5-3.5 Hz band-pass on the accelerometer magnitude
(`CAD_*` constants) by timing rising zero-crossings of the pedalling oscillation.
It is an approximation from the frame IMU, not a crank sensor; treat it as a rough
indicator. For accurate cadence add a magnet/reed switch on the crank.

## Want true accuracy?

Add a real wheel-speed sensor (reed switch / Hall on a spoke). GPIO3 (RX) is left
free for exactly that, and the fused estimate can serve as a backup channel.
