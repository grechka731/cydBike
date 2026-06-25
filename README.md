<img width="4104" height="3432" alt="bike" src="https://github.com/user-attachments/assets/eb3d4dfb-b644-465a-85d6-93dc74cc98ec" />
# CYD Bike Computer

A small bike computer + electronic gear shifter that runs on the "Cheap Yellow
Display" (CYD, `ESP32-2432S028R`) - the 2.8" ESP32 board with an ILI9341 screen
and resistive touch.

No wheel magnet, no GPS. Speed is estimated from the onboard MPU6050 (motion +
frame vibration), gears are moved by a single servo, and altitude/pressure come
from a BMP180. Everything runs off one battery shared between the CYD and the
servo, so a few things are tuned around that constraint (see notes below).

For a full step-by-step usage guide (Russian first, then English), see
[`docs/USAGE.md`](docs/USAGE.md).

## What it does

- **Gear shifting** with a servo. Manual control with the joystick (right = up,
  left = down, hold to repeat) or fully automatic by speed. Per-gear angles and
  the servo end-stops are calibrated from the on-screen menu and saved to NVS.
- **Sensorless speed** from the MPU6050: gravity tracking, longitudinal
  acceleration, a vibration model and zero-velocity updates are fused together.
  The vibration part is calibrated to real measured data.
- **Ride stats**: trip distance, persistent odometer, max / average speed, ride
  timer, current grade (%) from pitch, and an experimental cadence estimate.
- **Environment**: temperature, pressure and altitude from the BMP180, with
  min/max tracking, sea-level calibration and a barometric weather trend
  (rising / falling / steady).
- **Screens** you swipe through: main speed, telemetry grid, speed graph and an
  altitude graph with elevation gain. There is also a settings/menu screen and a
  servo calibration screen.
- **Wake on motion**: after a while parked the screen blanks to a dim clock and
  comes back the moment you start moving or tap it.
- **Clock** without an RTC module - the time is set from your phone's browser
  when you open the dashboard, then kept by the ESP32.
- **Records & maintenance**: all-time top speed, longest ride and biggest climb
  are kept in NVS; chain-lube and service reminders count down by distance and
  warn on screen and on the dashboard.
- **Power & calories**: a rough power estimate from rider mass, grade, speed and
  acceleration, integrated into an energy/calorie figure for the ride.
- **Ride summary**: an "itogi" screen with distance, time, average/max speed,
  elevation gain and average cadence.
- **WiFi dashboard**: the board hosts its own access point with a web page
  showing live telemetry, speed/altitude charts and all the settings. You can
  also tune speed/vibration thresholds live, browse saved rides, download a CSV
  straight to your phone and plot a ride's speed/altitude without pulling the
  card. Ride files can also be opened and charted directly on the CYD.
- **SD logging** (optional): every power-up starts a new CSV file with an
  auto-incremented number (`RIDE8324.CSV` -> `RIDE8325.CSV`) and logs all sensor
  values once a second.
- **Brightness** is done by scaling pixel colors, not PWM (see the GPIO21 note).
- Optional overspeed warning, km/h or mph, and color invert - all saved to NVS.

## Hardware

| Part | Notes |
|---|---|
| ESP32-2432S028R (CYD) | 2.8" ILI9341 240x320 + XPT2046 touch |
| MPU6050 | accel + gyro, I2C `0x68` |
| BMP180 | pressure / temperature, I2C `0x77` |
| Servo (SG90 / MG90 etc.) | moves the derailleur |
| Analog joystick | only the X axis is used |
| microSD card | optional, for ride logs |

## Wiring

The screen and touch controller are already wired on the CYD board. These are the
free GPIOs on the side connectors used for everything else:

| Peripheral | Signal | GPIO |
|---|---|---|
| I2C (MPU6050 + BMP180) | SDA | 21 |
| I2C (MPU6050 + BMP180) | SCL | 22 |
| Servo | PWM | 1 |
| Joystick | X (shift) | 35 |
| Joystick | Y (reserved) | 27 |
| microSD | CS / SCK / MISO / MOSI | 5 / 18 / 19 / 23 |

Two things to keep in mind on the CYD:

- **GPIO1** is the UART0 TX pin. It drives the servo here, so the firmware never
  calls `Serial.begin()` (that would fight the servo). Serial debug is off by
  default in `debug.h`.
- **GPIO21** is shared with the LCD backlight enable, so the backlight can't be
  PWM-dimmed or switched off. Brightness is faked by darkening the drawn colors,
  and "sleep" just blanks the screen rather than killing the backlight.

Power the servo from something that can take its stall current.

## Build & flash

```bash
pio run                  # build
pio run --target upload  # flash over USB (first time has to be USB)
```

All TFT_eSPI options are set through `build_flags`, so there's no `User_Setup.h`
to edit.

## WiFi / dashboard

The board starts an access point:

- SSID: `CYD-Bike`
- Password: `ride1234`
- Open `http://192.168.4.1`

Change those in `config.h` (`WIFI_AP_SSID` / `WIFI_AP_PASS`). Set `FEATURE_WIFI`
or `FEATURE_SD` to `0` there if you don't want those subsystems.

### Joining your home network (optional)

Fill in `WIFI_STA_SSID` / `WIFI_STA_PASS` in `config.h` and the board also joins
that network while keeping its own access point. Once connected it pulls the
real time over NTP (`NTP_SERVER`), so the clock no longer depends on opening the
dashboard. Leave the STA fields empty to stay access-point only.

### Over-the-air updates

With `FEATURE_OTA` enabled, open `/update` from the dashboard, pick a compiled
`firmware.bin` and upload it; the board flashes and reboots. The very first
flash still has to be over USB.

## Speed estimation

A plain accelerometer can't recover steady cruising speed (constant velocity =
zero acceleration), so the vibration estimator does the heavy lifting at speed.
It needs a one-time calibration. The how and why is written up in
[`docs/SPEED_ESTIMATION.md`](docs/SPEED_ESTIMATION.md).

## Layout

```
src/
  main.cpp          setup / loop
  config.h          pins, thresholds, tuning
  debug.h           serial logging switch (off)
  app_settings.*    persisted user settings (NVS)
  app_clock.*       time keeping without an RTC
  display_ui.*      screens, touch, swipe, sleep
  gear_control.*    servo shifting + calibration
  joystick.*        joystick reading
  i2c_bus.*         shared I2C init
  bmp_sensor.*      BMP180 driver
  mpu_sensor.*      MPU6050 driver + boot calibration
  mpu_speed.*       fused speed / cadence estimator
  ride_stats.*      trip, odometer, grade, history, trend
  net_telemetry.*   WiFi AP, web page, ride export
  sd_logger.*       CSV ride logging
  github_logo.h     boot splash
docs/
  SPEED_ESTIMATION.md
```

## License

MIT, see [`LICENSE`](LICENSE).
