#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <ESPmDNS.h>

#include <captive_portal/captive_portal.h>
#include <mqtt_handle.h>
#include <camera_handler.h>
Preferences preferences;
String unique_id;

void get_unique_id(){
  unique_id = WiFi.macAddress();
  unique_id.replace(":","");
  unique_id = "esp32-" + unique_id;
}
/*void get_efuse_numeric_id(){
  uint64_t num_id = 0LL;
  esp_efuse_mac_get_default((uint8_t*) (&num_id));
  unique_id = String(num_id, HEX);
}*/


void browseService(const char *service, const char *proto) {
  Serial.printf("Browsing for service _%s._%s.local. ... ", service, proto);
  int n = MDNS.queryService(service, proto);
  if (n == 0) {
    Serial.println("no services found");
  } else {
    Serial.print(n);
    Serial.println(" service(s) found");
    for (int i = 0; i < n; ++i) {
      // Print details for each service found
      Serial.print("  ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(MDNS.txt(i,0));
      Serial.print(" - ");
      Serial.print(MDNS.hostname(i));
      Serial.print(" (");
      Serial.print(MDNS.IP(i).toString());
      Serial.print(":");
      Serial.print(MDNS.port(i));
      Serial.println(")");
    }
  }
  Serial.println();
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  get_unique_id();
  Serial.printf("Unique ID: %s\n", unique_id.c_str());
  Serial.println("Start");

  connectToWiFi();

  if (!MDNS.begin(unique_id.c_str())) {
    Serial.println("Error setting up MDNS responder!");
    delay(1000);
    ESP.restart();
  }
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);

  browseService("mqtt", "tcp");
  browseService("http", "tcp");
  
  xTaskCreatePinnedToCore(loopMQTT, "amg8833", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(streaming_task, "streaming_task", 4096, NULL, 2, NULL, 1);


}

void loop() {
  vTaskDelay(1000);

}

