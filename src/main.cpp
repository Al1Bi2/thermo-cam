#include <Arduino.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>



#include "fsm_builder.h"
#include "fsm.h"
#include "actions.h"



String get_unique_id(){
    String mac_id = WiFi.macAddress();
    mac_id.replace(":","");
    String unique_id = "esp32-" + mac_id;
    return unique_id;
  }
  
  



constexpr std::array<Transition,11> createTransitions{
        TransitionBuilder().from(DeviceState::INIT).on(DeviceEvent::BOOT_STARTED)
            .to(DeviceState::WIFI_CONNECTING)
            .action(Actions::connectWifi)
            .build(),
        
        TransitionBuilder().from(DeviceState::WIFI_CONNECTING).on(DeviceEvent::WIFI_OK)
            .to(DeviceState::IP_RESOLVING)
            .action(Actions::resolveIP)
            .build(),

        TransitionBuilder().from(DeviceState::WIFI_CONNECTING).on(DeviceEvent::WIFI_FAIL)
            .to(DeviceState::CAPTIVE_PORTAL)
            .action(Actions::startCaptivePortal)
            .build(),
        
        TransitionBuilder().from(DeviceState::IP_RESOLVING).on(DeviceEvent::SERVER_FOUND)
            .to(DeviceState::MQTT_CONNECTING)
            .action(Actions::mqttConnect)
            .build(),

        TransitionBuilder().from(DeviceState::IP_RESOLVING).on(DeviceEvent::MDNS_FAIL)
            .to(DeviceState::INIT)
            .action(Actions::restart)
            .build(),
        
        TransitionBuilder().from(DeviceState::MQTT_CONNECTING).on(DeviceEvent::MQTT_OK)
            .to(DeviceState::SERVER_CONNECTING)
            .action(Actions::discoverServer)
            .build(),

        TransitionBuilder().from(DeviceState::MQTT_CONNECTING).on(DeviceEvent::MQTT_FAIL)
            .to(DeviceState::INIT)
            .action(Actions::restart)
            .build(),
                
        TransitionBuilder().from(DeviceState::SERVER_CONNECTING).on(DeviceEvent::MQTT_ACK_CONNECT)
            .to(DeviceState::REGISTERED)
            .action(Actions::getReady)
            .build(),
                
        TransitionBuilder().from(DeviceState::REGISTERED).on(DeviceEvent::MQTT_START_STREAM)
            .to(DeviceState::ACTIVE)
            .action(Actions::startCapture)
            .build(),

        TransitionBuilder().from(DeviceState::ACTIVE).on(DeviceEvent::MQTT_STOP_STREAM)
            .to(DeviceState::REGISTERED)
            .action(Actions::stopCapture)
            .build(),

        TransitionBuilder().from(DeviceState::ANY).on(DeviceEvent::REBOOT)
            .to(DeviceState::REBOOTING)
            .action(Actions::restart)
            .build()
       
};



void setup() {

    Serial.begin(115200);
    delay(1000);

    delay(1000);   
    
    ctx.server_ip = "";
    ctx.mqtt_client = nullptr;
    ctx.web_server = nullptr;

    fsm = new FSM<11>(ctx, createTransitions);
    lock_ctx();
    fsm->device.id = get_unique_id();

    unlock_ctx();
    Serial.println("Device ID: " + fsm->device.id);
    cam_server::camera_setup();
    mqtt::setup_amg8833();

    xTaskCreatePinnedToCore(fsm_rtos_task, "fsm_task", 4096, fsm, 2, &fsm_task_handle, 1);
    log_info("Init start");

    fsm->post_event(DeviceEvent::BOOT_STARTED);


}

void loop() {
    
    
}

