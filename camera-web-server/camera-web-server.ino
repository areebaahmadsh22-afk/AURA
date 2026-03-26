#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// =========================
// WiFi
// =========================
//const char* WIFI_SSID = "ValeriaRed";
//const char* WIFI_PASSWORD = "Gabu1234";
//const char *ssid = "Areeba's A26";
//const char *password =  "Ahmad196!";
const char* WIFI_SSID = "Labs-LSD";
const char* WIFI_PASSWORD = "aulaslsd";
// =========================
// Firebase Realtime Database
// Root-level key: takePhoto
// =========================
const char* FIREBASE_URL = "https://iot-vibes-default-rtdb.europe-west1.firebasedatabase.app/takePhoto.json";

// =========================
// Your computer Flask server
// Example: http://192.168.1.34:5000/upload
// =========================
const char* UPLOAD_URL = "http://194.210.156.68:5001/upload";

// =========================
// AI Thinker ESP32-CAM pins
// =========================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

unsigned long lastCheck = 0;
const unsigned long checkInterval = 1500; // ms

void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;   // Good balance
    config.jpeg_quality = 10;            // Lower = better quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    while (true) {
      delay(1000);
    }
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
  }

  Serial.println("Camera initialized");
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("ESP32-CAM IP: ");
  Serial.println(WiFi.localIP());
}

bool getTakePhotoFlag() {
  WiFiClientSecure client;
  client.setInsecure(); // for HTTPS

  HTTPClient https;
  if (!https.begin(client, FIREBASE_URL)) {
    Serial.println("Failed to connect to Firebase");
    return false;
  }

  int httpCode = https.GET();
  if (httpCode <= 0) {
    Serial.printf("Firebase GET failed: %s\n", https.errorToString(httpCode).c_str());
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  payload.trim();
  Serial.print("Firebase takePhoto = ");
  Serial.println(payload);

  return payload == "true";
}

bool resetTakePhotoFlag() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (!https.begin(client, FIREBASE_URL)) {
    Serial.println("Failed to reconnect to Firebase for reset");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  int httpCode = https.PUT("false");

  if (httpCode > 0) {
    Serial.println("takePhoto reset to false");
    https.end();
    return true;
  } else {
    Serial.printf("Firebase PUT failed: %s\n", https.errorToString(httpCode).c_str());
    https.end();
    return false;
  }
}

bool uploadPhoto(camera_fb_t *fb) {
  HTTPClient http;
  if (!http.begin(UPLOAD_URL)) {
    Serial.println("Failed to connect to upload server");
    return false;
  }

  http.addHeader("Content-Type", "image/jpeg");
  int httpResponseCode = http.POST(fb->buf, fb->len);

  Serial.print("Upload response code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(response);
    http.end();
    return true;
  } else {
    Serial.printf("Upload failed: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }
}

void captureAndUpload() {
  Serial.println("Capturing photo...");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.print("Photo size: ");
  Serial.println(fb->len);

  bool uploaded = uploadPhoto(fb);

  esp_camera_fb_return(fb);

  if (uploaded) {
    Serial.println("Photo uploaded successfully");
    resetTakePhotoFlag();
  } else {
    Serial.println("Photo upload failed");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  startCamera();
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    connectWiFi();
  }

  if (millis() - lastCheck >= checkInterval) {
    lastCheck = millis();

    bool takePhoto = getTakePhotoFlag();
    if (takePhoto) {
      captureAndUpload();
    }
  }
}