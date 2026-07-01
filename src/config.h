#pragma once

#define FEATURE_WIFI 1

#define FEATURE_SD   1

#define SERVO_PIN 1

// Default angle for each gear (gear1, gear2, gear3). Gear 3 is set to 200 -
// only used as the *default* the very first time the device boots (or after
// a settings reset); if you already calibrated gear 3 before, this value
// alone won't move it - go to the on-device Servo Calibration screen,
// select gear 3, and Save the new angle there so it's written to NVS.
static const int GEAR_ANGLES[3] = { 40, 145, 220 };

static const int NUM_GEARS_MAX     = 8;
static const int DEFAULT_NUM_GEARS = 3;

static const int NUM_GEARS = 3;

static const int GEAR2_DOWNSHIFT_ANGLE = 115;

static const int ANGLE_MIN = 0;
// Raised from 180 so gear angles up to ~200-210 can be tried/calibrated.
// IMPORTANT: this is just the software ceiling - it does NOT mean the servo
// can safely reach that angle. A DB961WP is a standard digital servo, not
// a wide-throw actuator: it has its own internal end-point limit, and
// commanding it past that limit makes it stall against its own stop and
// draw extra current instead of moving further (the exact kind of current
// spike that was browning out the battery before). Always find the real
// safe value using the on-device Servo Calibration screen (jog with
// +1/+10, watching/listening for straining) rather than typing a number in
// blind - and back off the moment it starts straining without moving.
static const int ANGLE_MAX = 220;

static const int SERVO_CAL_MIN_DEFAULT = 40;
static const int SERVO_CAL_MAX_DEFAULT = 200;

static const int SERVO_BACKLASH_DEG    = 6;

// --- Servo movement speed ---
// Slower steps = less current spike per pulse
static const int SERVO_STEP_DEG_UP        = 1;
static const int SERVO_STEP_DELAY_UP_MS   = 10;   // было 6 → дольше пауза между шагами
static const int SERVO_REST_EVERY_DEG     = 8;    // было 10 → чаще паузы
static const int SERVO_REST_PAUSE_MS      = 150;  // было 90 → длиннее пауза для восстановления батареи

static const int SERVO_STEP_DEG_DOWN      = 5;    // было 8 → меньше шаг вниз, меньше пик тока
static const int SERVO_STEP_DELAY_DOWN_MS = 8;    // было 1 → пауза вниз тоже нужна

// --- Servo power saving / battery-rest options ---
static const bool          SERVO_RELEASE_WHEN_IDLE  = true;
// Дольше держим перед отпусканием — серва успевает стабилизироваться
static const unsigned long SERVO_HOLD_AFTER_MOVE_MS = 800;   // было 500
// Дольше ждём при старте — дисплей и ESP32 успевают стабилизироваться
static const unsigned long SERVO_BOOT_SETTLE_MS     = 1200;  // было 600
static const bool          SERVO_REST_ON_DOWNSHIFT  = true;

// --- Soft-start: сколько шагов разгоняться плавно в начале движения ---
// Первые N шагов делаем паузу SERVO_SOFTSTART_EXTRA_MS поверх обычной,
// чтобы не давать резкий пик тока при старте с нуля.
static const int           SERVO_SOFTSTART_STEPS     = 5;    // первые 5 шагов — плавный старт
static const int           SERVO_SOFTSTART_EXTRA_MS  = 20;   // доп. задержка на каждый шаг

// --- Re-engage cooldown: минимальная пауза перед повторным включением ---
// Если серва только что отпустила (de-energized), подождать немного
// перед следующим движением, чтобы батарея восстановилась.
static const unsigned long SERVO_REENGAGE_COOLDOWN_MS = 120; // пауза перед повторным включением

static const int LASTKNOWN_SAVE_EVERY_DEG = 10;

static const float AUTOSHIFT_MIN_KMH  = 8.0f;
static const float AUTOSHIFT_MAX_KMH  = 34.0f;
static const float AUTOSHIFT_HYST_KMH = 2.5f;
static const unsigned long AUTOSHIFT_MIN_INTERVAL_MS = 1200;

#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33

static const int  TOUCH_RAW_X_MIN = 250;
static const int  TOUCH_RAW_X_MAX = 3850;
static const int  TOUCH_RAW_Y_MIN = 250;
static const int  TOUCH_RAW_Y_MAX = 3850;

#define JOY_VRX_PIN 35
#define JOY_VRY_PIN 27

static const int JOY_CENTER   = 1830;
static const int JOY_DEADZONE = 400;

static const int JOY_RIGHT_THRESHOLD = JOY_CENTER + JOY_DEADZONE;
static const int JOY_LEFT_THRESHOLD  = JOY_CENTER - JOY_DEADZONE;

static const unsigned long JOY_DEBOUNCE_MS     = 50;
static const unsigned long JOY_REPEAT_DELAY_MS = 400;

static const uint8_t       MPU_DLPF_CFG         = 0x03;
static const uint8_t       MPU_SMPLRT_DIV       = 0x04;
static const unsigned long MPU_STEP_INTERVAL_US = 5000UL;
static const float         MPU_DT_MIN           = 0.002f;
static const float         MPU_DT_MAX           = 0.05f;

static const int           MPU_CALIB_SAMPLES    = 400;

static const float         MPU_LPF_ALPHA        = 0.18f;
static const float         MPU_DEADZONE_MS2     = 0.05f;
static const float         MPU_MAX_SPEED_MS     = 33.0f;
static const float         MPU_BRAKE_MS2        = -0.20f;

static const float         MPU_GRAV_CORR_RATE       = 0.30f;
static const float         MPU_GRAV_CORR_RATE_STILL = 5.00f;
static const float         MPU_GRAV_ACC_TOL     = 1.2f;
static const float         MPU_GRAV_RENORM      = 0.01f;

static const float         MPU_ZUPT_VAR_MAX     = 0.07f;
static const float         MPU_ZUPT_GYRO_MAX    = 0.04f;
static const float         MPU_VAR_BETA         = 0.20f;
static const unsigned long MPU_ZUPT_HOLD_MS     = 250;
static const float         MPU_GYRO_BIAS_BETA   = 0.05f;

static const bool          VIB_ENABLE           = true;
static const float         VIB_HP_HZ            = 6.0f;
static const float         VIB_LP_HZ            = 30.0f;
static const float         VIB_RMS_TAU          = 0.60f;
static const float         VIB_FLOOR_TAU        = 1.0f;
static const float         VIB_REF_KMH          = 10.0f;
static const float         VIB_RMS_AT_REF       = 0.50f;
static const float         VIB_MIN_RMS          = 0.08f;
static const float         VIB_MAX_KMH          = 60.0f;

static const float         FUSE_VIB_PULL        = 0.6f;
static const float         FUSE_VIB_PULL_LOW    = 0.15f;
static const float         FUSE_DECAY_NO_VIB    = 0.40f;

static const float         MPU_BARO_MOVE_MS     = 0.12f;
static const float         MPU_BARO_ALT_ALPHA   = 0.15f;
static const unsigned long MPU_BARO_INTERVAL_MS = 100;

static const float         CAD_HP_HZ            = 0.5f;
static const float         CAD_LP_HZ            = 3.5f;
static const float         CAD_MIN_AMP_MS2      = 0.15f;
static const float         CAD_RMS_TAU          = 0.8f;
static const float         CAD_TIMEOUT_MS       = 2500;

static const float         SEA_LEVEL_HPA        = 1013.25f;
static const uint32_t      I2C_CLOCK_HZ         = 100000UL;

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

#define SHOW_I2C_DIAGNOSTIC 1

static const char* NVS_NAMESPACE   = "servocfg";
static const char* NVS_KEY_TARGET  = "target";
static const char* NVS_KEY_LASTKNOWN = "lastknown";

static const char* NVS_APP_NAMESPACE = "appcfg";

#define BACKLIGHT_PIN 21

static const int TOUCH_SENS_MIN     = 20;
static const int TOUCH_SENS_MAX     = 100;
static const int TOUCH_SENS_STEP    = 10;
static const int TOUCH_SENS_DEFAULT = 60;

static const int TOUCH_Z_AT_MIN_SENS = 1500;
static const int TOUCH_Z_AT_MAX_SENS = 420;

static const int BRIGHTNESS_MIN     = 20;
static const int BRIGHTNESS_MAX     = 100;
static const int BRIGHTNESS_STEP    = 10;
static const int BRIGHTNESS_DEFAULT = 100;

static const float KMH_TO_MPH = 0.621371f;

static const float SPEED_WARN_DEFAULT_KMH = 0.0f;
static const float SPEED_WARN_MAX_KMH     = 80.0f;
static const float SPEED_WARN_STEP_KMH    = 2.0f;

static const int   SPEED_CALIB_MIN_PCT     = 50;
static const int   SPEED_CALIB_MAX_PCT     = 200;
static const int   SPEED_CALIB_STEP_PCT    = 5;
static const int   SPEED_CALIB_DEFAULT_PCT = 100;

static const int   SPEED_HIST_LEN          = 120;
static const unsigned long SPEED_HIST_INTERVAL_MS = 250;

static const float MOVING_KMH = 1.0f;

#define WIFI_AP_SSID "CYD-Bike"
#define WIFI_AP_PASS "ride1234"
#define WIFI_HTTP_PORT 80

#define WIFI_STA_SSID ""
#define WIFI_STA_PASS ""
#define FEATURE_OTA 1
#define NTP_SERVER "pool.ntp.org"

#define SD_CS_PIN   5
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23
static const unsigned long SD_LOG_INTERVAL_MS = 1000;

#define SD_FILE_PREFIX "RIDE"

static const unsigned long SLEEP_TIMEOUT_MS = 45000;
static const float         SLEEP_WAKE_KMH   = 2.0f;

static const int           ALT_HIST_LEN          = 120;
static const unsigned long ALT_HIST_INTERVAL_MS  = 2000;

static const float         BARO_TREND_TAU_S      = 600.0f;

static const float         BARO_TREND_THRESH_HPA = 0.4f;

static const int           CLOCK_TZ_DEFAULT_MIN  = 180;

static const float RIDER_MASS_DEFAULT_KG = 80.0f;
static const float RIDER_MASS_MIN_KG     = 30.0f;
static const float RIDER_MASS_MAX_KG     = 200.0f;
static const float RIDER_MASS_STEP_KG    = 5.0f;

static const float POWER_CRR             = 0.006f;
static const float POWER_CDA             = 0.45f;
static const float POWER_RHO             = 1.225f;
static const float POWER_DRIVETRAIN_EFF  = 0.97f;
static const float HUMAN_EFFICIENCY      = 0.24f;

static const int   CHAIN_LUBE_KM_DEFAULT = 200;
static const int   SERVICE_KM_DEFAULT    = 2000;
static const int   MAINT_KM_MIN          = 0;
static const int   MAINT_KM_MAX          = 20000;
static const int   MAINT_KM_STEP         = 50;

static const char* MAINT_LOG_FILE        = "/MAINT.LOG";