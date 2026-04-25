#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>

// ============================================================
//  HARDWARE PINS
//  GPIO numbers on the Seeeduino XIAO ESP32-S3.
// ============================================================
const int PWM_PIN  = 4;   // PWM output to fan  (blue wire,  pin 4 of 4-pin connector)
const int DHT_PIN  = 18;  // DHT11 data line
const int TACH_PIN = 2;   // Fan tachometer input (yellow wire, pin 3) — open-collector,
                           // pulled up internally; no external resistor required
#define   DHT_TYPE DHT11  // Sensor model (DHT11 or DHT22)

// ============================================================
//  WIFI
//  Credentials are injected at build time from secrets.ini
//  (gitignored). Copy secrets.ini.example → secrets.ini and
//  fill in WIFI_SSID and WIFI_PASSWORD before flashing.
// ============================================================
#ifndef WIFI_SSID
#  error "WIFI_SSID not defined. Copy secrets.ini.example to secrets.ini and set your credentials."
#endif
#ifndef WIFI_PASSWORD
#  error "WIFI_PASSWORD not defined. Copy secrets.ini.example to secrets.ini and set your credentials."
#endif

// ============================================================
//  PWM SIGNAL
//  The fan expects a 25 kHz PWM signal on its control wire.
//  8-bit resolution gives duty-cycle values from 0 to 255.
//  LEDC channel 0 is dedicated to fan speed.
// ============================================================
const int PWM_FREQ       = 25000; // Carrier frequency (Hz) — Intel 4-pin fan standard
const int PWM_RESOLUTION = 8;     // Bit depth → 256 steps (0 = off, 255 = 100%)
const int PWM_CHANNEL    = 0;     // ESP32 LEDC hardware channel (0–15)

const int PWM_MIN = 80;   // Minimum duty cycle while the fan is running (≈ 31%).
                           // Below this threshold most fans stall. Tune to your model.
const int PWM_MAX = 255;  // Maximum duty cycle (100%).
                           // Per Intel spec, 100% duty = full speed on compliant fans.

// ============================================================
//  FAN CURVE — temperature zones (°C)
//
//  Zone 1 — OFF:   T ≤ TEMP_IDLE             → PWM 0        (fan stopped)
//  Zone 2 — IDLE:  TEMP_IDLE < T ≤ TEMP_MIN  → PWM_MIN      (keeps air moving quietly)
//  Zone 3 — RAMP:  TEMP_MIN  < T < TEMP_MAX  → linear ramp  (PWM_MIN → PWM_MAX)
//  Zone 4 — FULL:  T ≥ TEMP_MAX              → PWM_MAX      (maximum cooling)
// ============================================================
const float TEMP_IDLE = 21.0f; // Fan switches off below this temperature
const float TEMP_MIN  = 25.0f; // Fan holds minimum speed up to here, then ramp begins
const float TEMP_MAX  = 35.0f; // Ramp ends here; fan runs at full speed above this

// ============================================================
//  SLEW RATE — maximum PWM change per 2 s sample cycle.
//  Prevents abrupt speed jumps caused by small temperature fluctuations.
//
//  UP   = 255 → speed increases immediately (1 cycle) to prioritise cooling
//  DOWN =  70 → speed decreases by ≈ 27% per cycle; full range takes ~8 s
// ============================================================
const int SLEW_RATE_UP   = 255; // Max PWM increase per cycle (0–255)
const int SLEW_RATE_DOWN =  70; // Max PWM decrease per cycle (0–255)

// ============================================================
//  SENSOR & SERVER INSTANCES
// ============================================================
DHT       dht(DHT_PIN, DHT_TYPE); // DHT11 temperature/humidity sensor
WebServer server(80);              // HTTP server on port 80

// ============================================================
//  RUNTIME STATE
//  Written by the main loop every 2 s; read by the HTTP handlers.
// ============================================================
float         currentTemp     = NAN; // Latest temperature reading (°C); NAN until first valid read
float         currentHumidity = NAN; // Latest humidity reading (%); NAN until first valid read
int           currentSpeedPct = 0;   // Fan speed as percentage (0–100), derived from currentPWM
unsigned long currentRPM      = 0;   // Fan speed in RPM measured via tachometer
int           currentPWM      = 0;   // Actual PWM value being output, slew-rate limited (0–255)

// ============================================================
//  TACHOMETER ISR STATE
//  Modified exclusively inside the ISR; read atomically in the main loop.
// ============================================================
volatile unsigned long tachPulseCount = 0; // Pulse count accumulated since last reset
volatile unsigned long lastTachPulse  = 0; // Timestamp (µs) of the last accepted pulse (debounce)

// ---------------------------------------------------------------
// calculateTargetPWM()
// Returns the desired PWM for a given temperature (4 zones):
//   T ≤ TEMP_IDLE             → 0        (fan off)
//   TEMP_IDLE < T ≤ TEMP_MIN  → PWM_MIN  (idle — minimum speed, keeps air moving)
//   TEMP_MIN  < T < TEMP_MAX  → linear ramp PWM_MIN → PWM_MAX
//   T ≥ TEMP_MAX              → PWM_MAX  (full speed)
// Returns currentPWM unchanged on NaN (maintains output on sensor error).
// ---------------------------------------------------------------
int calculateTargetPWM(float temperature) {
  if (isnan(temperature)) {
    return currentPWM;  // Keep current output on sensor error
  }

  if (temperature <= TEMP_IDLE) return 0;
  if (temperature <= TEMP_MIN)  return PWM_MIN;
  if (temperature >= TEMP_MAX)  return PWM_MAX;

  float t = (temperature - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
  return (int)(PWM_MIN + t * (PWM_MAX - PWM_MIN));
}

// ---------------------------------------------------------------

// HTTP GET /data — JSON response with all sensor and fan values
void handleData() {
  if (isnan(currentTemp)) {
    server.send(503, "application/json", "{\"error\":\"sensor not ready\"}");
    return;
  }
  String json = "{";
  json += "\"temperature_c\":"  + String(currentTemp, 1)     + ",";
  json += "\"humidity_pct\":"   + String(currentHumidity, 1) + ",";
  json += "\"fan_speed_pct\":" + String(currentSpeedPct)     + ",";
  json += "\"fan_rpm\":"        + String(currentRPM);
  json += "}";
  server.send(200, "application/json", json);
}

// HTTP GET /metrics — Prometheus text exposition format
void handleMetrics() {
  String body;
  body += "# HELP fan_temperature_celsius Temperature reading from DHT11 sensor\n";
  body += "# TYPE fan_temperature_celsius gauge\n";
  body += "fan_temperature_celsius " + String(isnan(currentTemp) ? 0.0f : currentTemp, 2) + "\n";
  body += "# HELP fan_humidity_percent Relative humidity from DHT11 sensor\n";
  body += "# TYPE fan_humidity_percent gauge\n";
  body += "fan_humidity_percent " + String(isnan(currentHumidity) ? 0.0f : currentHumidity, 2) + "\n";
  body += "# HELP fan_speed_percent Fan PWM duty cycle percentage (0-100)\n";
  body += "# TYPE fan_speed_percent gauge\n";
  body += "fan_speed_percent " + String(currentSpeedPct) + "\n";
  body += "# HELP fan_rpm Fan rotational speed in RPM measured via tachometer\n";
  body += "# TYPE fan_rpm gauge\n";
  body += "fan_rpm " + String(currentRPM) + "\n";
  server.send(200, "text/plain; version=0.0.4; charset=utf-8", body);
}

// ---------------------------------------------------------------

// ISR: increments pulse counter on each falling edge from the TACH pin.
// 5 ms debounce rejects PWM-induced noise on the open-collector TACH line
// (5 ms → max detectable speed ≈ 6000 RPM, well above any real fan).
void IRAM_ATTR tachISR() {
  unsigned long now = micros();
  if (now - lastTachPulse >= 5000) {
    tachPulseCount++;
    lastTachPulse = now;
  }
}

// ---------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("DHT11 Fan Controller Starting...");

  // Configure PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);

  // Set initial fan speed to zero
  ledcWrite(PWM_CHANNEL, 0);

  // Start DHT sensor
  dht.begin();

  // Configure TACH pin with internal pull-up and attach interrupt
  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected — IP: ");
  Serial.println(WiFi.localIP());

  // Register HTTP endpoints and start web server
  server.on("/data",    handleData);
  server.on("/metrics", handleMetrics);
  server.begin();
  Serial.println("Web server started — endpoints: /data  /metrics");

  Serial.println("Fan controller ready!");
  Serial.print("Fan curve: 0% at ");
  Serial.print(TEMP_MIN);
  Serial.print("°C  →  100% at ");
  Serial.print(TEMP_MAX);
  Serial.println("°C");
}

void loop() {
  server.handleClient();

  static unsigned long lastSampleTime = 0;
  unsigned long now = millis();
  if (now - lastSampleTime < 2000) return;
  unsigned long elapsed = now - lastSampleTime;  // actual window, not hardcoded 2000
  lastSampleTime = now;

  // Atomically read and reset tachometer counter
  noInterrupts();
  unsigned long pulses = tachPulseCount;
  tachPulseCount = 0;
  interrupts();

  // Calculate RPM using actual elapsed time.
  // RPM = (pulses / 2 pulses_per_rev) * (60000 ms/min / elapsed ms)
  // Force 0 when fan is commanded off to suppress noise-induced readings.
  if (currentPWM == 0) {
    currentRPM = 0;
  } else {
    currentRPM = (pulses * 30000UL) / elapsed;
  }

  float temperature = dht.readTemperature();  // Celsius
  float humidity    = dht.readHumidity();

  // Check for sensor read errors
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT11 read error — keeping previous fan speed");
    return;
  }

  currentTemp     = temperature;
  currentHumidity = humidity;

  // Calculate target PWM from temperature, then apply slew rate limiter.
  // Speed increases instantly (SLEW_RATE_UP = 255) to react to heat.
  // Speed decreases gradually (SLEW_RATE_DOWN = 70 per cycle ≈ 8 s full range).
  int targetPWM = calculateTargetPWM(temperature);
  if (targetPWM > currentPWM) {
    currentPWM = min(currentPWM + SLEW_RATE_UP, targetPWM);
  } else if (targetPWM < currentPWM) {
    currentPWM = max(currentPWM - SLEW_RATE_DOWN, targetPWM);
  }
  ledcWrite(PWM_CHANNEL, currentPWM);

  currentSpeedPct = map(currentPWM, 0, 255, 0, 100);

  // Serial output
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.print(" °C | Humidity: ");
  Serial.print(humidity, 1);
  Serial.print(" % | PWM Value: ");
  Serial.print(currentPWM);
  Serial.print(" | Fan Speed: ");
  Serial.print(currentSpeedPct);
  Serial.print(" % | RPM: ");
  Serial.println(currentRPM);
}