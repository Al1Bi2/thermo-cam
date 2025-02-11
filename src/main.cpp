#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "settings/camera_pins.h"


#include "esp_camera.h"
// put function declarations here:
int myFunction(int, int);

void setup() {
  delay(1000);
  Serial.begin(9600);
  Serial.println("Start");
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  Serial.println(result);
}

void loop() {
  delay(1000);
  Serial.println("Start");
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}