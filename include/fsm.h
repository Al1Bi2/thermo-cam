#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include "log.h"
#include <map>
#include "global_sync.h"
#include "amg8833.h"
#include <Preferences.h>

enum class DeviceState{
    INIT,
    WIFI_CONNECTING,
    CAPTIVE_PORTAL,
    IP_RESOLVING,
    MQTT_CONNECTING,
    SERVER_CONNECTING,
    REGISTERED,
    ACTIVE,
    ANY
};


enum class DeviceEvent{
    START_CONNECTING_WIFI,
    WIFI_OK,
    WIFI_FAIL,
    SERVER_FOUND,
    MDNS_FAIL,
    SERVER_NOT_FOUND,
    MQTT_OK,
    MQTT_FAIL,
    SERVER_OFFLINE,
    MQTT_ACK_CONNECT,
    MQTT_START_STREAM,
    MQTT_STOP_STREAM,
    REBOOT,
    ANY
};
const char* State2Str(DeviceState state);
const char* Event2Str(DeviceEvent event);
struct DeviceContext{

    String id;
    String ip;
    String server_ip;
    String mqtt_host;
    WiFiClient wifi;
    PubSubClient* mqtt_client = nullptr;
    WebServer* web_server = nullptr;
    TaskHandle_t send_matrix_task;
    AMG8833 amg8833;
    bool streaming = false;
};

struct Transition {
  DeviceState from;
  DeviceEvent event;
  DeviceState to;
  void (*action)();
};





template<int N>
class FSM{
  public:
    FSM(DeviceContext& device, const std::array<Transition,N>& transitions) : device(device), transitions(transitions),current_state(DeviceState::INIT) {
        event_queue = xQueueCreate(10, sizeof(DeviceEvent)); // Очередь на 10 событий
        if (event_queue == NULL) {
            log_error("Error creating event queue");
            while(1); // Повесить систему, если очередь не создана
        }
        init_ctx_mutex();
        Serial.println("Successfully created FSM!");
        if(&device == nullptr) {
            Serial.println("FATAL: Invalid device reference!");
            while(1);
        }
    }
    
    
    void post_event(DeviceEvent event) {
      xQueueSend(event_queue, &event, 0);
    }

    void process_events() {
      DeviceEvent event;

      while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        log_debug("FSM: Received event: " + String(Event2Str(event)));


        lock_ctx();

        Serial.println("FSM: Current state: " + String(State2Str(current_state)));
        Serial.println("FSM: New event: " + String(Event2Str(event)));
        auto it = std::find_if(transitions.begin(), transitions.end(),
        [&](const Transition& t) {
            return (t.from == current_state && t.event == event) || 
            (t.from == DeviceState::ANY && t.event == event) || (t.from == current_state && t.event == DeviceEvent::ANY);
        });

        if(it == transitions.end()) {
            log_debug("FSM: No transition found");
            unlock_ctx();
            continue;
        }

        DeviceState new_state = it->to;
        current_state = new_state;
        Serial.println("FSM: New state: " + String(State2Str(current_state)));
        unlock_ctx();
        
        it->action();
      }
    }

    DeviceState get_current_state() {
        return current_state;
    }

    public:
        DeviceContext device;
        Preferences preferences;
    private:

        std::array<Transition,N> transitions;
        QueueHandle_t event_queue;
        DeviceState current_state;


};

extern TaskHandle_t fsm_task_handle;
void fsm_rtos_task(void* params);

template<int NUM_TRANSITIONS>
void fsm_task(FSM<NUM_TRANSITIONS>& fsm) {
  while (true) {
    fsm.process_events();        // Обрабатываем события
    vTaskDelay(pdMS_TO_TICKS(10)); // Лёгкий делей
  }
}
template<size_t NUM_TRANSITIONS>
void fsm_task_wrapper(void* params) {
    auto* fsm = static_cast<FSM<NUM_TRANSITIONS>*>(params);
    fsm_task(*fsm);
}

extern DeviceContext ctx;
extern FSM<15>* fsm;