#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "log.h"
#include "fsm.h"

#include <ESPmDNS.h>
namespace captive_portal {
    

const String localIPURL = "http://esp32.local";
const char CAPTIVE_PORTAL_HTML[] PROGMEM = R"rawliteral(
    <form action='/save' method='POST'>
        SSID: <input type='text' name='ssid'><br>
        Password: <input type='password' name='pass'><br>
        <input type='submit' value='Save'>
    </form>
    )rawliteral";
const String IP = "192.168.0.1";


void handleRoot() {
    Serial.println("Root");
    fsm->device.web_server->send(200, "text/html", CAPTIVE_PORTAL_HTML);
}

void handleSave() {
    log_info("Saving settings...");
    String ssid = fsm->device.web_server->arg("ssid");
    String pass = fsm->device.web_server->arg("pass");

    if (ssid.length() > 0 && pass.length() > 0) {
        fsm->device.preferences.putString("ssid", ssid);
        fsm->device.preferences.putString("pass", pass);
        fsm->device.web_server->send(200, "text/html", "Настройки сохранены! Перезагрузка...");
        delay(1000);
        fsm->post_event(DeviceEvent::REBOOT);
    }else{
        fsm->device.web_server->send(200, "text/html", CAPTIVE_PORTAL_HTML);
    }
}


void startCaptivePortal() {
    lock_ctx();
    String id = fsm->device.id;
    unlock_ctx();
    WiFi.softAPConfig(IPAddress().fromString(IP), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(id);
    log_info("Ip address: " + WiFi.softAPIP().toString());
    MDNS.begin(localIPURL);
    MDNS.addService("http", "tcp", 80);
    lock_ctx();
    fsm->device.web_server = new WebServer(80);
    unlock_ctx();

    fsm->device.web_server->on("/", HTTP_GET,handleRoot );
    
    fsm->device.web_server->on("/save", HTTP_POST, handleSave);
    
    fsm->device.web_server->begin();
    while (1) {
        fsm->device.web_server->handleClient();
        delay(100);
    }

    fsm->device.web_server->stop();
    MDNS.end();
    lock_ctx();
    delete fsm->device.web_server;
    unlock_ctx();
    
}

}