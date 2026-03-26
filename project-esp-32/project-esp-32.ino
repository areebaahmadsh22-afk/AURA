#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// =========================
// WiFi
// =========================
const char* ssid = "Memórias Lisboa";
const char* password = "MEMORIAS";

// =========================
// Firebase Realtime Database
// IMPORTANT: use your actual database URL
// =========================
const char* FIREBASE_BASE = "https://iot-vibes-default-rtdb.europe-west1.firebasedatabase.app";

// =========================
// Pins
// =========================
const int PULSE_PIN = 34;   // Analog input from pulse sensor
const int FAN_PIN   = 26;   // Fan control output

// NEW PINS THAT I JUST ADDED 
const int LED_R = 25;
const int LED_G = 27;
const int LED_B = 14;
const int BUZZER_PIN = 32;

// Change this if your RGB LED is common anode
//const bool RGB_COMMON_ANODE = false;

// =========================
// Pulse detection settings
// =========================
int threshold = 75;                 // You may need to tune this
unsigned long lastBeatTime = 0;
unsigned long lastSampleTime = 0;
bool pulseAboveThreshold = false;
int bpm = 0;

// Debounce / beat timing
const unsigned long minBeatInterval = 300;   // ms -> ignores impossible double beats
const unsigned long sampleInterval  = 10;    // ms between sensor reads

// Firebase send/read timing
unsigned long lastFirebaseUpdate = 0;
const unsigned long firebaseInterval = 6000; // send BPM every 6 seconds

// Fan logic threshold
const int HIGH_BPM_THRESHOLD = 20;

// =========================
// WiFi
// =========================
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// =========================
// Firebase helpers
// =========================
bool firebasePutInt(const String& path, int value) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String(FIREBASE_BASE) + "/" + path + ".json";

  if (!https.begin(client, url)) {
    Serial.println("PUT int: begin failed");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  int code = https.PUT(String(value));

  if (code > 0) {
    Serial.printf("PUT %s = %d [HTTP %d]\n", path.c_str(), value, code);
    https.end();
    return true;
  } else {
    Serial.printf("PUT %s failed: %s\n", path.c_str(), https.errorToString(code).c_str());
    https.end();
    return false;
  }
}

bool firebasePutString(const String& path, const String& value) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String(FIREBASE_BASE) + "/" + path + ".json";

  if (!https.begin(client, url)) {
    Serial.println("PUT string: begin failed");
    return false;
  }

  https.addHeader("Content-Type", "application/json");

  // Strings in Firebase REST must be quoted JSON strings
  String jsonValue = "\"" + value + "\"";
  int code = https.PUT(jsonValue);

  if (code > 0) {
    Serial.printf("PUT %s = %s [HTTP %d]\n", path.c_str(), value.c_str(), code);
    https.end();
    return true;
  } else {
    Serial.printf("PUT %s failed: %s\n", path.c_str(), https.errorToString(code).c_str());
    https.end();
    return false;
  }
}

String firebaseGetString(const String& path, const String& defaultValue = "") {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String(FIREBASE_BASE) + "/" + path + ".json";

  if (!https.begin(client, url)) {
    Serial.println("GET string: begin failed");
    return defaultValue;
  }

  int code = https.GET();
  if (code <= 0) {
    Serial.printf("GET %s failed: %s\n", path.c_str(), https.errorToString(code).c_str());
    https.end();
    return defaultValue;
  }

  String payload = https.getString();
  https.end();

  payload.trim();

  // Remove surrounding quotes if Firebase returns a JSON string
  if (payload.length() >= 2 && payload[0] == '"' && payload[payload.length() - 1] == '"') {
    payload = payload.substring(1, payload.length() - 1);
  }

  return payload;
}

// =========================
// Pulse sensor reading
// Simple threshold-based beat detection
// =========================
void updatePulseSensor() {
  unsigned long now = millis();

  if (now - lastSampleTime < sampleInterval) return;
  lastSampleTime = now;

  int signal = analogRead(PULSE_PIN);

  // For debugging/tuning threshold:
  //Serial.println(signal);

  if (signal > threshold && !pulseAboveThreshold) {
    pulseAboveThreshold = true;

    unsigned long ibi = now - lastBeatTime; // inter-beat interval in ms

    if (ibi > minBeatInterval && lastBeatTime > 0) {
      bpm = 60000 / ibi;
      Serial.print("Beat detected | Signal: ");
      Serial.print(signal);
      Serial.print(" | BPM: ");
      Serial.println(bpm);
    }

    lastBeatTime = now;
  }

  if (signal < threshold) {
    pulseAboveThreshold = false;
  }
}

// =========================
// Fan logic
// =========================
void updateFanFromFirebaseLogic() {
  String emotion = firebaseGetString("emotion", "neutral");
  emotion.trim();
  emotion.toLowerCase();

  bool shouldFanBeOn = (bpm >= HIGH_BPM_THRESHOLD && emotion == "angry");

  digitalWrite(FAN_PIN, shouldFanBeOn ? HIGH : LOW);

  firebasePutString("fan", shouldFanBeOn ? "ON" : "OFF");

  Serial.print("Emotion: ");
  Serial.print(emotion);
  Serial.print(" | BPM: ");
  Serial.print(bpm);
  Serial.print(" | Fan: ");
  Serial.println(shouldFanBeOn ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // LED off at startup
  writeRgb(false, false, false);

  // ESP32 analog setup
  analogReadResolution(12); // values 0-4095

  connectWiFi();


  Serial.println("System ready");
}


// Adding a firebase boolean reader
bool firebaseGetBool(const String& path, bool defaultValue = false) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String(FIREBASE_BASE) + "/" + path + ".json";

  if (!https.begin(client, url)) {
    Serial.println("GET bool: begin failed");
    return defaultValue;
  }

  int code = https.GET();
  if (code <= 0) {
    Serial.printf("GET %s failed: %s\n", path.c_str(), https.errorToString(code).c_str());
    https.end();
    return defaultValue;
  }

  String payload = https.getString();
  https.end();

  payload.trim();
  payload.toLowerCase();

  if (payload == "true") return true;
  if (payload == "false") return false;

  return defaultValue;
}

void writeRgb(bool rOn, bool gOn, bool bOn) {
  digitalWrite(LED_R, rOn ? HIGH : LOW);
  digitalWrite(LED_G, gOn ? HIGH : LOW);
  digitalWrite(LED_B, bOn ? HIGH : LOW);

}

void updateLedFromFirebase() {
  String ledColor = firebaseGetString("ledColor", "#FFFFFF");
  ledColor.trim();
  ledColor.toUpperCase();

  Serial.print("LED color from Firebase: ");
  Serial.println(ledColor);

  if (ledColor == "#FF0000") {          // red
    writeRgb(true, false, false);
  } 
  else if (ledColor == "#00FF00") {     // green
    writeRgb(false, true, false);
  } 
  else if (ledColor == "#0000FF") {     // blue
    writeRgb(false, false, true);
  } 
  else if (ledColor == "#FFD700") {     // yellow / gold
    writeRgb(true, true, false);
  } 
  else if (ledColor == "#800080") {     // purple
    writeRgb(true, false, true);
  } 
  else if (ledColor == "#FFFFFF") {     // white
    writeRgb(true, true, true);
  } 
  else if (ledColor == "#D4A7F0") {     // light purple approximation
    writeRgb(true, false, true);
  } 
  else {
    // default OFF
    writeRgb(false, false, false);
  }
}

void updateBuzzerFromFirebase() {
  bool buzzerOn = firebaseGetBool("buzzer", false);

  Serial.print("Buzzer from Firebase: ");
  Serial.println(buzzerOn ? "true" : "false");

  digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectWiFi();
  }

  // Continuously update pulse reading
  updatePulseSensor();

  // Every 6 seconds, push BPM to Firebase and evaluate fan logic
  unsigned long now = millis();
  if (now - lastFirebaseUpdate >= firebaseInterval) {
    lastFirebaseUpdate = now;

    firebasePutInt("bpm", bpm);
    updateFanFromFirebaseLogic();
    updateLedFromFirebase();
    updateBuzzerFromFirebase();
    
  }
}