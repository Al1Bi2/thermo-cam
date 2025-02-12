#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "esp_camera.h"

#include <settings/camera_pins.h>
#include <captive_portal/captive_portal.h>


Preferences preferences;
// put function declarations here:
int myFunction(int, int);

void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Start");
  preferences.begin("wifi", false);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");

  if (ssid.length() == 0) {
      
      startCaptivePortal();
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    startCaptivePortal();
  }


}

void loop() {
  delay(1000);
  Serial.println("GOAAAAL");

}

