#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include <captive_portal/captive_portal.h>
#include <mqtt_handle.h>
#include <camera_handler.h>
Preferences preferences;


void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Start");

  connectToWiFi();
  xTaskCreatePinnedToCore(loopMQTT, "amg8833", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(streaming_task, "streaming_task", 4096, NULL, 2, NULL, 1);


}

void loop() {
  vTaskDelay(1000);

}

