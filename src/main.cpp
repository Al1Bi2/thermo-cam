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

      for(int j = 0; j < MDNS.numTxt(i); j++){
        Serial.print(MDNS.txtKey(i, j));
        Serial.print(" - ");
        Serial.println(MDNS.txt(i, j));
        Serial.print("; ");
      }
      Serial.println();
    }
  }
  Serial.println();

  
}

void find_server(){
 
  
  int n = 0;
  int found = -1;
  do{
    n = MDNS.queryService("http", "tcp");
    for (int i = 0; i < n; ++i) {
      if(MDNS.hostname(i) == "thermocam-server"){
        found = i;
      }
      Serial.println(MDNS.hostname(i));
      delay(100);
    }
  } while(found == -1);


  Serial.print("Found server: ");
  Serial.println(MDNS.hostname(found));
  Serial.print("IP: ");
  Serial.println(MDNS.IP(found).toString());
  WebServer server(80);

  MDNS.end();
  Serial.println("mDNS responder stopped");
  MQTTBrokerIP = MDNS.IP(found).toString();


}

void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  
  get_unique_id();
  Serial.printf("Unique ID: %s\n", unique_id.c_str());
  Serial.println("Start");

  connectToWiFi(unique_id);
  if (!MDNS.begin(unique_id.c_str())) {
    Serial.println("Error setting up MDNS responder!");
    delay(1000);
    ESP.restart();
    MDNS.addService("http", "tcp", 80);
  }
  MDNS.addService("http", "tcp", 80);
  Serial.println("mDNS responder started");
  find_server();
  connect_mqtt();

  delay(100);
  setup_mqtt();
  xTaskCreatePinnedToCore(streaming_task, "streaming_task", 2800, NULL, 2, NULL, 1);


}

void loop() {
}

