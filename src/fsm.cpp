#include "fsm.h"
TaskHandle_t fsm_task_handle = nullptr;

FSM<11>* fsm;
DeviceContext ctx = {};
void fsm_rtos_task(void* params) {
    auto* fsm = static_cast<FSM<11>*>(params);
    while(true) {
        fsm->process_events();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

const char* State2Str(DeviceState state){    
    switch(state){
        case DeviceState::INIT:
            return "INIT";
        case DeviceState::WIFI_CONNECTING:
            return "WIFI_CONNECTING";
        case DeviceState::CAPTIVE_PORTAL:
            return "CAPTIVE_PORTAL";
        case DeviceState::IP_RESOLVING:
            return "IP_RESOLVING";
        case DeviceState::MQTT_CONNECTING:
            return "MQTT_CONNECTING";
        case DeviceState::SERVER_CONNECTING:
            return "SERVER_CONNECTING";
        case DeviceState::REGISTERED:
            return "REGISTERED";
        case DeviceState::ACTIVE:
            return "ACTIVE";
        case DeviceState::ANY:
            return "ANY";
        default:
            return "UNKNOWN";
    }
}
const char* Event2Str(DeviceEvent event){
    switch(event){
        case DeviceEvent::BOOT_STARTED:
            return "BOOT_STARTED";
        case DeviceEvent::WIFI_OK:
            return "WIFI_OK";
        case DeviceEvent::WIFI_FAIL:    
            return "WIFI_FAIL";
        case DeviceEvent::SERVER_FOUND:
            return "SERVER_FOUND";
        case DeviceEvent::MDNS_FAIL:
            return "MDNS_FAIL";
        case DeviceEvent::MQTT_OK:
            return "MQTT_OK";
        case DeviceEvent::MQTT_ACK_CONNECT:
            return "MQTT_ACK_CONNECT";
        case DeviceEvent::MQTT_START_STREAM:
            return "MQTT_START_STREAM";
        case DeviceEvent::MQTT_STOP_STREAM:
            return "MQTT_STOP_STREAM";
        case DeviceEvent::REBOOT:
            return "REBOOT";
        case DeviceEvent::ANY:
            return "ANY";
        default:
            return "UNKNOWN";
    }
}
