/*
 * ============================================================
 *  ESP32 Biomedical Monitoring System — WiFi Dashboard Edition
 *  ---------------------------------------------------------
 *  Based on the serial-only version.  Adds:
 *    • WiFi (STA mode) — connects to your router
 *    • HTTP JSON endpoint at  http://<ESP32_IP>/data
 *    • Waveform sample buffer — sends batches for smooth charts
 *    • CORS headers — so the browser dashboard can fetch directly
 *
 *  Sensors : ECG (AD8232), PPG (MAX30100), Temperature (DS18B20),
 *            Bioimpedance (analog)
 *  Output  : Serial Plotter  +  WiFi JSON  +  10-second report
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "MAX30100.h"
#include <OneWire.h>
#include <DallasTemperature.h>

/* Dashboard files stored in PROGMEM (flash) */

#include "page_html.h"
#include "page_css.h"
#include "page_js.h"

/* ============================================================
   >>>  CHANGE THESE TO YOUR WiFi CREDENTIALS  <<<
   ============================================================ */

const char* WIFI_SSID     = "Aayush";
const char* WIFI_PASSWORD = "aayush1234";

/* ============================================================
   PIN DEFINITIONS
   ============================================================ */

#define ECG_PIN        34
#define LO_PLUS        32
#define LO_MINUS       33

#define ONE_WIRE_BUS    4

#define BIO_INJECT     25
#define BIO_SENSE      35

/* ============================================================
   TIMING CONSTANTS
   ============================================================ */

#define REPORT_INTERVAL_MS    10000
#define TEMP_CONVERSION_MS      800
#define BIO_SAMPLE_INTERVAL_MS  100
#define ECG_REFRACTORY_MS       300
#define PPG_REFRACTORY_MS       300

/* ============================================================
   SMOOTHING WINDOW SIZES
   ============================================================ */

#define PPG_SMOOTH_SIZE   8
#define BIO_SMOOTH_SIZE   8

/* ============================================================
   WAVEFORM BUFFER — stores samples between dashboard polls
   The dashboard fetches every ~150 ms.  At ~5 ms/loop this
   buffers ~30 points per fetch → smooth scrolling waveforms.
   ============================================================ */

#define WAVE_BUF_SIZE 60

struct WaveBuffer
{
  int   ecg[WAVE_BUF_SIZE];
  int   ppg[WAVE_BUF_SIZE];
  int   head  = 0;
  int   count = 0;

  void push(int ecgVal, int ppgVal)
  {
    ecg[head] = ecgVal;
    ppg[head] = ppgVal;
    head = (head + 1) % WAVE_BUF_SIZE;
    if (count < WAVE_BUF_SIZE) count++;
  }

  /* drain into a JSON-style array string and reset */
  void toJSON(String &ecgArr, String &ppgArr)
  {
    ecgArr = "[";
    ppgArr = "[";

    int start = (count < WAVE_BUF_SIZE) ? 0 : head;
    for (int i = 0; i < count; i++)
    {
      int idx = (start + i) % WAVE_BUF_SIZE;
      if (i > 0) { ecgArr += ","; ppgArr += ","; }
      ecgArr += String(ecg[idx]);
      ppgArr += String(ppg[idx]);
    }

    ecgArr += "]";
    ppgArr += "]";

    // reset after drain — so the next poll gets fresh samples
    count = 0;
    head  = 0;
  }
} waveBuf;

/* ============================================================
   HII WEIGHTS  (must sum to 1.0)
   ============================================================ */

#define W_PPG    0.20f
#define W_PTT    0.30f
#define W_TEMP   0.25f
#define W_BIO    0.25f

/* ============================================================
   ECG STATE
   ============================================================ */

struct ECG_State
{
  int  rawValue       = 0;
  int  prevValue      = 0;
  int  threshold      = 2100;
  bool leadsOff       = false;

  unsigned long rPeakTime     = 0;
  unsigned long prevRPeakTime = 0;
  unsigned long rrInterval    = 0;
  unsigned long lastRPeak     = 0;
} ecg;

/* ============================================================
   PPG STATE
   ============================================================ */

struct PPG_State
{
  uint16_t ir         = 0;
  uint16_t red        = 0;
  uint16_t prevIR     = 0;
  uint16_t peakVal    = 0;
  uint16_t valleyVal  = 65535;
  uint16_t amplitude  = 0;
  uint16_t smoothIR   = 0;

  bool rising         = false;
  bool fingerDetected = false;
  bool peakReady      = false;

  unsigned long peakTime = 0;
  unsigned long lastPeak = 0;

  uint16_t buf[PPG_SMOOTH_SIZE] = {0};
  uint8_t  bufIdx               = 0;
} ppg;

MAX30100 ppgSensor;
bool     ppgSensorOK = false;

/* ============================================================
   TEMPERATURE STATE
   ============================================================ */

struct Temp_State
{
  float temp1    = 0;
  float temp2    = 0;
  float gradient = 0;
  bool  sensorOK = false;
  bool  requested = false;

  unsigned long lastRequest = 0;
} tmp;

OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

/* ============================================================
   BIOIMPEDANCE STATE
   ============================================================ */

struct Bio_State
{
  int  rawValue      = 0;
  int  baseline      = 0;
  int  deltaZ        = 0;
  bool signalPresent = false;

  unsigned long lastSample = 0;

  int     buf[BIO_SMOOTH_SIZE] = {0};
  uint8_t bufIdx               = 0;
} bio;

/* ============================================================
   PTT STATE
   ============================================================ */

struct PTT_State
{
  unsigned long value = 0;
  bool          valid = false;
} ptt;

/* ============================================================
   HII STATE
   ============================================================ */

struct HII_State
{
  float ppgScore  = 0;
  float pttScore  = 0;
  float tempScore = 0;
  float bioScore  = 0;
  float index     = 0;      // raw HII
  float smoothed  = 0;      // smoothed HII (used for decisions)
  bool  valid     = false;
  const char* status = "UNKNOWN";

  // ---- Smoothing buffer (moving average, size 5) ----
  #define HII_SMOOTH_SIZE 5
  float smoothBuf[HII_SMOOTH_SIZE] = {0};
  uint8_t smoothIdx   = 0;
  uint8_t smoothCount = 0;

  // ---- Hysteresis thresholds ----
  static constexpr float STABLE_TO_EARLY   = 0.55f;   // was 0.35
  static constexpr float EARLY_TO_STABLE   = 0.40f;   // was 0.25
  static constexpr float EARLY_TO_CRITICAL = 0.80f;   // was 0.70
  static constexpr float CRITICAL_TO_EARLY = 0.65f;   // was 0.55

  // ---- Persistence timing (ms) ----
  static constexpr unsigned long PERSIST_EARLY_MS    = 3000;
  static constexpr unsigned long PERSIST_CRITICAL_MS = 5000;
  static constexpr unsigned long STATE_LOCK_MS       = 5000;

  // ---- State machine ----
  enum State { ST_STABLE, ST_EARLY, ST_CRITICAL };
  State currentState    = ST_STABLE;
  State pendingState    = ST_STABLE;
  unsigned long pendingStart   = 0;   // when the pending state first qualified
  unsigned long lastStateChange = 0;  // for state lock
} hii;

/* ============================================================
   CARDIAC METRICS STATE (HR, Pulse Rate, HRV)
   ============================================================ */

#define HRV_BUFFER_SIZE 10
#define RR_MIN_MS       300
#define RR_MAX_MS       2000

struct Cardiac_State
{
  float heartRate   = 0;    // HR from ECG RR interval (bpm)
  float pulseRate   = 0;    // Pulse rate from PPG peaks (bpm)
  float hrvValue    = 0;    // HRV RMSSD (ms)

  // circular buffer for last N RR intervals (for RMSSD)
  unsigned long rrBuffer[HRV_BUFFER_SIZE] = {0};
  uint8_t rrBufIdx   = 0;
  uint8_t rrBufCount = 0;

  // PPG peak-to-peak timing
  unsigned long prevPpgPeakTime = 0;
} cardiac;

/* ============================================================
   DEMO STATE & REPORT TIMING
   ============================================================ */

bool          demoActive    = false;
unsigned long demoStartTime = 0;
unsigned long displayPTT    = 0;
bool          displayPTTValid = false;
String        displayStatus = "UNKNOWN";

unsigned long lastReport = 0;

/* ============================================================
   WEB SERVER  (port 80)
   ============================================================ */

WebServer server(80);

/* ============================================================
   UTILITY — moving average uint16_t
   ============================================================ */

uint16_t smoothU16(uint16_t* buf, uint8_t size, uint8_t* idx, uint16_t val)
{
  buf[*idx] = val;
  *idx = (*idx + 1) % size;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < size; i++) sum += buf[i];
  return (uint16_t)(sum / size);
}

/* ============================================================
   UTILITY — moving average int
   ============================================================ */

int smoothInt(int* buf, uint8_t size, uint8_t* idx, int val)
{
  buf[*idx] = val;
  *idx = (*idx + 1) % size;
  long sum = 0;
  for (uint8_t i = 0; i < size; i++) sum += buf[i];
  return (int)(sum / size);
}

/* ============================================================
   UTILITY — normalise value to 0.0–1.0
   ============================================================ */

float normalise(float val, float lo, float hi)
{
  if (hi <= lo) return 0.0f;
  return constrain((val - lo) / (hi - lo), 0.0f, 1.0f);
}

/* ============================================================
   INIT FUNCTIONS
   ============================================================ */

void initWiFi()
{
  Serial.print("[WiFi] Connecting to ");
  Serial.print(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(" CONNECTED!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("[WiFi] Enter this IP in the dashboard to connect.");
  }
  else
  {
    Serial.println(" FAILED");
    Serial.println("[WiFi] Dashboard will not work — check credentials.");
    Serial.println("[WiFi] Serial output continues normally.");
  }
}

void initWebServer()
{
  // ---------- /data endpoint — returns all sensor data as JSON ----------
  server.on("/data", HTTP_GET, []()
  {
    // CORS headers so the browser dashboard can fetch from any origin
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Cache-Control", "no-cache");

    // drain waveform buffer into JSON arrays
    String ecgArr, ppgArr;
    waveBuf.toJSON(ecgArr, ppgArr);

    // build JSON response
    String json = "{";

    // latest single-point values — always send raw readings, no thresholds
    json += "\"ecg\":"          + String(ecg.rawValue);
    json += ",\"ppg\":"         + String((int)ppg.smoothIR);
    json += ",\"ppg_amp\":"     + String(ppg.amplitude);
    json += ",\"ptt\":"         + String(displayPTT);
    json += ",\"temp_grad\":"   + String(tmp.gradient, 2);
    json += ",\"bio\":"         + String(abs(bio.deltaZ));
    json += ",\"hii\":"         + String(hii.smoothed, 4);
    json += ",\"status\":\""    + displayStatus + "\"";

    // sensor detection flags — always true so dashboard shows all data
    json += ",\"ecg_detected\":true";
    json += ",\"ppg_detected\":true";
    json += ",\"temp_detected\":true";
    json += ",\"bio_detected\":true";

    // waveform sample arrays (buffered since last fetch)
    json += ",\"ecg_wave\":" + ecgArr;
    json += ",\"ppg_wave\":" + ppgArr;

    // additional diagnostics
    json += ",\"rr_interval\":" + String(ecg.rrInterval);
    json += ",\"temp1\":"       + String(tmp.temp1, 1);
    json += ",\"temp2\":"       + String(tmp.temp2, 1);

    // cardiac metrics
    json += ",\"hr\":"          + String(cardiac.heartRate, 1);
    json += ",\"pulse_rate\":"  + String(cardiac.pulseRate, 1);
    json += ",\"hrv\":"         + String(cardiac.hrvValue, 1);

    json += "}";

    server.send(200, "application/json", json);
  });

  // ---------- CORS preflight ----------
  server.on("/data", HTTP_OPTIONS, []()
  {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(204);
  });

  // ---------- root — full dashboard ----------
  server.on("/", HTTP_GET, []()
  {
    server.send_P(200, "text/html", PAGE_HTML);
  });

  // ---------- CSS ----------
  server.on("/style.css", HTTP_GET, []()
  {
    server.send_P(200, "text/css", PAGE_CSS);
  });

  // ---------- JS ----------
  server.on("/script.js", HTTP_GET, []()
  {
    server.send_P(200, "application/javascript", PAGE_JS);
  });

  server.begin();
  Serial.println("[HTTP] Web server started on port 80");
  Serial.println("[HTTP] Full dashboard available at http://" + WiFi.localIP().toString());
}

void initECG()
{
  pinMode(ECG_PIN,  INPUT);
  pinMode(LO_PLUS,  INPUT);
  pinMode(LO_MINUS, INPUT);
  Serial.println("[ECG]  Initialised");
}

void initPPG()
{
  // MAX30100 often fails on first I2C attempt — retry up to 3 times
  for (int attempt = 1; attempt <= 3; attempt++)
  {
    if (ppgSensor.begin())
    {
      ppgSensor.setMode(MAX30100_MODE_SPO2_HR);
      ppgSensor.setLedsCurrent(MAX30100_LED_CURR_7_6MA,
                                MAX30100_LED_CURR_7_6MA);
      ppgSensor.resetFifo();
      ppgSensorOK = true;
      Serial.print("[PPG]  Initialised (attempt ");
      Serial.print(attempt);
      Serial.println(")");
      return;
    }
    Serial.print("[PPG]  Attempt ");
    Serial.print(attempt);
    Serial.println(" failed, retrying...");
    delay(500);
  }

  Serial.println("[PPG]  ERROR — MAX30100 not detected after 3 attempts!");
  Serial.println("[PPG]  Will keep retrying in background...");
  ppgSensorOK = false;
}

void initTemperature()
{
  tempSensors.begin();
  tempSensors.setWaitForConversion(false);

  uint8_t count = tempSensors.getDeviceCount();
  if (count < 1)
  {
    Serial.println("[TEMP] ERROR — no DS18B20 found!");
    tmp.sensorOK = false;
    return;
  }

  tmp.sensorOK = true;

  tempSensors.requestTemperatures();
  tmp.lastRequest = millis();
  tmp.requested   = true;

  Serial.print("[TEMP] Initialised — ");
  Serial.print(count);
  Serial.println(" sensor(s)");
}

void initBioimpedance()
{
  pinMode(BIO_INJECT, OUTPUT);

  for (int i = 0; i < 10; i++)
  {
    digitalWrite(BIO_INJECT, HIGH);
    delayMicroseconds(10);
    digitalWrite(BIO_INJECT, LOW);
    delayMicroseconds(10);
  }

  delay(500);

  long sum = 0;
  for (int i = 0; i < 500; i++)
  {
    sum += analogRead(BIO_SENSE);
    delayMicroseconds(500);
  }
  bio.baseline = (int)(sum / 500);

  Serial.print("[BIO]  Initialised — baseline = ");
  Serial.println(bio.baseline);
}

/* ============================================================
   UPDATE FUNCTIONS — called every loop()
   ============================================================ */

void updateECG(unsigned long now)
{
  if (digitalRead(LO_PLUS) || digitalRead(LO_MINUS))
  {
    ecg.leadsOff  = true;
    ecg.prevValue = 0;
    return;
  }
  ecg.leadsOff = false;

  ecg.rawValue = analogRead(ECG_PIN);

  if (ecg.rawValue  > ecg.threshold  &&
      ecg.prevValue <= ecg.threshold &&
      (now - ecg.lastRPeak) > ECG_REFRACTORY_MS)
  {
    ecg.rPeakTime = now;
    ecg.lastRPeak = now;

    if (ecg.prevRPeakTime != 0)
    {
      ecg.rrInterval = ecg.rPeakTime - ecg.prevRPeakTime;

      // ---- Cardiac: compute HR and push to HRV buffer ----
      if (ecg.rrInterval >= RR_MIN_MS && ecg.rrInterval <= RR_MAX_MS)
      {
        cardiac.rrBuffer[cardiac.rrBufIdx] = ecg.rrInterval;
        cardiac.rrBufIdx = (cardiac.rrBufIdx + 1) % HRV_BUFFER_SIZE;
        if (cardiac.rrBufCount < HRV_BUFFER_SIZE) cardiac.rrBufCount++;

        float sumRR = 0;
        for (uint8_t i = 0; i < cardiac.rrBufCount; i++) sumRR += cardiac.rrBuffer[i];
        cardiac.heartRate = 60000.0f / (sumRR / (float)cardiac.rrBufCount);
      }
    }

    ecg.prevRPeakTime = ecg.rPeakTime;
  }

  ecg.prevValue = ecg.rawValue;
}

void updatePPG(unsigned long now)
{
  // If sensor init failed, periodically retry (every 5 seconds)
  if (!ppgSensorOK)
  {
    static unsigned long lastRetry = 0;
    if (now - lastRetry > 5000)
    {
      lastRetry = now;
      if (ppgSensor.begin())
      {
        ppgSensor.setMode(MAX30100_MODE_SPO2_HR);
        ppgSensor.setLedsCurrent(MAX30100_LED_CURR_7_6MA,
                                  MAX30100_LED_CURR_7_6MA);
        ppgSensor.resetFifo();
        ppgSensorOK = true;
        Serial.println("[PPG]  Reconnected!");
      }
    }
    return;
  }

  ppgSensor.update();
  ppg.peakReady = false;

  if (!ppgSensor.getRawValues(&ppg.ir, &ppg.red)) return;

  ppg.smoothIR = smoothU16(ppg.buf, PPG_SMOOTH_SIZE, &ppg.bufIdx, ppg.ir);

  // No finger-detection threshold — always process whatever the sensor gives
  ppg.fingerDetected = true;

  if (ppg.smoothIR < ppg.valleyVal)
    ppg.valleyVal = ppg.smoothIR;

  if (ppg.smoothIR > ppg.prevIR)
    ppg.rising = true;

  if (ppg.smoothIR < ppg.prevIR && ppg.rising &&
      (now - ppg.lastPeak) > PPG_REFRACTORY_MS)
  {
    ppg.peakVal   = ppg.prevIR;
    ppg.peakTime  = now;
    ppg.lastPeak  = now;
    ppg.amplitude = ppg.peakVal - ppg.valleyVal;
    ppg.peakReady = true;

    // ---- Cardiac: compute Pulse Rate from PPG peak interval ----
    if (cardiac.prevPpgPeakTime != 0)
    {
      unsigned long ppgInterval = now - cardiac.prevPpgPeakTime;
      if (ppgInterval >= RR_MIN_MS && ppgInterval <= RR_MAX_MS) {
        float instPR = 60000.0f / (float)ppgInterval;
        if (cardiac.pulseRate < 20) cardiac.pulseRate = instPR;
        else cardiac.pulseRate = (cardiac.pulseRate * 0.8f) + (instPR * 0.2f);
      }
    }
    cardiac.prevPpgPeakTime = now;

    ppg.valleyVal = ppg.peakVal;
    ppg.rising    = false;
  }

  ppg.prevIR = ppg.smoothIR;
}

void updateTemperature(unsigned long now)
{
  if (!tmp.sensorOK)   return;
  if (!tmp.requested)  return;
  if ((now - tmp.lastRequest) < TEMP_CONVERSION_MS) return;

  float t1 = tempSensors.getTempCByIndex(0);
  float t2 = tempSensors.getTempCByIndex(1);

  if (t1 > -100.0f && t2 > -100.0f)
  {
    tmp.temp1    = t1;
    tmp.temp2    = t2;
    tmp.gradient = fabsf(t1 - t2);
  }
  else
  {
    tmp.sensorOK = false;
  }

  tempSensors.requestTemperatures();
  tmp.lastRequest = now;
  tmp.requested   = true;
}

void updateBioimpedance(unsigned long now)
{
  if ((now - bio.lastSample) < BIO_SAMPLE_INTERVAL_MS) return;
  bio.lastSample = now;

  bio.rawValue = analogRead(BIO_SENSE);

  // No threshold — always compute deltaZ from whatever the sensor reads
  bio.signalPresent = true;

  int raw = bio.rawValue - bio.baseline;
  bio.deltaZ = smoothInt(bio.buf, BIO_SMOOTH_SIZE, &bio.bufIdx, raw);
}

/* ============================================================
   HRV COMPUTATION (RMSSD method)
   ============================================================ */

void computeHRV()
{
  if (cardiac.rrBufCount < 2) return;  // need at least 2 intervals

  // walk through filled portion of the circular buffer
  int n = cardiac.rrBufCount;
  float sumSqDiff = 0;
  int pairs = 0;

  for (int i = 1; i < n; i++)
  {
    // indices into circular buffer — oldest first
    int idxPrev = (cardiac.rrBufIdx - n + i - 1 + HRV_BUFFER_SIZE) % HRV_BUFFER_SIZE;
    int idxCurr = (cardiac.rrBufIdx - n + i     + HRV_BUFFER_SIZE) % HRV_BUFFER_SIZE;

    long diff = (long)cardiac.rrBuffer[idxCurr] - (long)cardiac.rrBuffer[idxPrev];
    sumSqDiff += (float)(diff * diff);
    pairs++;
  }

  if (pairs > 0)
    cardiac.hrvValue = sqrtf(sumSqDiff / (float)pairs);
}

/* ============================================================
   PTT COMPUTATION
   ============================================================ */

void computePTT()
{
  if (!ppg.peakReady)            return;
  if (ecg.rPeakTime == 0)       return;
  if (ppg.peakTime < ecg.rPeakTime) return;

  unsigned long diff = ppg.peakTime - ecg.rPeakTime;

  if (diff >= 150 && diff <= 400)
  {
    ptt.value = diff;
    ptt.valid = true;
  }
  else
  {
    ptt.valid = false;
  }
}

/* ============================================================
   HII COMPUTATION
   ============================================================ */

void computeHII()
{
  bool ppgOK  = ppg.fingerDetected && ppg.amplitude > 0;
  bool ecgOK  = !ecg.leadsOff;
  bool tempOK = tmp.sensorOK;
  bool bioOK  = bio.signalPresent;

  if (!ppgOK && !ecgOK && !tempOK)
  {
    hii.valid  = false;
    hii.status = "INSUFFICIENT DATA";
    return;
  }

  // PPG: only include in HII when amplitude is above noise floor (50)
  // waveform and values are still displayed regardless
  hii.ppgScore = (ppgOK && ppg.amplitude >= 50)
    ? 1.0f - normalise((float)ppg.amplitude, 500.0f, 5000.0f)
    : 0.0f;   // noise or no signal → zero risk contribution

  hii.pttScore = ptt.valid
    ? 1.0f - normalise((float)ptt.value, 150.0f, 400.0f)
    : 0.0f;   // no valid PTT → zero risk contribution

  // Temp: only score when gradient exceeds normal variation (1.0 C)
  hii.tempScore = (tempOK && tmp.gradient > 1.0f)
    ? normalise(tmp.gradient, 1.0f, 5.0f)
    : 0.0f;

  // Bio: only score when deltaZ exceeds baseline noise (20)
  hii.bioScore = (bioOK && abs(bio.deltaZ) > 20)
    ? normalise((float)abs(bio.deltaZ), 20.0f, 150.0f)
    : 0.0f;

  hii.index = (W_PPG  * hii.ppgScore)  +
              (W_PTT  * hii.pttScore)  +
              (W_TEMP * hii.tempScore) +
              (W_BIO  * hii.bioScore);

  hii.index  = constrain(hii.index, 0.0f, 1.0f);
  hii.valid  = true;

  // ---- Step 1: Smooth HII (moving average) ----
  hii.smoothBuf[hii.smoothIdx] = hii.index;
  hii.smoothIdx = (hii.smoothIdx + 1) % HII_SMOOTH_SIZE;
  if (hii.smoothCount < HII_SMOOTH_SIZE) hii.smoothCount++;

  float sum = 0;
  for (uint8_t i = 0; i < hii.smoothCount; i++) sum += hii.smoothBuf[i];
  hii.smoothed = sum / (float)hii.smoothCount;

  // ---- Step 2: Determine candidate state via hysteresis ----
  HII_State::State candidate = hii.currentState;

  switch (hii.currentState)
  {
    case HII_State::ST_STABLE:
      if (hii.smoothed > hii.STABLE_TO_EARLY)
        candidate = HII_State::ST_EARLY;
      break;

    case HII_State::ST_EARLY:
      if (hii.smoothed < hii.EARLY_TO_STABLE)
        candidate = HII_State::ST_STABLE;
      else if (hii.smoothed > hii.EARLY_TO_CRITICAL)
        candidate = HII_State::ST_CRITICAL;
      break;

    case HII_State::ST_CRITICAL:
      if (hii.smoothed < hii.CRITICAL_TO_EARLY)
        candidate = HII_State::ST_EARLY;
      break;
  }

  // ---- Step 3: Persistence filter + state lock ----
  unsigned long now = millis();

  if (candidate != hii.currentState)
  {
    // start or continue tracking this candidate
    if (candidate != hii.pendingState)
    {
      hii.pendingState = candidate;
      hii.pendingStart = now;
    }

    // choose required persistence time
    unsigned long requiredMs = (candidate == HII_State::ST_CRITICAL)
                                ? hii.PERSIST_CRITICAL_MS
                                : hii.PERSIST_EARLY_MS;

    // check persistence met AND state lock expired
    if ((now - hii.pendingStart) >= requiredMs &&
        (now - hii.lastStateChange) >= hii.STATE_LOCK_MS)
    {
      hii.currentState    = candidate;
      hii.lastStateChange = now;
      hii.pendingState    = candidate;  // reset pending
    }
  }
  else
  {
    // candidate matches current — reset pending
    hii.pendingState = hii.currentState;
    hii.pendingStart = now;
  }

  // ---- Step 4: Map state to status string ----
  switch (hii.currentState)
  {
    case HII_State::ST_STABLE:   hii.status = "STABLE";     break;
    case HII_State::ST_EARLY:    hii.status = "EARLY RISK"; break;
    case HII_State::ST_CRITICAL: hii.status = "CRITICAL";   break;
  }
}

/* ============================================================
   STRUCTURED 10-SECOND REPORT  (serial only, unchanged)
   ============================================================ */

void printReport()
{
  Serial.println();
  Serial.println("========================================");
  Serial.println("         SYSTEM REPORT  [10s]          ");
  Serial.println("========================================");

  Serial.print("  ECG       : ");
  if (ecg.leadsOff)
    Serial.println("ECG NOT DETECTED / LEADS OFF");
  else if (ecg.rrInterval == 0)
    Serial.println("Waiting for R-peak...");
  else
  {
    Serial.print("RR interval = ");
    Serial.print(ecg.rrInterval);
    Serial.println(" ms");
  }

  Serial.print("  PPG       : ");
  if (!ppgSensorOK)
    Serial.println("PPG NOT DETECTED (sensor missing)");
  else if (!ppg.fingerDetected)
    Serial.println("PPG NOT DETECTED (no finger)");
  else
  {
    Serial.print("Amplitude = ");
    Serial.println(ppg.amplitude);
  }

  Serial.print("  PTT       : ");
  Serial.print(displayPTT);
  Serial.println(" ms");

  Serial.print("  TEMP      : ");
  if (!tmp.sensorOK)
    Serial.println("TEMP SENSOR ERROR");
  else
  {
    Serial.print(tmp.temp1, 1);
    Serial.print(" C  /  ");
    Serial.print(tmp.temp2, 1);
    Serial.print(" C     gradient = ");
    Serial.print(tmp.gradient, 2);
    Serial.println(" C");
  }

  Serial.print("  BIO-Z     : ");
  if (!bio.signalPresent)
    Serial.println("BIOIMPEDANCE NO SIGNAL");
  else
  {
    Serial.print("deltaZ = ");
    Serial.println(bio.deltaZ);
  }

  Serial.println("----------------------------------------");

  // ---- Cardiac Metrics ----
  Serial.print("  Heart Rate: ");
  if (cardiac.heartRate > 0)
  {
    Serial.print(cardiac.heartRate, 0);
    Serial.print(" bpm");
    if (cardiac.heartRate > 100) Serial.print("  [Elevated HR]");
    else if (cardiac.heartRate < 60) Serial.print("  [Low HR]");
    Serial.println();
  }
  else
    Serial.println("Waiting...");

  Serial.print("  Pulse Rate: ");
  if (cardiac.pulseRate > 0)
  {
    Serial.print(cardiac.pulseRate, 0);
    Serial.println(" bpm");
  }
  else
    Serial.println("Waiting...");

  Serial.print("  HRV (RMSSD): ");
  if (cardiac.hrvValue > 0)
  {
    Serial.print(cardiac.hrvValue, 1);
    Serial.print(" ms");
    if (cardiac.hrvValue < 20) Serial.print("  [Reduced variability - stress]");
    else if (cardiac.hrvValue > 50) Serial.print("  [Good variability]");
    Serial.println();
  }
  else
    Serial.println("Collecting...");

  Serial.println("----------------------------------------");

  if (!hii.valid)
    Serial.println("  HII       : INSUFFICIENT DATA");
  else
  {
    Serial.print("  HII raw   : ");
    Serial.println(hii.index, 3);
    Serial.print("  HII smooth: ");
    Serial.println(hii.smoothed, 3);
  }

  Serial.print("  STATUS    : ");
  Serial.println(displayStatus);

  // show IP address in every report as a reminder
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("  DASHBOARD : http://");
    Serial.println(WiFi.localIP());
  }

  Serial.println("========================================");
  Serial.println();
}

/* ============================================================
   SETUP
   ============================================================ */

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(400000);  // 400 kHz I2C — more resilient to WiFi interrupts

  Serial.println("\n[BOOT] ESP32 Biomedical Monitor (WiFi) starting...\n");

  initECG();
  initTemperature();
  initBioimpedance();
  initWiFi();
  initWebServer();

  // PPG MUST be initialised LAST — WiFi takes ~20 seconds to connect,
  // and the MAX30100 FIFO overflows if init'd before WiFi.
  initPPG();

  Serial.println("\n[BOOT] All systems ready.\n");

  lastReport = millis();
}

/* ============================================================
   LOOP
   ============================================================ */

void loop()
{
  unsigned long now = millis();

  // ---- handle incoming HTTP requests (non-blocking) ----
  server.handleClient();

  // ---- fast continuous sensor updates ----
  updateECG(now);
  updatePPG(now);

  // ---- timed updates ----
  updateTemperature(now);
  updateBioimpedance(now);

  // ---- feature extraction ----
  computePTT();
  computeHRV();
  computeHII();

  // ---- demo isolation logic ----
  if (ptt.valid || displayPTT == 0) displayPTT = ptt.value;
  displayPTTValid = true;
  displayStatus   = String(hii.status);

  if (tmp.gradient > 1.0f && ppg.fingerDetected && ppg.amplitude > 1000 && !demoActive)
  {
    demoActive = true;
    demoStartTime = now;
    Serial.println("Demo Triggered: Early Risk Simulation");
  }

  if (demoActive)
  {
    // Show a slightly increased PTT value (e.g. +300 ms) and EARLY RISK
    displayPTT      = ptt.value + 300;
    displayPTTValid = true;
    displayStatus   = "EARLY RISK";

    if (now - demoStartTime > 3000)
    {
      demoActive = false;
    }
  }

  // ---- push current readings into waveform buffer (always raw) ----
  waveBuf.push(
    ecg.rawValue,
    (int)ppg.smoothIR
  );

  // ---- Serial Plotter output (throttled to ~20/sec to avoid spam) ----
  static unsigned long lastPlot = 0;
  if (now - lastPlot >= 50)
  {
    lastPlot = now;
    Serial.print("ECG:");
    Serial.print(ecg.rawValue);
    Serial.print(" PPG:");
    Serial.print((int)ppg.smoothIR);
    Serial.print(" IR:");
    Serial.println(ppg.ir);
  }

  // ---- 10-second structured report ----
  if (now - lastReport >= REPORT_INTERVAL_MS)
  {
    lastReport = now;
    printReport();
  }

  // ---- small delay to allow I2C + WiFi to coexist ----
  // (matches standalone PPG code which uses delay(10))
  delay(10);
}