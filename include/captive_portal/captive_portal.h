#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
//#include <ESPmDNS.h>
const String localIPURL = "http://esp32.local";
const char CAPTIVE_PORTAL_HTML[] PROGMEM = R"rawliteral(
    <form action='/save' method='POST'>
        SSID: <input type='text' name='ssid'><br>
        Password: <input type='password' name='pass'><br>
        <input type='submit' value='Save'>
    </form>
    )rawliteral";

DNSServer dnsServer;
WebServer server(80);
extern Preferences preferences;
void handleRoot() {
    Serial.println("Root");
    server.send(200, "text/html", CAPTIVE_PORTAL_HTML);
}
void handleSave() {String ssid = server.arg("ssid");
    Serial.println("Save");
    String pass = server.arg("pass");
    if (ssid.length() > 0 && pass.length() > 0) {
        preferences.putString("ssid", ssid);
        preferences.putString("pass", pass);
        server.send(200, "text/html", "Настройки сохранены! Перезагрузка...");
        delay(1000);
        ESP.restart();
    }else{
        server.send(200, "text/html", CAPTIVE_PORTAL_HTML);
    }
}

void startCaptivePortal(String hostname) {
    Serial.println("Starting Captive Portal...");
    WiFi.softAP(hostname);
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.print("ESP32 IP as soft AP: ");
    Serial.println(WiFi.softAPIP());
    server.on("/", HTTP_GET,handleRoot );
    
    server.on("/save", HTTP_POST, handleSave);
    
    server.begin();
    while (1) {
        dnsServer.processNextRequest();
        server.handleClient();
        delay(100);
    }
    server.stop();
    dnsServer.stop();
}

void connectToWiFi(String hostname) {
    preferences.begin("wifi", false);
    String ssid = preferences.getString("ssid", "");
    String pass = preferences.getString("pass", "");
    if(ssid.length() == 0) {
        startCaptivePortal(hostname);
    }
    Serial.println("Connecting to WiFi...");
    WiFi.begin(preferences.getString("ssid").c_str(), preferences.getString("pass").c_str());
    for(int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print(".");
    }
    if(!WiFi.isConnected()) {
        startCaptivePortal(hostname);
    }
    Serial.println("Connected to WiFi");
}
