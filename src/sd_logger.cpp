#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "sd_logger.h"

#if FEATURE_SD
#include <SPI.h>
#include <SD.h>
#include "ride_stats.h"
#include "mpu_speed.h"
#include "mpu_sensor.h"
#include "bmp_sensor.h"
#include "gear_control.h"
#include "app_clock.h"

static SPIClass sdSPI(HSPI);
static bool          sdReady = false;
static char          fileName[24] = {0};
static int           rideNumber = 0;
static unsigned long lastLogMs = 0;
static unsigned long startMs = 0;

static int rideNumberFromName(const char* name) {
  if (!name) return -1;

  if (name[0] == '/') name++;
  size_t plen = strlen(SD_FILE_PREFIX);
  if (strncasecmp(name, SD_FILE_PREFIX, plen) != 0) return -1;
  const char* p = name + plen;
  if (*p < '0' || *p > '9') return -1;
  long n = 0;
  while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
  return (int)n;
}

static int findHighestRideNumber() {
  int highest = 0;
  File root = SD.open("/");
  if (!root) return highest;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (!f.isDirectory()) {
      int n = rideNumberFromName(f.name());
      if (n > highest) highest = n;
    }
    f.close();
  }
  root.close();
  return highest;
}

void sdLoggerBegin() {
  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSPI)) {
    DBG_PRINTLN("[sd] card not found / mount failed");
    sdReady = false;
    return;
  }

  rideNumber = findHighestRideNumber() + 1;
  snprintf(fileName, sizeof(fileName), "/%s%d.CSV", SD_FILE_PREFIX, rideNumber);

  File f = SD.open(fileName, FILE_WRITE);
  if (f) {
    f.println("epoch,t_s,speed_kmh,gear,temp_c,press_hpa,alt_m,grade_pct,cad_rpm,trip_km,pitch_deg,roll_deg");
    f.close();
    sdReady = true;
    startMs = millis();
    DBG_PRINTF("[sd] logging to %s\n", fileName);
  } else {
    sdReady = false;
    DBG_PRINTLN("[sd] could not open log file");
  }
}

bool sdLoggerAvailable()    { return sdReady; }
const char* sdCurrentFile() { return sdReady ? fileName : ""; }
int  sdCurrentRideNumber()  { return sdReady ? rideNumber : 0; }

void sdLoggerUpdate() {
  if (!sdReady) return;
  unsigned long now = millis();
  if (now - lastLogMs < SD_LOG_INTERVAL_MS) return;
  lastLogMs = now;

  BmpData b = readBmpData();
  MpuData m = readMpuData();
  uint32_t epoch = clockNowEpochUtc();

  File f = SD.open(fileName, FILE_APPEND);
  if (!f) return;
  char line[160];
  snprintf(line, sizeof(line),
           "%lu,%lu,%.2f,%d,%.1f,%.1f,%.1f,%+.1f,%.0f,%.3f,%+.1f,%+.1f",
           (unsigned long)epoch,
           (now - startMs) / 1000UL,
           getSpeedKmh(),
           getCurrentGear(),
           b.valid ? b.temperatureC : 0.0f,
           b.valid ? b.pressureHpa : 0.0f,
           b.valid ? b.altitudeM : 0.0f,
           rideGradePercent(),
           rideCadenceRpm(),
           rideTripKm(),
           m.valid ? m.pitchDeg : 0.0f,
           m.valid ? m.rollDeg : 0.0f);
  f.println(line);
  f.close();
}

static int readCsvLine(File& f, char* buf, int maxLen) {
  int i = 0;
  bool any = false;
  while (f.available()) {
    char c = (char)f.read();
    any = true;
    if (c == '\n') break;
    if (c == '\r') continue;
    if (i < maxLen - 1) buf[i++] = c;
  }
  buf[i] = 0;
  return any ? i : -1;
}

static float csvField(const char* line, int idx) {
  int col = 0;
  const char* p = line;
  while (col < idx && *p) {
    if (*p == ',') col++;
    p++;
  }
  return (float)atof(p);
}

static bool csvIsData(const char* line) {
  return (line[0] >= '0' && line[0] <= '9');
}

int sdListRideFiles(RideFileEntry* out, int maxN) {
  if (!sdReady || !out || maxN <= 0) return 0;
  int count = 0;
  File root = SD.open("/");
  if (!root) return 0;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (!f.isDirectory()) {
      int n = rideNumberFromName(f.name());
      if (n >= 0) {
        int pos = count;
        while (pos > 0 && out[pos - 1].number < n) {
          if (pos < maxN) out[pos] = out[pos - 1];
          pos--;
        }
        if (pos < maxN) {
          out[pos].number = n;
          snprintf(out[pos].name, sizeof(out[pos].name), "/%s%d.CSV", SD_FILE_PREFIX, n);
          if (count < maxN) count++;
        }
      }
    }
    f.close();
  }
  root.close();
  return count;
}

bool sdReadRideSummary(const char* name, RideFileSummary* out) {
  if (!sdReady || !name || !out) return false;
  out->valid = false;
  out->distKm = 0.0f;
  out->timeSec = 0;
  out->maxSpeedKmh = 0.0f;
  out->avgSpeedKmh = 0.0f;
  out->elevGainM = 0.0f;
  out->avgCadRpm = 0.0f;
  out->samples = 0;

  File f = SD.open(name, FILE_READ);
  if (!f) return false;

  char buf[200];
  double sumSpeed = 0.0, sumCad = 0.0;
  int speedN = 0, cadN = 0;
  bool first = true;
  float altEMA = 0.0f, lastClimb = 0.0f, elev = 0.0f;

  while (f.available()) {
    int len = readCsvLine(f, buf, sizeof(buf));
    if (len <= 0 || !csvIsData(buf)) continue;

    float spd = csvField(buf, 2);
    float alt = csvField(buf, 6);
    float cad = csvField(buf, 8);

    out->timeSec = (unsigned long)csvField(buf, 1);
    out->distKm  = csvField(buf, 9);
    if (spd > out->maxSpeedKmh) out->maxSpeedKmh = spd;
    if (spd > 1.0f) { sumSpeed += spd; speedN++; }
    if (cad > 0.0f) { sumCad += cad; cadN++; }

    if (first) { altEMA = alt; lastClimb = alt; first = false; }
    else { altEMA = altEMA * 0.9f + alt * 0.1f; }
    if (altEMA > lastClimb + 1.0f) { elev += (altEMA - lastClimb); lastClimb = altEMA; }
    else if (altEMA < lastClimb) { lastClimb = altEMA; }

    out->samples++;
  }
  f.close();

  if (speedN > 0) out->avgSpeedKmh = (float)(sumSpeed / speedN);
  if (cadN > 0)   out->avgCadRpm  = (float)(sumCad / cadN);
  out->elevGainM = elev;
  out->valid = (out->samples > 0);
  return out->valid;
}

int sdReadRideSeries(const char* name, float* speed, float* alt, int maxPts) {
  if (!sdReady || !name || maxPts <= 0) return 0;

  File f = SD.open(name, FILE_READ);
  if (!f) return 0;
  char buf[200];
  int total = 0;
  while (f.available()) {
    int len = readCsvLine(f, buf, sizeof(buf));
    if (len <= 0 || !csvIsData(buf)) continue;
    total++;
  }
  f.close();
  if (total == 0) return 0;

  int stride = (total + maxPts - 1) / maxPts;
  if (stride < 1) stride = 1;

  f = SD.open(name, FILE_READ);
  if (!f) return 0;
  int idx = 0, out = 0;
  while (f.available() && out < maxPts) {
    int len = readCsvLine(f, buf, sizeof(buf));
    if (len <= 0 || !csvIsData(buf)) continue;
    if ((idx % stride) == 0) {
      if (speed) speed[out] = csvField(buf, 2);
      if (alt)   alt[out]   = csvField(buf, 6);
      out++;
    }
    idx++;
  }
  f.close();
  return out;
}

void sdAppendMaintenance(const char* type, double odoKm, uint32_t epoch) {
  if (!sdReady) return;
  File f = SD.open(MAINT_LOG_FILE, FILE_APPEND);
  if (!f) return;
  char line[96];
  snprintf(line, sizeof(line), "%lu,%.2f,%s",
           (unsigned long)epoch, odoKm, type ? type : "");
  f.println(line);
  f.close();
}

#else

void sdLoggerBegin() {}
bool sdLoggerAvailable() { return false; }
const char* sdCurrentFile() { return ""; }
int  sdCurrentRideNumber() { return 0; }
void sdLoggerUpdate() {}
int  sdListRideFiles(RideFileEntry* out, int maxN) { (void)out; (void)maxN; return 0; }
bool sdReadRideSummary(const char* name, RideFileSummary* out) { (void)name; if (out) out->valid = false; return false; }
int  sdReadRideSeries(const char* name, float* speed, float* alt, int maxPts) { (void)name; (void)speed; (void)alt; (void)maxPts; return 0; }
void sdAppendMaintenance(const char* type, double odoKm, uint32_t epoch) { (void)type; (void)odoKm; (void)epoch; }

#endif
