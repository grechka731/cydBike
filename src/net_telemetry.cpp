#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "net_telemetry.h"

#if FEATURE_WIFI
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "app_settings.h"
#include "ride_stats.h"
#include "mpu_speed.h"
#include "mpu_sensor.h"
#include "bmp_sensor.h"
#include "gear_control.h"
#include "sd_logger.h"
#include "app_clock.h"

#if FEATURE_SD
#include <SPI.h>
#include <SD.h>
#endif

#if FEATURE_OTA
#include <Update.h>
#endif

static WebServer server(WIFI_HTTP_PORT);
static bool started = false;
static char ipBuf[20] = "0.0.0.0";
static char staIpBuf[20] = "";
static const bool staWanted = (sizeof(WIFI_STA_SSID) > 1);
static bool staConfigured = false;
static bool staSynced = false;

static const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD Bike</title>
<style>
body{font-family:system-ui,Arial,sans-serif;margin:0;background:#111;color:#eee}
header{background:#1b6;color:#022;padding:14px 18px;font-weight:700;font-size:20px}
.wrap{padding:16px;max-width:680px;margin:auto}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:18px}
.card{background:#1c1c1c;border-radius:10px;padding:12px}
.card .v{font-size:26px;font-weight:700}
.card .l{font-size:12px;color:#9a9}
.big{grid-column:1/3;text-align:center}
.big .v{font-size:54px}
h2{font-size:15px;color:#1b6;border-bottom:1px solid #333;padding-bottom:6px;margin-top:24px}
.row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;gap:10px}
label{flex:1}
input,select{background:#222;color:#eee;border:1px solid #444;border-radius:6px;padding:6px 8px;font-size:15px;width:110px}
button{background:#1b6;color:#022;border:0;border-radius:6px;padding:8px 12px;font-weight:700;cursor:pointer}
button.warn{background:#c44;color:#fff}
a{color:#1b6}
small{color:#888}
.fitem{display:flex;align-items:center;justify-content:space-between;gap:8px;padding:6px 0;border-bottom:1px solid #262626}
canvas{width:100%;background:#161616;border-radius:8px;margin-top:10px}
</style></head><body>
<header>CYD Bike Computer</header>
<div class="wrap">
<div class="grid">
  <div class="card big"><div class="v" id="spd">--</div><div class="l" id="spdu">KM/H</div></div>
  <div class="card"><div class="v" id="trip">--</div><div class="l">TRIP</div></div>
  <div class="card"><div class="v" id="odo">--</div><div class="l">ODOMETER</div></div>
  <div class="card"><div class="v" id="max">--</div><div class="l">MAX</div></div>
  <div class="card"><div class="v" id="avg">--</div><div class="l">AVG</div></div>
  <div class="card"><div class="v" id="time">--</div><div class="l">RIDE TIME</div></div>
  <div class="card"><div class="v" id="grade">--</div><div class="l">GRADE %</div></div>
  <div class="card"><div class="v" id="cad">--</div><div class="l">CADENCE</div></div>
  <div class="card"><div class="v" id="pw">--</div><div class="l">POWER W</div></div>
  <div class="card"><div class="v" id="kcal">--</div><div class="l">KCAL</div></div>
  <div class="card"><div class="v" id="gear">--</div><div class="l">GEAR</div></div>
  <div class="card"><div class="v" id="temp">--</div><div class="l">TEMP C</div></div>
  <div class="card"><div class="v" id="alt">--</div><div class="l">ALT m</div></div>
</div>

<h2>Records</h2>
<div class="row"><label>Top speed</label><span id="recspd">--</span></div>
<div class="row"><label>Longest ride</label><span id="rectrip">--</span></div>
<div class="row"><label>Max climb</label><span id="recelev">--</span></div>
<div class="row"><button class="warn" onclick="act('records')">Reset records</button></div>

<h2>Maintenance</h2>
<div class="row"><label>Chain lube in</label><span id="chainleft">--</span></div>
<div class="row"><label>Service in</label><span id="servleft">--</span></div>
<div class="row"><label>Chain interval (km)</label>
  <input type="number" id="chainkm" min="0" max="20000" step="50" onchange="setv('chainkm',this.value)"></div>
<div class="row"><label>Service interval (km)</label>
  <input type="number" id="servkm" min="0" max="20000" step="50" onchange="setv('servkm',this.value)"></div>
<div class="row">
  <button onclick="maint('chain')">Chain lubed</button>
  <button onclick="maint('service')">Service done</button>
</div>

<h2>Settings</h2>
<div class="row"><label>Units</label>
  <select id="units" onchange="setv('units',this.value)">
    <option value="0">km/h</option><option value="1">mph</option></select></div>
<div class="row"><label>Brightness (20-100)</label>
  <input type="number" id="bright" min="20" max="100" step="10" onchange="setv('bright',this.value)"></div>
<div class="row"><label>Gears (2-8)</label>
  <input type="number" id="gears" min="2" max="8" onchange="setv('gears',this.value)"></div>
<div class="row"><label>Rider mass (kg)</label>
  <input type="number" id="mass" min="30" max="200" step="1" onchange="setv('mass',this.value)"></div>
<div class="row"><label>Speed warning (km/h, 0=off)</label>
  <input type="number" id="warn" min="0" max="80" step="2" onchange="setv('warn',this.value)"></div>
<div class="row"><label>Auto-shift</label>
  <select id="autosh" onchange="setv('autosh',this.value)">
    <option value="0">off</option><option value="1">on</option></select></div>
<div class="row"><label>Speed calibration (%)</label>
  <input type="number" id="calib" min="50" max="200" step="5" onchange="setv('calib',this.value)"></div>
<div class="row"><label>Invert colours</label>
  <select id="inv" onchange="setv('inv',this.value)">
    <option value="0">off</option><option value="1">on</option></select></div>

<h2>Tuning</h2>
<div class="row"><label>Vibration ref speed (km/h)</label>
  <input type="number" id="vref" step="0.5" onchange="setv('vref',this.value)"></div>
<div class="row"><label>Vibration RMS at ref</label>
  <input type="number" id="vrms" step="0.01" onchange="setv('vrms',this.value)"></div>
<div class="row"><label>Vibration min RMS</label>
  <input type="number" id="vmin" step="0.01" onchange="setv('vmin',this.value)"></div>
<div class="row"><label>Vibration fusion pull</label>
  <input type="number" id="pull" step="0.05" onchange="setv('pull',this.value)"></div>
<div class="row"><label>Moving threshold (km/h)</label>
  <input type="number" id="move" step="0.1" onchange="setv('move',this.value)"></div>
<div class="row"><label>Auto-shift min (km/h)</label>
  <input type="number" id="ashmin" step="0.5" onchange="setv('ashmin',this.value)"></div>
<div class="row"><label>Auto-shift max (km/h)</label>
  <input type="number" id="ashmax" step="0.5" onchange="setv('ashmax',this.value)"></div>

<h2>Saved rides</h2>
<div id="rides"><small>Loading...</small></div>
<div id="viewer" style="display:none">
  <div class="row"><b id="vtitle"></b><button onclick="closeView()">Close</button></div>
  <div id="vsum"><small>--</small></div>
  <canvas id="chart" width="640" height="220"></canvas>
</div>

<h2>System</h2>
<div class="row"><label>Set clock from this device</label><button onclick="setclock()">Sync time</button></div>
<div class="row"><label>Firmware update</label><a href="/update">open updater</a></div>

<p><small>AP <b>)HTML" WIFI_AP_SSID R"HTML(</b>. Data refreshes every second.</small></p>
</div>
<script>
function fmt(x,d){return (x===undefined||x===null)?'--':Number(x).toFixed(d);}
function el(id){return document.getElementById(id);}
async function tick(){
 try{
  let r=await fetch('/api/telemetry'); let d=await r.json();
  el('spd').textContent=fmt(d.speed,1); el('spdu').textContent=d.unit;
  el('trip').textContent=fmt(d.trip,2)+' '+d.dunit; el('odo').textContent=fmt(d.odo,1)+' '+d.dunit;
  el('max').textContent=fmt(d.max,1); el('avg').textContent=fmt(d.avg,1);
  let t=d.time|0; el('time').textContent=(t/60|0)+'m'+(t%60)+'s';
  el('grade').textContent=fmt(d.grade,1); el('cad').textContent=fmt(d.cad,0)+' rpm';
  el('pw').textContent=fmt(d.power,0); el('kcal').textContent=fmt(d.kcal,0);
  el('gear').textContent=d.gear+'/'+d.gears; el('temp').textContent=fmt(d.temp,1); el('alt').textContent=fmt(d.alt,0);
 }catch(e){}
}
async function loadcfg(){
 let r=await fetch('/api/config'); let c=await r.json();
 el('units').value=c.units; el('bright').value=c.bright; el('gears').value=c.gears;
 el('mass').value=c.mass; el('warn').value=c.warn; el('autosh').value=c.autosh;
 el('calib').value=c.calib; el('inv').value=c.inv;
 el('recspd').textContent=fmt(c.recspd,1)+' '+c.unit;
 el('rectrip').textContent=fmt(c.rectrip,2)+' '+c.dunit;
 el('recelev').textContent=fmt(c.recelev,0)+' m';
 el('chainleft').textContent=fmt(c.chainleft,0)+' km';
 el('servleft').textContent=fmt(c.servleft,0)+' km';
 el('chainkm').value=c.chainkm; el('servkm').value=c.servkm;
 el('vref').value=c.vref; el('vrms').value=c.vrms; el('vmin').value=c.vmin;
 el('pull').value=c.pull; el('move').value=c.move; el('ashmin').value=c.ashmin; el('ashmax').value=c.ashmax;
}
async function setv(k,v){await fetch('/api/set?key='+k+'&val='+encodeURIComponent(v)); loadcfg();}
async function act(w){if(w!='trip'&&!confirm('Confirm reset?'))return; await fetch('/api/reset?what='+w); loadcfg();}
async function maint(d){await fetch('/api/maint?do='+d); loadcfg();}
async function setclock(){let e=Math.floor(Date.now()/1000); await fetch('/api/settime?epoch='+e); alert('Clock set');}
async function loadrides(){
 try{
  let r=await fetch('/api/rides'); let a=await r.json();
  let h='';
  if(!a.length){h='<small>No saved rides.</small>';}
  for(const it of a){
   h+='<div class="fitem"><span>'+it.name+'</span><span>'+
      '<a href="/dl?f='+encodeURIComponent(it.name)+'">download</a> '+
      '<button onclick="view(\''+it.name+'\')">chart</button></span></div>';
  }
  el('rides').innerHTML=h;
 }catch(e){el('rides').innerHTML='<small>SD unavailable.</small>';}
}
function parseCsv(txt){
 let sp=[],al=[];
 for(const line of txt.split('\n')){
  if(!line||line[0]<'0'||line[0]>'9')continue;
  let c=line.split(',');
  if(c.length<7)continue;
  sp.push(parseFloat(c[2])); al.push(parseFloat(c[6]));
 }
 return {sp,al};
}
function draw(sp,al){
 let cv=el('chart'),x=cv.getContext('2d'),W=cv.width,H=cv.height;
 x.clearRect(0,0,W,H);
 function line(arr,col){
  if(!arr.length)return;
  let mn=Math.min(...arr),mx=Math.max(...arr); if(mx-mn<1e-3)mx=mn+1;
  x.strokeStyle=col;x.lineWidth=2;x.beginPath();
  for(let i=0;i<arr.length;i++){
   let px=W*i/(arr.length-1||1);
   let py=H-((arr[i]-mn)/(mx-mn))*(H-16)-8;
   i?x.lineTo(px,py):x.moveTo(px,py);
  }
  x.stroke();
 }
 line(al,'#39f'); line(sp,'#1b6');
}
async function view(name){
 let r=await fetch('/dl?f='+encodeURIComponent(name)); let t=await r.text();
 let d=parseCsv(t);
 el('vtitle').textContent=name;
 let mx=d.sp.length?Math.max(...d.sp):0;
 el('vsum').innerHTML='<small>'+d.sp.length+' samples, max '+fmt(mx,1)+' km/h (green=speed, blue=altitude)</small>';
 el('viewer').style.display='block';
 draw(d.sp,d.al);
 el('viewer').scrollIntoView({behavior:'smooth'});
}
function closeView(){el('viewer').style.display='none';}
loadcfg(); loadrides(); tick(); setInterval(tick,1000);
</script>
</body></html>)HTML";

static const char UPDATE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA</title>
<style>body{font-family:system-ui,Arial,sans-serif;background:#111;color:#eee;padding:20px}
button{background:#1b6;color:#022;border:0;border-radius:6px;padding:10px 14px;font-weight:700}
#bar{height:10px;background:#1b6;width:0;border-radius:5px;margin-top:12px}</style></head><body>
<h3>Firmware update</h3>
<p>Select a compiled <code>firmware.bin</code> and upload. The board reboots when done.</p>
<form id="f" method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin"><br><br>
<button type="submit">Upload</button></form>
<div id="bar"></div>
<p id="st"></p>
<script>
let f=document.getElementById('f');
f.onsubmit=function(e){
 e.preventDefault();
 let fd=new FormData(f), xhr=new XMLHttpRequest();
 xhr.open('POST','/update');
 xhr.upload.onprogress=function(ev){if(ev.lengthComputable)document.getElementById('bar').style.width=(ev.loaded/ev.total*100)+'%';};
 xhr.onload=function(){document.getElementById('st').textContent=xhr.responseText;};
 xhr.send(fd);
};
</script>
</body></html>)HTML";

static void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleTelemetry() {
  BmpData b = readBmpData();
  char buf[480];
  snprintf(buf, sizeof(buf),
    "{\"speed\":%.1f,\"unit\":\"%s\",\"dunit\":\"%s\","
    "\"trip\":%.2f,\"odo\":%.1f,\"max\":%.1f,\"avg\":%.1f,\"time\":%lu,"
    "\"grade\":%.1f,\"cad\":%.0f,\"power\":%.0f,\"kcal\":%.0f,"
    "\"gear\":%d,\"gears\":%d,\"temp\":%.1f,\"alt\":%.0f}",
    toDisplaySpeed(getSpeedKmh()), speedUnitLabel(), distUnitLabel(),
    toDisplayDistance(rideTripKm()), toDisplayDistance(rideOdometerKm()),
    toDisplaySpeed(rideMaxSpeedKmh()), toDisplaySpeed(rideAvgSpeedKmh()),
    rideTimeSec(), rideGradePercent(), rideCadenceRpm(),
    rideCurrentPowerW(), rideEnergyKcal(),
    getCurrentGear(), getActiveNumGears(),
    b.valid ? b.temperatureC : 0.0f, b.valid ? b.altitudeM : 0.0f);
  server.send(200, "application/json", buf);
}

static void handleConfig() {
  const AppSettings& s = settings();
  double odo = rideOdometerKm();
  char buf[640];
  snprintf(buf, sizeof(buf),
    "{\"units\":%u,\"unit\":\"%s\",\"dunit\":\"%s\",\"bright\":%d,\"gears\":%d,"
    "\"warn\":%.0f,\"autosh\":%d,\"calib\":%d,\"inv\":%d,\"mass\":%.0f,"
    "\"recspd\":%.1f,\"rectrip\":%.2f,\"recelev\":%.0f,"
    "\"chainleft\":%.0f,\"servleft\":%.0f,\"chainkm\":%d,\"servkm\":%d,"
    "\"vref\":%.2f,\"vrms\":%.2f,\"vmin\":%.2f,\"pull\":%.2f,\"move\":%.2f,"
    "\"ashmin\":%.1f,\"ashmax\":%.1f}",
    s.units, speedUnitLabel(), distUnitLabel(), s.brightness, s.numGears,
    s.speedWarnKmh, s.autoShift ? 1 : 0, s.speedCalibPct, s.inverted ? 1 : 0, s.riderMassKg,
    toDisplaySpeed(s.recMaxSpeedKmh), toDisplayDistance(s.recMaxTripKm), s.recMaxElevM,
    settingsChainKmLeft(odo), settingsServiceKmLeft(odo), s.chainLubeKm, s.serviceKm,
    s.tuneVibRefKmh, s.tuneVibRmsAtRef, s.tuneVibMinRms, s.tuneFuseVibPull, s.tuneMovingKmh,
    s.tuneAutoshiftMinKmh, s.tuneAutoshiftMaxKmh);
  server.send(200, "application/json", buf);
}

static void handleSet() {
  String key = server.arg("key");
  String val = server.arg("val");
  int iv = val.toInt();
  float fv = val.toFloat();
  if      (key == "units")  settingsSetUnits((uint8_t)iv);
  else if (key == "bright") settingsSetBrightness(iv);
  else if (key == "gears")  settingsSetNumGears(iv);
  else if (key == "warn")   settingsSetSpeedWarn(fv);
  else if (key == "autosh") settingsSetAutoShift(iv != 0);
  else if (key == "calib")  settingsSetSpeedCalibPct(iv);
  else if (key == "inv")    settingsSetInverted(iv != 0);
  else if (key == "mass")   settingsSetRiderMass(fv);
  else if (key == "chainkm") settingsSetChainLubeKm(iv);
  else if (key == "servkm")  settingsSetServiceKm(iv);
  else                       settingsSetTune(key.c_str(), fv);
  server.send(200, "text/plain", "ok");
}

static void handleReset() {
  String what = server.arg("what");
  if (what == "trip") rideResetTrip();
  else if (what == "odo") rideResetOdometer();
  else if (what == "records") settingsResetRecords();
  server.send(200, "text/plain", "ok");
}

static void handleSetTime() {
  uint32_t epoch = (uint32_t)strtoul(server.arg("epoch").c_str(), nullptr, 10);
  if (epoch > 1700000000UL) clockSetEpoch(epoch);
  server.send(200, "text/plain", "ok");
}

static void handleMaint() {
  String d = server.arg("do");
  double odo = rideOdometerKm();
  uint32_t epoch = clockNowEpochUtc();
  if (d == "chain") {
    settingsMarkChainLubed(odo);
    sdAppendMaintenance("chain", odo, epoch);
  } else if (d == "service") {
    settingsMarkServiceDone(odo);
    sdAppendMaintenance("service", odo, epoch);
  }
  server.send(200, "text/plain", "ok");
}

static void handleRides() {
  RideFileEntry entries[30];
  int n = sdListRideFiles(entries, 30);
  String out = "[";
  for (int i = 0; i < n; i++) {
    if (i) out += ",";
    out += "{\"n\":";
    out += entries[i].number;
    out += ",\"name\":\"";
    out += entries[i].name;
    out += "\"}";
  }
  out += "]";
  server.send(200, "application/json", out);
}

static bool safeRideName(const String& f) {
  if (f.length() < 2 || f.length() > 23) return false;
  if (f[0] != '/') return false;
  if (f.indexOf("..") >= 0) return false;
  return true;
}

static void handleDownload() {
  String f = server.arg("f");
  if (!safeRideName(f)) {
    server.send(400, "text/plain", "bad name");
    return;
  }
#if FEATURE_SD
  File file = SD.open(f.c_str(), FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "not found");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=" + f.substring(1));
  server.streamFile(file, "text/csv");
  file.close();
#else
  server.send(404, "text/plain", "no sd");
#endif
}

#if FEATURE_OTA
static void handleUpdatePage() {
  server.send_P(200, "text/html", UPDATE_HTML);
}

static void handleUpdateDone() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", Update.hasError() ? "update failed" : "update ok, rebooting");
  delay(600);
  ESP.restart();
}

static void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}
#endif

void netBegin() {
  WiFi.mode(staWanted ? WIFI_AP_STA : WIFI_AP);
  bool ok = WiFi.softAP(WIFI_AP_SSID, (sizeof(WIFI_AP_PASS) > 1) ? WIFI_AP_PASS : nullptr);
  if (!ok) {
    DBG_PRINTLN("[net] softAP failed");
    started = false;
    return;
  }
  IPAddress ip = WiFi.softAPIP();
  snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  if (staWanted) WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

  server.on("/", handleRoot);
  server.on("/api/telemetry", handleTelemetry);
  server.on("/api/config", handleConfig);
  server.on("/api/set", handleSet);
  server.on("/api/reset", handleReset);
  server.on("/api/settime", handleSetTime);
  server.on("/api/maint", handleMaint);
  server.on("/api/rides", handleRides);
  server.on("/dl", handleDownload);
#if FEATURE_OTA
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
#endif
  server.begin();
  started = true;
  DBG_PRINTF("[net] AP '%s' at %s\n", WIFI_AP_SSID, ipBuf);
}

void netUpdate() {
  if (!started) return;
  server.handleClient();

  if (staWanted && WiFi.status() == WL_CONNECTED) {
    if (staIpBuf[0] == 0) {
      IPAddress sip = WiFi.localIP();
      snprintf(staIpBuf, sizeof(staIpBuf), "%d.%d.%d.%d", sip[0], sip[1], sip[2], sip[3]);
    }
    if (!staConfigured) {
      configTime(0, 0, NTP_SERVER);
      staConfigured = true;
    } else if (!staSynced) {
      time_t now = time(nullptr);
      if (now > 1700000000) {
        clockSetEpoch((uint32_t)now);
        staSynced = true;
      }
    }
  } else if (staWanted) {
    staIpBuf[0] = 0;
  }
}

bool        netStarted()      { return started; }
const char* netApSsid()       { return WIFI_AP_SSID; }
const char* netApIp()         { return ipBuf; }
int         netClientCount()  { return started ? WiFi.softAPgetStationNum() : 0; }
bool        netStaConnected() { return staWanted && WiFi.status() == WL_CONNECTED; }
const char* netStaIp()        { return staIpBuf; }

#else

void        netBegin() {}
void        netUpdate() {}
bool        netStarted() { return false; }
const char* netApSsid() { return ""; }
const char* netApIp() { return ""; }
int         netClientCount() { return 0; }
bool        netStaConnected() { return false; }
const char* netStaIp() { return ""; }

#endif
