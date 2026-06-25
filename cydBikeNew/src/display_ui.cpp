#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"
#include "display_ui.h"
#include "github_logo.h"
#include "app_settings.h"
#include "ride_stats.h"
#include "gear_control.h"
#include "mpu_speed.h"
#include "bmp_sensor.h"
#include "mpu_sensor.h"
#include "i2c_bus.h"
#include "net_telemetry.h"
#include "app_clock.h"
#include "sd_logger.h"
#include <string.h>

static TFT_eSPI tft = TFT_eSPI();
static SPIClass touchSPI = SPIClass(VSPI);
static XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);

enum UiScreen {
  SCREEN_MAIN = 0,
  SCREEN_TELEMETRY,
  SCREEN_GRAPH,
  SCREEN_ALTGRAPH,
  SCREEN_MENU,
  SCREEN_SERVOCAL,
  SCREEN_SUMMARY,
  SCREEN_RECORDS,
  SCREEN_HISTORY,
  SCREEN_FILEVIEW
};
static UiScreen uiScreen = SCREEN_MAIN;
static const int NUM_VIEW_SCREENS = 4;

static int  menuPage = 0;
static bool blinkPhase = false;

static unsigned long lastActivityMs = 0;
static bool          screenAsleep = false;
static char          sleepClockStr[12] = "";

static int calAngle = 0, calGear = 1;

static uint16_t COL_BG, COL_FG, COL_MID, COL_WARN, COL_OK, COL_ACC;

static uint16_t dim565(uint16_t c, int pct) {
  if (pct >= 100) return c;
  int r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  r = r * pct / 100; g = g * pct / 100; b = b * pct / 100;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void applyUiColors() {
  int br = settings().brightness;
  uint16_t bg, fg, mid;
  if (!settings().inverted) {
    bg = TFT_BLACK; fg = TFT_WHITE; mid = TFT_DARKGREY;
  } else {
    bg = TFT_WHITE; fg = TFT_BLACK; mid = TFT_LIGHTGREY;
  }

  COL_BG   = dim565(bg, br);
  COL_FG   = dim565(fg, br);
  COL_MID  = dim565(mid, br);
  COL_WARN = dim565(TFT_RED, br);
  COL_OK   = dim565(TFT_GREEN, br);
  COL_ACC  = dim565(TFT_CYAN, br);
}

static int touchZThreshold = TOUCH_Z_AT_MAX_SENS;
static void applyTouchSensitivity() {
  touchZThreshold = map(settings().touchSens, TOUCH_SENS_MIN, TOUCH_SENS_MAX,
                        TOUCH_Z_AT_MIN_SENS, TOUCH_Z_AT_MAX_SENS);
}

static const int MARGIN = 12;
static const int TOPBAR_H = 22;
static const int SWIPE_MIN = 55;

static int W() { return tft.width(); }
static int H() { return tft.height(); }

static bool inRect(int sx, int sy, int x, int y, int w, int h) {
  return (sx >= x && sx <= x + w && sy >= y && sy <= y + h);
}

static const int TB_W = 54;
static int tbPageX() { return 4; }
static int tbSetX()  { return W() - 4 - TB_W - 4 - TB_W; }
static int tbInvX()  { return W() - 4 - TB_W; }

static void drawButton(int x, int y, int w, int h, const char* label,
                       bool filled, int textSize) {
  if (filled) {
    tft.fillRoundRect(x, y, w, h, 3, COL_FG);
    tft.setTextColor(COL_BG, COL_FG);
  } else {
    tft.fillRoundRect(x, y, w, h, 3, COL_BG);
    tft.drawRoundRect(x, y, w, h, 3, COL_FG);
    tft.setTextColor(COL_FG, COL_BG);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(textSize);
  tft.drawString(label, x + w / 2, y + h / 2);
}

static const char* viewName() {
  switch (uiScreen) {
    case SCREEN_MAIN:      return "SPEED";
    case SCREEN_TELEMETRY: return "TELEMETRY";
    case SCREEN_GRAPH:     return "GRAPH";
    case SCREEN_ALTGRAPH:  return "ALTITUDE";
    default:               return "";
  }
}

static void drawTopBar() {
  tft.fillRect(0, 0, W(), TOPBAR_H, COL_BG);
  drawButton(tbPageX(), 2, TB_W, TOPBAR_H - 4, "PAGE", false, 1);
  drawButton(tbSetX(),  2, TB_W, TOPBAR_H - 4, "MENU", false, 1);
  drawButton(tbInvX(),  2, TB_W, TOPBAR_H - 4, "INV",  false, 1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_MID, COL_BG);
  tft.setTextSize(1);
  tft.drawString(viewName(), W() / 2, TOPBAR_H / 2);
}

void setupDisplay() {
  tft.init();
  tft.setRotation(0);

  applyUiColors();
  applyTouchSensitivity();
  tft.fillScreen(COL_BG);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touch.begin(touchSPI);
  touch.setRotation(0);

  lastActivityMs = millis();
}

static const unsigned long SPLASH_DURATION_MS = 3000;
void drawCalibrationSplash() {
  tft.fillScreen(COL_BG);
  int logoX = (W() - GITHUB_LOGO_W) / 2;
  int logoY = (H() / 2) - GITHUB_LOGO_H;
  tft.drawBitmap(logoX, logoY, GITHUB_LOGO, GITHUB_LOGO_W, GITHUB_LOGO_H, COL_FG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_OK, COL_BG);
  tft.setTextSize(2);
  tft.drawString("github.com/", W() / 2, (H() / 2) + 14);
  tft.drawString("grechka731", W() / 2, (H() / 2) + 38);
  delay(SPLASH_DURATION_MS);
  tft.fillScreen(COL_BG);
}

void drawBootDiagnostics() {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("I2C DIAGNOSTIC", MARGIN, 12);

  int y = 44;
  tft.setTextSize(1);
  uint8_t addrs[16];
  int n = scanI2C(addrs, 16);
  char line[40];

  if (n == 0) {
    tft.setTextColor(COL_WARN, COL_BG);
    tft.drawString("No I2C devices found", MARGIN, y); y += 16;
    tft.setTextColor(COL_FG, COL_BG);
    tft.drawString("Check sensor VCC/GND/SDA/SCL.", MARGIN, y); y += 14;
    tft.drawString("SDA=GPIO21 also drives the", MARGIN, y); y += 14;
    tft.drawString("LCD backlight on CYD - this", MARGIN, y); y += 14;
    tft.drawString("can block the I2C bus.", MARGIN, y); y += 20;
  } else {
    tft.setTextColor(COL_FG, COL_BG);
    snprintf(line, sizeof(line), "Found %d device(s):", n);
    tft.drawString(line, MARGIN, y); y += 16;
    for (int i = 0; i < n; i++) {
      snprintf(line, sizeof(line), "  0x%02X", addrs[i]);
      tft.drawString(line, MARGIN, y); y += 14;
    }
    y += 8;
  }

  tft.setTextSize(2);
  tft.setTextColor(isMpuFound() ? COL_OK : COL_WARN, COL_BG);
  snprintf(line, sizeof(line), "MPU6050: %s", isMpuFound() ? "OK" : "NO");
  tft.drawString(line, MARGIN, y); y += 26;
  tft.setTextColor(isBmpFound() ? COL_OK : COL_WARN, COL_BG);
  snprintf(line, sizeof(line), "BMP180: %s", isBmpFound() ? "OK" : "NO");
  tft.drawString(line, MARGIN, y); y += 26;

#if FEATURE_WIFI
  tft.setTextSize(1);
  tft.setTextColor(COL_ACC, COL_BG);
  if (netStarted()) {
    snprintf(line, sizeof(line), "WiFi AP: %s", netApSsid());
    tft.drawString(line, MARGIN, y); y += 14;
    snprintf(line, sizeof(line), "http://%s", netApIp());
    tft.drawString(line, MARGIN, y); y += 14;
  }
#endif

  delay(4000);
  tft.fillScreen(COL_BG);
}

static void drawSpeedBig(float dispSpeed, bool warn) {
  int top = TOPBAR_H + 8;
  tft.fillRect(0, top, W(), 70, COL_BG);
  tft.setTextDatum(TC_DATUM);
  uint16_t c = (warn && blinkPhase) ? COL_WARN : COL_FG;
  tft.setTextColor(c, COL_BG);
  tft.setTextSize(8);
  char buf[10];
  snprintf(buf, sizeof(buf), "%.1f", dispSpeed);
  tft.drawString(buf, W() / 2, top);

  if (warn) {
    uint16_t fc = blinkPhase ? COL_WARN : COL_BG;
    tft.drawRect(2, 2, W() - 4, H() - 4, fc);
    tft.drawRect(3, 3, W() - 6, H() - 6, fc);
  }
}

static void drawGraph(int gx, int gy, int gw, int gh, bool withLabels) {
  tft.fillRect(gx, gy, gw, gh, COL_BG);
  tft.drawRect(gx, gy, gw, gh, COL_MID);

  int count, head, cap;
  const float* hist = rideSpeedHistory(&count, &head, &cap);

  float vmax = 5.0f;
  for (int i = 0; i < count; i++) {
    float v = toDisplaySpeed(hist[i]);
    if (v > vmax) vmax = v;
  }

  vmax *= 1.15f;

  if (withLabels) {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MID, COL_BG);
    tft.setTextSize(1);
    char b[10];
    snprintf(b, sizeof(b), "%.0f", vmax);
    tft.drawString(b, gx + gw - 2, gy + 2);
    tft.drawString("0", gx + gw - 2, gy + gh - 10);
  }

  if (count < 2) return;
  int prevX = 0, prevY = 0;
  bool first = true;
  for (int i = 0; i < count; i++) {
    int idx = (head - count + i + cap) % cap;
    float v = toDisplaySpeed(hist[idx]);
    int px = gx + 1 + (int)((long)(gw - 2) * i / (count - 1));
    int py = gy + gh - 2 - (int)((float)(gh - 4) * (v / vmax));
    if (py < gy + 1) py = gy + 1;
    if (!first) tft.drawLine(prevX, prevY, px, py, COL_OK);
    prevX = px; prevY = py; first = false;
  }
}

static const int MAIN_UNIT_Y = TOPBAR_H + 80;
static const int MAIN_BAR_Y  = TOPBAR_H + 98;
static const int MAIN_ROW_Y0 = TOPBAR_H + 112;
static const int MAIN_ROW_H  = 26;
static const int MAIN_GEAR_Y = TOPBAR_H + 196;
static const int MAIN_GRAPH_Y = TOPBAR_H + 222;
static int  mainGraphH() { return H() - MAIN_GRAPH_Y - 6; }
static const char* MAIN_LABELS[3] = { "TRIP", "MAX", "AVG" };

static void drawMainChrome() {
  tft.fillScreen(COL_BG);
  drawTopBar();

  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString(speedUnitLabel(), W() / 2, MAIN_UNIT_Y);
  tft.drawFastHLine(MARGIN, MAIN_BAR_Y + 10, W() - 2 * MARGIN, COL_MID);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  for (int i = 0; i < 3; i++) {
    tft.drawString(MAIN_LABELS[i], MARGIN, MAIN_ROW_Y0 + i * MAIN_ROW_H);
  }
  tft.drawString("GEAR", MARGIN, MAIN_GEAR_Y);
}

static void drawConfidenceBar() {
  int fullW = W() - 2 * MARGIN;
  tft.fillRect(MARGIN, MAIN_BAR_Y, fullW, 3, COL_BG);
  float c = getSpeedConfidence();
  if (c < 0) c = 0; if (c > 1) c = 1;
  int w = (int)(fullW * c);
  if (w > 0) tft.fillRect(MARGIN, MAIN_BAR_Y, w, 3, COL_ACC);
}

static void drawMainRow(int i, const char* value) {
  int y = MAIN_ROW_Y0 + i * MAIN_ROW_H;
  tft.fillRect(110, y - 1, W() - 110 - MARGIN, 20, COL_BG);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString(value, W() - MARGIN, y);
}

static void drawGearBar() {
  int gear = getCurrentGear();
  int n = getActiveNumGears();
  bool mv = isGearMoving();
  tft.fillRect(96, MAIN_GEAR_Y - 4, W() - 96, 24, COL_BG);
  int barW = 14, gap = 6;
  int totalW = n * barW + (n - 1) * gap;
  int x0 = (W() - MARGIN) - totalW;
  int y = MAIN_GEAR_Y - 2;
  for (int i = 0; i < n; i++) {
    int x = x0 + i * (barW + gap);
    if ((i + 1) == gear) {
      if (mv) tft.drawRoundRect(x, y, barW, 20, 3, COL_FG);
      else    tft.fillRoundRect(x, y, barW, 20, 3, COL_FG);
    } else {
      tft.drawRoundRect(x, y, barW, 20, 3, COL_MID);
    }
  }
}

static void updateMain() {
  float rawKmh = getSpeedKmh();
  float warnKmh = settings().speedWarnKmh;
  bool warn = (warnKmh > 0.0f && rawKmh >= warnKmh);

  drawSpeedBig(toDisplaySpeed(rawKmh), warn);
  drawConfidenceBar();

  char ck[12];
  clockFormatTime(ck, sizeof(ck));
  tft.fillRect(MARGIN, MAIN_UNIT_Y - 2, 84, 18, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_MID, COL_BG);
  tft.setTextSize(2);
  tft.drawString(ck, MARGIN, MAIN_UNIT_Y);

  tft.fillRect(W() - MARGIN - 44, MAIN_UNIT_Y - 2, 44, 18, COL_BG);
  if (settingsChainKmLeft(rideOdometerKm()) <= 0.0f) {
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_WARN, COL_BG);
    tft.setTextSize(2);
    tft.drawString("OIL", W() - MARGIN, MAIN_UNIT_Y);
  }

  char buf[20];
  snprintf(buf, sizeof(buf), "%.2f %s", toDisplayDistance(rideTripKm()), distUnitLabel());
  drawMainRow(0, buf);
  snprintf(buf, sizeof(buf), "%.1f", toDisplaySpeed(rideMaxSpeedKmh()));
  drawMainRow(1, buf);
  snprintf(buf, sizeof(buf), "%.1f", toDisplaySpeed(rideAvgSpeedKmh()));
  drawMainRow(2, buf);

  drawGearBar();
  drawGraph(MARGIN, MAIN_GRAPH_Y, W() - 2 * MARGIN, mainGraphH(), false);
}

static const char* TELE_LABELS[14] = {
  "TRIP", "ODO", "MAX", "AVG", "TIME", "GRADE", "CADENCE",
  "TEMP", "T-MIN", "T-MAX", "PRESS", "P-MIN", "P-MAX", "ALT"
};
static const int TELE_N = 14;
static int teleRowY(int i) { return TOPBAR_H + 8 + i * 20; }

static void drawTeleChrome() {
  tft.fillScreen(COL_BG);
  drawTopBar();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  for (int i = 0; i < TELE_N; i++) tft.drawString(TELE_LABELS[i], MARGIN, teleRowY(i));
}

static void drawTeleVal(int i, const char* v) {
  int y = teleRowY(i);
  tft.fillRect(120, y - 1, W() - 120 - MARGIN, 18, COL_BG);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString(v, W() - MARGIN, y);
}

static void updateTelemetry() {
  char b[20];
  snprintf(b, sizeof(b), "%.2f%s", toDisplayDistance(rideTripKm()), distUnitLabel()); drawTeleVal(0, b);
  snprintf(b, sizeof(b), "%.1f%s", toDisplayDistance(rideOdometerKm()), distUnitLabel()); drawTeleVal(1, b);
  snprintf(b, sizeof(b), "%.1f", toDisplaySpeed(rideMaxSpeedKmh())); drawTeleVal(2, b);
  snprintf(b, sizeof(b), "%.1f", toDisplaySpeed(rideAvgSpeedKmh())); drawTeleVal(3, b);
  unsigned long t = rideTimeSec();
  snprintf(b, sizeof(b), "%lu:%02lu", t / 60, t % 60); drawTeleVal(4, b);
  snprintf(b, sizeof(b), "%+.1f%%", rideGradePercent()); drawTeleVal(5, b);
  snprintf(b, sizeof(b), "%.0f", rideCadenceRpm()); drawTeleVal(6, b);

  BmpData bmp = readBmpData();
  if (bmp.valid) {
    snprintf(b, sizeof(b), "%.1fC", bmp.temperatureC); drawTeleVal(7, b);
  } else drawTeleVal(7, "--");
  if (rideEnvValid()) {
    snprintf(b, sizeof(b), "%.1fC", rideMinTempC()); drawTeleVal(8, b);
    snprintf(b, sizeof(b), "%.1fC", rideMaxTempC()); drawTeleVal(9, b);
  } else { drawTeleVal(8, "--"); drawTeleVal(9, "--"); }
  if (bmp.valid) {
    snprintf(b, sizeof(b), "%.0f", bmp.pressureHpa); drawTeleVal(10, b);
  } else drawTeleVal(10, "--");
  if (rideEnvValid()) {
    snprintf(b, sizeof(b), "%.0f", rideMinPressureHpa()); drawTeleVal(11, b);
    snprintf(b, sizeof(b), "%.0f", rideMaxPressureHpa()); drawTeleVal(12, b);
  } else { drawTeleVal(11, "--"); drawTeleVal(12, "--"); }
  if (bmp.valid) {
    snprintf(b, sizeof(b), "%.0fm", bmp.altitudeM); drawTeleVal(13, b);
  } else drawTeleVal(13, "--");
}

static void drawGraphChrome() {
  tft.fillScreen(COL_BG);
  drawTopBar();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("SPEED HISTORY", MARGIN, TOPBAR_H + 8);
}

static void updateGraphScreen() {
  int gy = TOPBAR_H + 34;
  int gh = H() - gy - 40;
  drawGraph(MARGIN, gy, W() - 2 * MARGIN, gh, true);
  char b[28];
  tft.fillRect(0, H() - 34, W(), 34, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  snprintf(b, sizeof(b), "now %.1f", toDisplaySpeed(getSpeedKmh()));
  tft.drawString(b, MARGIN, H() - 30);
  tft.setTextDatum(TR_DATUM);
  snprintf(b, sizeof(b), "max %.1f", toDisplaySpeed(rideMaxSpeedKmh()));
  tft.drawString(b, W() - MARGIN, H() - 30);
}

static void drawAltGraphChrome() {
  tft.fillScreen(COL_BG);
  drawTopBar();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("ALTITUDE", MARGIN, TOPBAR_H + 8);
}

static void drawAltGraph(int gx, int gy, int gw, int gh) {
  tft.fillRect(gx, gy, gw, gh, COL_BG);
  tft.drawRect(gx, gy, gw, gh, COL_MID);
  int count, head, cap;
  const float* hist = rideAltHistory(&count, &head, &cap);
  if (count < 2) return;
  float amin = hist[0], amax = hist[0];
  for (int i = 0; i < count; i++) {
    int idx = (head - count + i + cap) % cap;
    float v = hist[idx];
    if (v < amin) amin = v;
    if (v > amax) amax = v;
  }
  float span = amax - amin;
  if (span < 5.0f) { float mid = (amax + amin) * 0.5f; amin = mid - 2.5f; amax = mid + 2.5f; span = 5.0f; }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_MID, COL_BG);
  tft.setTextSize(1);
  char b[10];
  snprintf(b, sizeof(b), "%.0f", amax);
  tft.drawString(b, gx + gw - 2, gy + 2);
  snprintf(b, sizeof(b), "%.0f", amin);
  tft.drawString(b, gx + gw - 2, gy + gh - 10);
  int prevX = 0, prevY = 0;
  bool first = true;
  for (int i = 0; i < count; i++) {
    int idx = (head - count + i + cap) % cap;
    float v = hist[idx];
    int px = gx + 1 + (int)((long)(gw - 2) * i / (count - 1));
    int py = gy + gh - 2 - (int)((float)(gh - 4) * ((v - amin) / span));
    if (py < gy + 1) py = gy + 1;
    if (!first) tft.drawLine(prevX, prevY, px, py, COL_ACC);
    prevX = px; prevY = py; first = false;
  }
}

static void updateAltScreen() {
  tft.fillRect(0, TOPBAR_H + 30, W(), 26, COL_BG);
  char b[28];
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  snprintf(b, sizeof(b), "%.0fm +%.0f", rideAltitudeM(), rideElevationGainM());
  tft.drawString(b, MARGIN, TOPBAR_H + 34);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_ACC, COL_BG);
  snprintf(b, sizeof(b), "%s%+.1f", ridePressureTrendLabel(), ridePressureTrendHpa());
  tft.drawString(b, W() - MARGIN, TOPBAR_H + 34);
  int gy = TOPBAR_H + 62;
  int gh = H() - gy - 8;
  drawAltGraph(MARGIN, gy, W() - 2 * MARGIN, gh);
}

static void drawCurrentScreen();

static void renderSleepClock(bool force) {
  char t[12];
  clockFormatTime(t, sizeof(t));
  if (!force && strcmp(t, sleepClockStr) == 0) return;
  strcpy(sleepClockStr, t);
  tft.fillScreen(COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(dim565(COL_FG, 35), COL_BG);
  tft.setTextSize(4);
  tft.drawString(t, W() / 2, H() / 2);
}

static void enterSleep() {
  screenAsleep = true;
  sleepClockStr[0] = 0;
  renderSleepClock(true);
}

static void wakeScreen() {
  screenAsleep = false;
  lastActivityMs = millis();
  drawCurrentScreen();
}

static void drawSleepClock() { renderSleepClock(false); }

enum ItemType { IT_VALUE, IT_TOGGLE, IT_ACTION };
struct MenuItem { const char* label; ItemType type; };
static const MenuItem MENU[] = {
  { "Units",            IT_TOGGLE },
  { "Brightness",       IT_VALUE  },
  { "Touch sens",       IT_VALUE  },
  { "Speed warn",       IT_VALUE  },
  { "Gears",            IT_VALUE  },
  { "Auto-shift",       IT_TOGGLE },
  { "Speed calib",      IT_VALUE  },
  { "Rider mass",       IT_VALUE  },
  { "Chain interval",   IT_VALUE  },
  { "Service interval", IT_VALUE  },
  { "Invert",           IT_TOGGLE },
  { "Reset trip",       IT_ACTION },
  { "Reset odo",        IT_ACTION },
  { "Chain lubed",      IT_ACTION },
  { "Service done",     IT_ACTION },
  { "Records",          IT_ACTION },
  { "Ride summary",     IT_ACTION },
  { "Ride history",     IT_ACTION },
  { "Servo calib",      IT_ACTION },
  { "Exit",             IT_ACTION },
};
static const int MENU_N = 20;
static const int MENU_PER_PAGE = 6;
static int menuPages() { return (MENU_N + MENU_PER_PAGE - 1) / MENU_PER_PAGE; }

static const int MROW_TOP = 30;
static const int MROW_H   = 40;
static const int MBTN_W   = 36;
static const int MNAV_Y   = 280;
static const int MNAV_H   = 34;
static int mrowY(int slot) { return MROW_TOP + slot * MROW_H; }

static void menuValueStr(int idx, char* out, int n) {
  const AppSettings& s = settings();
  switch (idx) {
    case 0: snprintf(out, n, "%s", (s.units == UNIT_MPH) ? "mph" : "km/h"); break;
    case 1: snprintf(out, n, "%d", s.brightness); break;
    case 2: snprintf(out, n, "%d", s.touchSens); break;
    case 3: if (s.speedWarnKmh <= 0.0f) snprintf(out, n, "off");
            else snprintf(out, n, "%.0f", toDisplaySpeed(s.speedWarnKmh)); break;
    case 4: snprintf(out, n, "%d", s.numGears); break;
    case 5: snprintf(out, n, "%s", s.autoShift ? "ON" : "off"); break;
    case 6: snprintf(out, n, "%d%%", s.speedCalibPct); break;
    case 7: snprintf(out, n, "%.0fkg", s.riderMassKg); break;
    case 8: snprintf(out, n, "%dkm", s.chainLubeKm); break;
    case 9: snprintf(out, n, "%dkm", s.serviceKm); break;
    case 10: snprintf(out, n, "%s", s.inverted ? "ON" : "off"); break;
    default: out[0] = 0; break;
  }
}

static void drawMenuRow(int slot, int idx) {
  int y = mrowY(slot);
  tft.fillRect(0, y, W(), MROW_H, COL_BG);
  const MenuItem& it = MENU[idx];

  if (it.type == IT_ACTION) {
    drawButton(MARGIN, y + 3, W() - 2 * MARGIN, MROW_H - 8, it.label, false, 2);
    return;
  }

  drawButton(MARGIN, y + 3, MBTN_W, MROW_H - 8, "-", false, 2);
  drawButton(W() - MARGIN - MBTN_W, y + 3, MBTN_W, MROW_H - 8, "+", false, 2);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(1);
  tft.drawString(it.label, MARGIN + MBTN_W + 8, y + MROW_H / 2 - 8);
  char v[12]; menuValueStr(idx, v, sizeof(v));
  tft.setTextSize(2);
  tft.drawString(v, MARGIN + MBTN_W + 8, y + MROW_H / 2 + 7);
}

static void drawMenuChrome() {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("SETTINGS", MARGIN, 6);
  char p[12]; snprintf(p, sizeof(p), "%d/%d", menuPage + 1, menuPages());
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_MID, COL_BG);
  tft.drawString(p, W() - MARGIN, 6);

  int base = menuPage * MENU_PER_PAGE;
  for (int slot = 0; slot < MENU_PER_PAGE; slot++) {
    int idx = base + slot;
    if (idx >= MENU_N) break;
    drawMenuRow(slot, idx);
  }

  drawButton(MARGIN, MNAV_Y, 70, MNAV_H, "PREV", false, 2);
  drawButton(W() - MARGIN - 70, MNAV_Y, 70, MNAV_H, "NEXT", false, 2);
}

static void menuAdjust(int idx, int dir) {
  const AppSettings& s = settings();
  switch (idx) {
    case 0: settingsSetUnits(s.units == UNIT_KMH ? UNIT_MPH : UNIT_KMH); break;
    case 1: settingsSetBrightness(s.brightness + dir * BRIGHTNESS_STEP); applyUiColors(); break;
    case 2: settingsSetTouchSens(s.touchSens + dir * TOUCH_SENS_STEP); applyTouchSensitivity(); break;
    case 3: settingsSetSpeedWarn(s.speedWarnKmh + dir * SPEED_WARN_STEP_KMH); break;
    case 4: settingsSetNumGears(s.numGears + dir); break;
    case 5: settingsSetAutoShift(!s.autoShift); break;
    case 6: settingsSetSpeedCalibPct(s.speedCalibPct + dir * SPEED_CALIB_STEP_PCT); break;
    case 7: settingsSetRiderMass(s.riderMassKg + dir * RIDER_MASS_STEP_KG); break;
    case 8: settingsSetChainLubeKm(s.chainLubeKm + dir * MAINT_KM_STEP); break;
    case 9: settingsSetServiceKm(s.serviceKm + dir * MAINT_KM_STEP); break;
    case 10: settingsSetInverted(!s.inverted); applyUiColors(); break;
    default: break;
  }
}

static void gotoScreen(UiScreen s);

static void enterServoCal();
static void enterHistory();

static void handleMenuTap(int sx, int sy) {

  if (inRect(sx, sy, MARGIN, MNAV_Y, 70, MNAV_H)) {
    menuPage = (menuPage - 1 + menuPages()) % menuPages();
    drawMenuChrome(); return;
  }
  if (inRect(sx, sy, W() - MARGIN - 70, MNAV_Y, 70, MNAV_H)) {
    menuPage = (menuPage + 1) % menuPages();
    drawMenuChrome(); return;
  }
  int base = menuPage * MENU_PER_PAGE;
  for (int slot = 0; slot < MENU_PER_PAGE; slot++) {
    int idx = base + slot;
    if (idx >= MENU_N) break;
    int y = mrowY(slot);
    if (sy < y || sy > y + MROW_H) continue;
    const MenuItem& it = MENU[idx];
    if (it.type == IT_ACTION) {
      switch (idx) {
        case 11: rideResetTrip(); break;
        case 12: rideResetOdometer(); break;
        case 13: settingsMarkChainLubed(rideOdometerKm()); sdAppendMaintenance("chain", rideOdometerKm(), clockNowEpochUtc()); break;
        case 14: settingsMarkServiceDone(rideOdometerKm()); sdAppendMaintenance("service", rideOdometerKm(), clockNowEpochUtc()); break;
        case 15: gotoScreen(SCREEN_RECORDS); return;
        case 16: gotoScreen(SCREEN_SUMMARY); return;
        case 17: enterHistory(); return;
        case 18: enterServoCal(); return;
        case 19: gotoScreen(SCREEN_MAIN); return;
      }

      drawButton(MARGIN, y + 3, W() - 2 * MARGIN, MROW_H - 8, "OK", true, 2);
      delay(120);
      drawMenuRow(slot, idx);
      return;
    }
    if (it.type == IT_TOGGLE) {
      menuAdjust(idx, +1);

      if (idx == 0 || idx == 10) drawMenuChrome(); else drawMenuRow(slot, idx);
      return;
    }

    if (sx < W() / 2) menuAdjust(idx, -1); else menuAdjust(idx, +1);
    drawMenuRow(slot, idx);
    return;
  }
}

static const int SC_LABEL_Y  = 40;
static const int SC_ANGLE_Y  = 70;
static const int SC_GEAR_Y   = 108;
static const int SC_COARSE_Y = 150;
static const int SC_FINE_Y   = 192;
static const int SC_SAVE_Y   = 240;
static const int SC_BTN_H    = 36;

static void drawServoCalValue() {

  tft.fillRect(0, SC_LABEL_Y, W(), 24, COL_BG);
  char g[24];
  snprintf(g, sizeof(g), "Gear %d / %d", calGear, getActiveNumGears());
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_ACC, COL_BG);
  tft.setTextSize(2);
  tft.drawString(g, W() / 2, SC_LABEL_Y);

  tft.fillRect(0, SC_ANGLE_Y, W(), 32, COL_BG);
  char b[24];
  snprintf(b, sizeof(b), "%d deg", calAngle);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(3);
  tft.drawString(b, W() / 2, SC_ANGLE_Y);
}

static void drawServoCalFooter() {
  tft.fillRect(0, SC_SAVE_Y + SC_BTN_H + 6, W(), 18, COL_BG);
  char b[40];
  snprintf(b, sizeof(b), "stored gear %d: %d deg", calGear, gearStoredAngle(calGear));
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_MID, COL_BG);
  tft.setTextSize(1);
  tft.drawString(b, W() / 2, SC_SAVE_Y + SC_BTN_H + 8);
}

static void drawServoCalChrome() {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("GEAR CALIB", W() / 2, 8);
  drawServoCalValue();
  int bw = (W() - 3 * MARGIN) / 2;
  drawButton(MARGIN, SC_GEAR_Y, bw, SC_BTN_H, "< GEAR", false, 2);
  drawButton(W() - MARGIN - bw, SC_GEAR_Y, bw, SC_BTN_H, "GEAR >", false, 2);
  drawButton(MARGIN, SC_COARSE_Y, bw, SC_BTN_H, "-10", false, 2);
  drawButton(W() - MARGIN - bw, SC_COARSE_Y, bw, SC_BTN_H, "+10", false, 2);
  drawButton(MARGIN, SC_FINE_Y, bw, SC_BTN_H, "-1", false, 2);
  drawButton(W() - MARGIN - bw, SC_FINE_Y, bw, SC_BTN_H, "+1", false, 2);
  drawButton(MARGIN, SC_SAVE_Y, bw, SC_BTN_H, "SAVE GEAR", false, 2);
  drawButton(W() - MARGIN - bw, SC_SAVE_Y, bw, SC_BTN_H, "BACK", false, 2);
  drawServoCalFooter();
}

static void enterServoCal() {
  calGear = getCurrentGear();
  if (calGear < 1) calGear = 1;
  if (calGear > getActiveNumGears()) calGear = getActiveNumGears();
  calAngle = gearStoredAngle(calGear);
  servoCalMoveTo(calAngle);
  uiScreen = SCREEN_SERVOCAL;
  drawServoCalChrome();
}

static void scMove(int delta) {
  calAngle = constrain(calAngle + delta, ANGLE_MIN, ANGLE_MAX);
  servoCalMoveTo(calAngle);
  drawServoCalValue();
}

static void scSelectGear(int delta) {
  calGear = constrain(calGear + delta, 1, getActiveNumGears());
  calAngle = gearStoredAngle(calGear);
  servoCalMoveTo(calAngle);
  drawServoCalValue();
  drawServoCalFooter();
}

static void handleServoCalTap(int sx, int sy) {
  int bw = (W() - 3 * MARGIN) / 2;
  if (inRect(sx, sy, MARGIN, SC_GEAR_Y, bw, SC_BTN_H)) { scSelectGear(-1); return; }
  if (inRect(sx, sy, W() - MARGIN - bw, SC_GEAR_Y, bw, SC_BTN_H)) { scSelectGear(+1); return; }
  if (inRect(sx, sy, MARGIN, SC_COARSE_Y, bw, SC_BTN_H)) { scMove(-10); return; }
  if (inRect(sx, sy, W() - MARGIN - bw, SC_COARSE_Y, bw, SC_BTN_H)) { scMove(+10); return; }
  if (inRect(sx, sy, MARGIN, SC_FINE_Y, bw, SC_BTN_H)) { scMove(-1); return; }
  if (inRect(sx, sy, W() - MARGIN - bw, SC_FINE_Y, bw, SC_BTN_H)) { scMove(+1); return; }
  if (inRect(sx, sy, MARGIN, SC_SAVE_Y, bw, SC_BTN_H)) {
    servoCalSaveGearAngle(calGear, calAngle);
    drawServoCalFooter();
    return;
  }
  if (inRect(sx, sy, W() - MARGIN - bw, SC_SAVE_Y, bw, SC_BTN_H)) {
    menuPage = 0; gotoScreen(SCREEN_MENU); return;
  }
}

static const char* SUM_LABELS[8] = {
  "Distance", "Time", "Avg speed", "Max speed",
  "Elev gain", "Avg cadence", "Power", "Energy"
};
static int sumRowY(int i) { return 40 + i * 26; }

static void drawSummaryChrome() {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("RIDE SUMMARY", W() / 2, 6);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_MID, COL_BG);
  tft.setTextSize(2);
  for (int i = 0; i < 8; i++) tft.drawString(SUM_LABELS[i], MARGIN, sumRowY(i));
  drawButton(W() / 2 - 40, H() - 40, 80, 32, "BACK", false, 2);
}

static void updateSummary() {
  char v[24];
  for (int i = 0; i < 8; i++) {
    int y = sumRowY(i);
    tft.fillRect(150, y - 1, W() - 150 - MARGIN, 18, COL_BG);
  }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  unsigned long t = rideTimeSec();
  snprintf(v, sizeof(v), "%.2f %s", toDisplayDistance(rideTripKm()), distUnitLabel());
  tft.drawString(v, W() - MARGIN, sumRowY(0));
  snprintf(v, sizeof(v), "%lu:%02lu", t / 60, t % 60);
  tft.drawString(v, W() - MARGIN, sumRowY(1));
  snprintf(v, sizeof(v), "%.1f", toDisplaySpeed(rideAvgSpeedKmh()));
  tft.drawString(v, W() - MARGIN, sumRowY(2));
  snprintf(v, sizeof(v), "%.1f", toDisplaySpeed(rideMaxSpeedKmh()));
  tft.drawString(v, W() - MARGIN, sumRowY(3));
  snprintf(v, sizeof(v), "%.0f m", rideElevationGainM());
  tft.drawString(v, W() - MARGIN, sumRowY(4));
  snprintf(v, sizeof(v), "%.0f rpm", rideAvgCadenceRpm());
  tft.drawString(v, W() - MARGIN, sumRowY(5));
  snprintf(v, sizeof(v), "%.0f W", rideCurrentPowerW());
  tft.drawString(v, W() - MARGIN, sumRowY(6));
  snprintf(v, sizeof(v), "%.0f kcal", rideEnergyKcal());
  tft.drawString(v, W() - MARGIN, sumRowY(7));
}

static void handleSummaryTap(int sx, int sy) {
  if (inRect(sx, sy, W() / 2 - 40, H() - 40, 80, 32)) { menuPage = 0; gotoScreen(SCREEN_MENU); return; }
}

static void drawRecordsScreen() {
  const AppSettings& s = settings();
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("RECORDS", W() / 2, 8);

  const char* L[3] = { "Top speed", "Longest ride", "Max climb" };
  char v[24];
  for (int i = 0; i < 3; i++) {
    int y = 44 + i * 30;
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_MID, COL_BG);
    tft.setTextSize(2);
    tft.drawString(L[i], MARGIN, y);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_FG, COL_BG);
    if (i == 0) snprintf(v, sizeof(v), "%.1f %s", toDisplaySpeed(s.recMaxSpeedKmh), speedUnitLabel());
    else if (i == 1) snprintf(v, sizeof(v), "%.2f %s", toDisplayDistance(s.recMaxTripKm), distUnitLabel());
    else snprintf(v, sizeof(v), "%.0f m", s.recMaxElevM);
    tft.drawString(v, W() - MARGIN, y);
  }

  tft.drawFastHLine(MARGIN, 142, W() - 2 * MARGIN, COL_MID);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_ACC, COL_BG);
  tft.setTextSize(2);
  tft.drawString("MAINTENANCE", W() / 2, 150);

  double odo = rideOdometerKm();
  float left[2] = { settingsChainKmLeft(odo), settingsServiceKmLeft(odo) };
  const char* L2[2] = { "Chain lube", "Service" };
  for (int i = 0; i < 2; i++) {
    int y = 180 + i * 28;
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_MID, COL_BG);
    tft.setTextSize(2);
    tft.drawString(L2[i], MARGIN, y);
    tft.setTextDatum(TR_DATUM);
    if (left[i] <= 0.0f) { tft.setTextColor(COL_WARN, COL_BG); snprintf(v, sizeof(v), "OVERDUE"); }
    else { tft.setTextColor(COL_FG, COL_BG); snprintf(v, sizeof(v), "in %.0f km", left[i]); }
    tft.drawString(v, W() - MARGIN, y);
  }
  drawButton(W() / 2 - 40, H() - 40, 80, 32, "BACK", false, 2);
}

static void handleRecordsTap(int sx, int sy) {
  if (inRect(sx, sy, W() / 2 - 40, H() - 40, 80, 32)) { menuPage = 0; gotoScreen(SCREEN_MENU); return; }
}

static const int HIST_MAX = 40;
static RideFileEntry histEntries[HIST_MAX];
static int histN = 0;
static int histPage = 0;
static const int HIST_PER_PAGE = 7;
static const int HROW_TOP = 36;
static const int HROW_H = 30;
static int histPages() { int p = (histN + HIST_PER_PAGE - 1) / HIST_PER_PAGE; return p < 1 ? 1 : p; }

static char fvName[24] = "";
static RideFileSummary fvSum;
static const int FV_MAX = 110;
static float fvSpeed[FV_MAX];
static float fvAlt[FV_MAX];
static int fvPts = 0;

static void plotSeries(int gx, int gy, int gw, int gh, const float* a, int n, uint16_t col, bool spd) {
  tft.fillRect(gx, gy, gw, gh, COL_BG);
  tft.drawRect(gx, gy, gw, gh, COL_MID);
  if (n < 2) return;
  float mn = spd ? toDisplaySpeed(a[0]) : a[0];
  float mx = mn;
  for (int i = 1; i < n; i++) {
    float vv = spd ? toDisplaySpeed(a[i]) : a[i];
    if (vv < mn) mn = vv;
    if (vv > mx) mx = vv;
  }
  float span = mx - mn;
  if (span < 1.0f) span = 1.0f;
  int prevX = 0, prevY = 0;
  bool first = true;
  for (int i = 0; i < n; i++) {
    float vv = spd ? toDisplaySpeed(a[i]) : a[i];
    int px = gx + 1 + (int)((long)(gw - 2) * i / (n - 1));
    int py = gy + gh - 2 - (int)((float)(gh - 4) * ((vv - mn) / span));
    if (py < gy + 1) py = gy + 1;
    if (!first) tft.drawLine(prevX, prevY, px, py, col);
    prevX = px; prevY = py; first = false;
  }
}

static void drawFileViewScreen() {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString(fvName, MARGIN, 6);

  char b[48];
  tft.setTextSize(1);
  tft.setTextColor(COL_MID, COL_BG);
  if (fvSum.valid) {
    unsigned long t = fvSum.timeSec;
    snprintf(b, sizeof(b), "%.2f%s  %lu:%02lu  max%.1f avg%.1f",
      toDisplayDistance(fvSum.distKm), distUnitLabel(), t / 60, t % 60,
      toDisplaySpeed(fvSum.maxSpeedKmh), toDisplaySpeed(fvSum.avgSpeedKmh));
    tft.drawString(b, MARGIN, 28);
    snprintf(b, sizeof(b), "+%.0fm  cad %.0f  (%d pts)", fvSum.elevGainM, fvSum.avgCadRpm, fvSum.samples);
    tft.drawString(b, MARGIN, 40);
  } else {
    tft.drawString("No data / read error", MARGIN, 28);
  }

  int top = 56;
  int bottom = H() - 44;
  int gh = (bottom - top) / 2 - 6;
  int gw = W() - 2 * MARGIN;
  tft.setTextColor(COL_OK, COL_BG);
  tft.drawString("SPEED", MARGIN, top - 1);
  plotSeries(MARGIN, top + 10, gw, gh - 10, fvSpeed, fvPts, COL_OK, true);
  int top2 = top + gh + 8;
  tft.setTextColor(COL_ACC, COL_BG);
  tft.drawString("ALT", MARGIN, top2 - 1);
  plotSeries(MARGIN, top2 + 10, gw, gh - 10, fvAlt, fvPts, COL_ACC, false);

  drawButton(W() / 2 - 40, H() - 38, 80, 32, "BACK", false, 2);
}

static void openRideFile(const char* name) {
  strncpy(fvName, name, sizeof(fvName) - 1);
  fvName[sizeof(fvName) - 1] = 0;
  sdReadRideSummary(name, &fvSum);
  fvPts = sdReadRideSeries(name, fvSpeed, fvAlt, FV_MAX);
  uiScreen = SCREEN_FILEVIEW;
  drawFileViewScreen();
}

static void drawHistoryScreen() {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.setTextSize(2);
  tft.drawString("RIDE FILES", MARGIN, 6);
  char p[12]; snprintf(p, sizeof(p), "%d/%d", histPage + 1, histPages());
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_MID, COL_BG);
  tft.drawString(p, W() - MARGIN, 6);

  if (histN == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_MID, COL_BG);
    tft.setTextSize(2);
    tft.drawString("No rides on SD", W() / 2, H() / 2);
  } else {
    int base = histPage * HIST_PER_PAGE;
    for (int slot = 0; slot < HIST_PER_PAGE; slot++) {
      int idx = base + slot;
      if (idx >= histN) break;
      int y = HROW_TOP + slot * HROW_H;
      drawButton(MARGIN, y, W() - 2 * MARGIN, HROW_H - 6, histEntries[idx].name, false, 2);
    }
  }
  int ny = H() - 38;
  drawButton(MARGIN, ny, 64, 32, "PREV", false, 2);
  drawButton(W() / 2 - 40, ny, 80, 32, "BACK", false, 2);
  drawButton(W() - MARGIN - 64, ny, 64, 32, "NEXT", false, 2);
}

static void enterHistory() {
  histN = sdListRideFiles(histEntries, HIST_MAX);
  histPage = 0;
  uiScreen = SCREEN_HISTORY;
  drawHistoryScreen();
}

static void handleHistoryTap(int sx, int sy) {
  int ny = H() - 38;
  if (inRect(sx, sy, MARGIN, ny, 64, 32)) { histPage = (histPage - 1 + histPages()) % histPages(); drawHistoryScreen(); return; }
  if (inRect(sx, sy, W() - MARGIN - 64, ny, 64, 32)) { histPage = (histPage + 1) % histPages(); drawHistoryScreen(); return; }
  if (inRect(sx, sy, W() / 2 - 40, ny, 80, 32)) { menuPage = 0; gotoScreen(SCREEN_MENU); return; }
  if (histN == 0) return;
  int base = histPage * HIST_PER_PAGE;
  for (int slot = 0; slot < HIST_PER_PAGE; slot++) {
    int idx = base + slot;
    if (idx >= histN) break;
    int y = HROW_TOP + slot * HROW_H;
    if (sy >= y && sy <= y + HROW_H - 6) { openRideFile(histEntries[idx].name); return; }
  }
}

static void handleFileViewTap(int sx, int sy) {
  if (inRect(sx, sy, W() / 2 - 40, H() - 38, 80, 32)) { uiScreen = SCREEN_HISTORY; drawHistoryScreen(); return; }
}

static void drawCurrentScreen() {
  switch (uiScreen) {
    case SCREEN_MAIN:      drawMainChrome(); break;
    case SCREEN_TELEMETRY: drawTeleChrome(); break;
    case SCREEN_GRAPH:     drawGraphChrome(); break;
    case SCREEN_ALTGRAPH:  drawAltGraphChrome(); break;
    case SCREEN_MENU:      drawMenuChrome(); break;
    case SCREEN_SERVOCAL:  drawServoCalChrome(); break;
    case SCREEN_SUMMARY:   drawSummaryChrome(); break;
    case SCREEN_RECORDS:   drawRecordsScreen(); break;
    case SCREEN_HISTORY:   drawHistoryScreen(); break;
    case SCREEN_FILEVIEW:  drawFileViewScreen(); break;
  }
}

static void gotoScreen(UiScreen s) {
  uiScreen = s;
  drawCurrentScreen();
  updateDisplay();
}

void drawStaticLayout() { drawCurrentScreen(); }

void updateDisplay() {

  bool moving = (getSpeedKmh() >= SLEEP_WAKE_KMH);
  if (moving) lastActivityMs = millis();
  if (screenAsleep) {
    if (moving) {
      wakeScreen();
    } else {
      drawSleepClock();
      return;
    }
  } else if (uiScreen != SCREEN_SERVOCAL &&
             millis() - lastActivityMs > SLEEP_TIMEOUT_MS) {
    enterSleep();
    return;
  }

  blinkPhase = !blinkPhase;
  switch (uiScreen) {
    case SCREEN_MAIN:      updateMain(); break;
    case SCREEN_TELEMETRY: updateTelemetry(); break;
    case SCREEN_GRAPH:     updateGraphScreen(); break;
    case SCREEN_ALTGRAPH:  updateAltScreen(); break;
    case SCREEN_SUMMARY:   updateSummary(); break;
    default: break;
  }
}

static void cycleView(int dir) {
  int v = (int)uiScreen;
  v = (v + dir + NUM_VIEW_SCREENS) % NUM_VIEW_SCREENS;
  gotoScreen((UiScreen)v);
}

static void handleViewTap(int sx, int sy) {
  if (sy <= TOPBAR_H) {
    if (inRect(sx, sy, tbPageX(), 0, TB_W, TOPBAR_H)) { cycleView(+1); return; }
    if (inRect(sx, sy, tbSetX(), 0, TB_W, TOPBAR_H))  { menuPage = 0; gotoScreen(SCREEN_MENU); return; }
    if (inRect(sx, sy, tbInvX(), 0, TB_W, TOPBAR_H)) {
      settingsSetInverted(!settings().inverted);
      applyUiColors();
      drawCurrentScreen();
      updateDisplay();
      return;
    }
  }
}

void pollDisplayTouch() {
  static bool wasTouched = false;
  static bool swallow = false;
  static int downX = 0, downY = 0, lastX = 0, lastY = 0;

  bool raw = touch.touched();
  bool now = false;
  int sx = 0, sy = 0;
  if (raw) {
    TS_Point p = touch.getPoint();
    if (p.z >= touchZThreshold) {
      now = true;
      sx = constrain(map(p.x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, W()), 0, W());
      sy = constrain(map(p.y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, H()), 0, H());
      lastX = sx; lastY = sy;
    }
  }

  if (now) lastActivityMs = millis();

  if (screenAsleep) {
    if (now) { wakeScreen(); updateDisplay(); swallow = true; }
    wasTouched = now;
    return;
  }

  if (now && !wasTouched) {
    downX = lastX = sx;
    downY = lastY = sy;
  } else if (!now && wasTouched) {
    if (swallow) { swallow = false; wasTouched = now; delay(120); return; }

    int dx = lastX - downX;
    int dy = lastY - downY;
    bool isView = (uiScreen == SCREEN_MAIN || uiScreen == SCREEN_TELEMETRY ||
                   uiScreen == SCREEN_GRAPH || uiScreen == SCREEN_ALTGRAPH);
    if (isView && abs(dx) > SWIPE_MIN && abs(dx) > abs(dy) && downY > TOPBAR_H) {
      cycleView(dx < 0 ? +1 : -1);
    } else {
      switch (uiScreen) {
        case SCREEN_MAIN:
        case SCREEN_TELEMETRY:
        case SCREEN_GRAPH:
        case SCREEN_ALTGRAPH: handleViewTap(downX, downY); break;
        case SCREEN_MENU:     handleMenuTap(downX, downY); break;
        case SCREEN_SERVOCAL: handleServoCalTap(downX, downY); break;
        case SCREEN_SUMMARY:  handleSummaryTap(downX, downY); break;
        case SCREEN_RECORDS:  handleRecordsTap(downX, downY); break;
        case SCREEN_HISTORY:  handleHistoryTap(downX, downY); break;
        case SCREEN_FILEVIEW: handleFileViewTap(downX, downY); break;
      }
    }
    delay(120);
  }
  wasTouched = now;
}
