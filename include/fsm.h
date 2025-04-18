#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include "log.h"

enum class DeviceState{
    DISCONNECTED,
    CONNECTED,
    ACTIVE
};

enum class DeviceEvent{
    SERVER_OFFLINE,
    MQTT_ACK_CONNECT,
    MQTT_START_STREAM,
    MQTT_STOP_STREAM
};

struct DeviceContext{
    volatile DeviceState state = DeviceState::DISCONNECTED;

    String id;
    String mqtt_host;
    WiFiClient wifi;
    PubSubClient mqtt;
    WebServer* web_server = nullptr;

    bool streaming = false;
};


extern DeviceContext ctx;


void handle_event(DeviceEvent evt) {
    switch (ctx.state) {
      case DeviceState::DISCONNECTED:
        if (evt == DeviceEvent::MQTT_ACK_CONNECT) {
          ctx.state = DeviceState::CONNECTED;
          log_info("FSM: → CONNECTED");
        }
        break;
  
      case DeviceState::CONNECTED:
        if (evt == DeviceEvent::MQTT_START_STREAM) {
          ctx.state = DeviceState::ACTIVE;
          ctx.mqtt.publish((ctx.id + "/status").c_str(), "active");
          log_info("FSM: → ACTIVE");
        }
        break;
  
      case DeviceState::ACTIVE:
        if (evt == DeviceEvent::MQTT_STOP_STREAM) {
          ctx.state = DeviceState::CONNECTED;
          ctx.mqtt.publish((ctx.id + "/status").c_str(), "connected");
          log_info("FSM: → CONNECTED");
        }
        break;
    }
  
    if (evt == DeviceEvent::SERVER_OFFLINE) {
      if (ctx.state == DeviceState::ACTIVE){}
        //stop_stream();
      ctx.state = DeviceState::DISCONNECTED;
      log_info("FSM: → DISCONNECTED");
    }
  }
  