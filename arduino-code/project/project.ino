#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi
const char* ssid = "Labs-LSD";
const char* password = "aulaslsd";
// PC IP from Flask terminal
const char* serverName = "http://10.2.8.74:5000/status";




// --- PINS ---
const int FAN_PIN = 26;
const int LED_R = 25;
const int LED_G = 27;
const int LED_B = 14;
const int BUZZER_PIN = 32;

void setup() {
  Serial.begin(115200);
  
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  
  pinMode(BUZZER_PIN, OUTPUT);
  
  // The Pin 33 GND Hack for your Buzzer!
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW); 

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
}

void loop() {
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    http.begin(serverName);
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      
      // Parse the JSON from Python
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      String fanState = doc["fan"];
      String ledColor = doc["led"];
      String buzzerState = doc["buzzer"];

      // --- 1. FAN CONTROL ---
      if (fanState == "ON") {
        digitalWrite(FAN_PIN, HIGH);
      } else {
        digitalWrite(FAN_PIN, LOW);
      }

      // --- 2. BUZZER CONTROL ---
      if (buzzerState == "PLAY_TUNE" || buzzerState == "SHORT_BEEP") {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(150);
        digitalWrite(BUZZER_PIN, LOW);
      } else {
         digitalWrite(BUZZER_PIN, LOW);
      }

      // --- 3. RGB LED CONTROL ---
      if (ledColor == "#0000FF") { // Blue (Sad)
        digitalWrite(LED_R, LOW); digitalWrite(LED_G, LOW); digitalWrite(LED_B, HIGH);
      } 
      else if (ledColor == "#FFD700") { // Yellow (Happy)
        digitalWrite(LED_R, HIGH); digitalWrite(LED_G, HIGH); digitalWrite(LED_B, LOW);
      } 
      else { // White (Neutral)
        digitalWrite(LED_R, HIGH); digitalWrite(LED_G, HIGH); digitalWrite(LED_B, HIGH);
      }
    }
    http.end();
  }
  delay(1000); // Check for new emotions every 1 second
}