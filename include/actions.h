#pragma once
#include "log.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include "captive_portal/captive_portal.h"
#include "fsm.h"
#include "mqtt_handle.h"
#include "camera_handler.h"
namespace Actions {

    void connectWifi() {
        
        lock_ctx();
        fsm->preferences.begin("wifi", false);
        String ssid = fsm->preferences.getString("ssid", "");
        String pass = fsm->preferences.getString("pass", "");
        unlock_ctx();
        if(ssid.length() == 0) {
            fsm->post_event(DeviceEvent::WIFI_FAIL);
        }
        log_debug("Connecting to WiFi...");

        WiFi.begin(ssid, pass);
        for(int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
            delay(500);
            Serial.print(".");
        }
        if(!WiFi.isConnected()) {
            fsm->post_event(DeviceEvent::WIFI_FAIL);
        }

        lock_ctx();
        fsm->device.ip = WiFi.localIP().toString();
        
        unlock_ctx();

        fsm->post_event(DeviceEvent::WIFI_OK);
        

    }

    void resolveIP() {
        if(mdns_init()!= ESP_OK){
            log_info("mDNS failed to start");
            fsm->post_event(DeviceEvent::MDNS_FAIL);
            return;
        }
        int n = 0;
        int found = -1;
        do{
            n = MDNS.queryService("http", "tcp");
            for (int i = 0; i < n; ++i) {
            if(MDNS.hostname(i) == "thermocam-server") {
                if(MDNS.IP(i)!=0){
                found = i;
                }else{
                Serial.println("Strange bug: MDNS.hostname(i) == \"thermocam-server\" but MDNS.IP(i)==0.0.0.0");
                }
            }
            Serial.println(MDNS.hostname(i));
            delay(100);
            }
        } while(found == -1);

        Serial.print("Found server: ");
        Serial.println(MDNS.hostname(found));
        Serial.print("IP: ");
        Serial.println(MDNS.IP(found).toString());

        lock_ctx();
        fsm->device.server_ip = MDNS.IP(found).toString();
        unlock_ctx();


        mdns_free();
        fsm->post_event(DeviceEvent::SERVER_FOUND);
    }

    void startCaptivePortal(){
        captive_portal::startCaptivePortal();
    }

 

    void mqttConnect() {
        mqtt::connectMqtt();
    }

    void discoverServer() {
        xTaskCreatePinnedToCore(mqtt::loop_mqtt, "loop_mqtt", 4906, NULL, 2, NULL, 1);
        xTaskCreatePinnedToCore(mqtt::handle_message, "handle_message", 4096, NULL, 2, NULL, 1);
        log_debug("Started MQTT loop");
        mqtt::discoverServer();
    }

    void getReady() {
        mqtt::get_ready();
        xTaskCreatePinnedToCore(cam_server::mjpeg_start, "cam_server", 4906, NULL, 2, NULL, 1);
    }

    void startCapture() {
        mqtt::start_stream();
        log_info("Started stream");

    }

    void stopCapture() {
        mqtt::stop_stream();
        log_info("Stopped stream");

    }


    void restart() {
        WiFi.disconnect(true);
        ESP.restart();
    }

    void nothing(){
        log_info("nothing");
    }
}