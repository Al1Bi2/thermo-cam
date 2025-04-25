#pragma once
#include <amg8833.h>
#include <json_generator.h>
#include <PubSubClient.h>
#include "cJSON.h"
#include "OV2640.h"
#include "fsm.h"
#include "log.h"

#define LASTWILL_MESSAGE "offline"
#define LASTWILL_QOS 1
extern OV2640 camera;

namespace mqtt{

#define evtADCreading      ( 1 << 3 )
EventGroupHandle_t eg; // variable for the event group handle

const int PAYLOAD_LEN = 1024;

AMG8833 amg8833;


const int AMG8833_FREQ = 100;


TaskHandle_t send_matrix_task;

String CONNECT_TOPIC = "/connect";
String SETTINGS_TOPIC = "/settings";
String CONTROL_TOPIC = "";
String SERVER_STATUS_TOPIC = "server/status";
String DEVICE_STATUS_TOPIC = "";
String DISCOVERY_TOPIC = "discovery";
String AMG8833_TOPIC = "";

struct struct_message{
    char payload [PAYLOAD_LEN] = {'\0'};
    String topic;
} incoming_message;

QueueHandle_t q_Message;
QueueHandle_t q_matrix;

SemaphoreHandle_t sem_mqqt_busy;

TaskHandle_t discovery_task_handle;

void lock_mqtt(){
    xSemaphoreTake(sem_mqqt_busy, portMAX_DELAY);
}

void unlock_mqtt(){
    xSemaphoreGive(sem_mqqt_busy);
}


void IRAM_ATTR mqtt_callback(char* topic, byte * payload, unsigned int length){

    incoming_message.topic = topic;
    int i = 0;
    for ( i; i < length; i++){
        incoming_message.payload[i] = ((char)payload[i]);
    }
    incoming_message.payload[i] = '\0';
    xQueueOverwrite( q_Message, (void *) &incoming_message );
    
}




void parse_json_and_set_settings(const char* message){
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_Parse(message), cJSON_Delete);
    if (!root) {
        Serial.println("Ошибка парсинга JSON");
        return;
    }

    // Проверяем, есть ли объект "camera"
    cJSON* camera_obj = cJSON_GetObjectItem(root.get(), "camera");
    if (camera_obj) {
        cJSON* item = camera_obj->child; // Получаем первый элемент объекта "camera"
        while (item) {
            if (cJSON_IsNumber(item)) { // Проверяем, что значение – число
                ::camera.set_settings(item->string, item->valueint);
            }
            item = item->next; // Переходим к следующему ключу
        }
    }

    // Аналогично для "amg8833"
    cJSON* amg8833_obj = cJSON_GetObjectItem(root.get(), "amg8833");
    if (amg8833_obj) {
        cJSON* item = amg8833_obj->child;
        while (item) {
            if (cJSON_IsNumber(item)) {
                amg8833.set_settings(item->string, item->valueint);
            }
            item = item->next;
        }
    }

}


void handle_message(void * parameters){
    Serial.println("Message handler started");
    struct struct_message new_message;
    while(true){
        if (q_Message == NULL) {
            Serial.println("Queue not initialized!");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        if ( xQueueReceive(q_Message, &new_message, portMAX_DELAY) == pdTRUE )
        {    
            Serial.print("Message received on topic: ");
            Serial.print(incoming_message.topic);
            Serial.print(". Message: ");
            Serial.println(incoming_message.payload);
            String msg_str = String(incoming_message.payload);
            if(incoming_message.topic == CONTROL_TOPIC){
                log_debug("CONTROL_TOPIC");
                if(strcmp(incoming_message.payload, "ack-connect")==0){
                    log_info("ACK_CONNECT");
                    fsm->post_event(DeviceEvent::MQTT_ACK_CONNECT);
                }
                if(strcmp(incoming_message.payload, "start")==0){
                    log_info("START");
                    fsm->post_event(DeviceEvent::MQTT_START_STREAM);
                }
                if(strcmp(incoming_message.payload,  "stop")==0){
                    log_info("STOP");
                    fsm->post_event(DeviceEvent::MQTT_STOP_STREAM);
                }

            }
            if(incoming_message.topic == SERVER_STATUS_TOPIC){
                if(strcmp(incoming_message.payload,  "offline")==0 ){
                    log_info("offline");
                    fsm->post_event(DeviceEvent::SERVER_OFFLINE);
                }
            }


            if(incoming_message.topic == SETTINGS_TOPIC){
                parse_json_and_set_settings(incoming_message.payload);
            }
        }
    }
}


void loop_mqtt(void * parameters) {
    Serial.println("MQTT loop started");

    xSemaphoreGive(sem_mqqt_busy);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(500); 
    while(true){
        xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY ); 
        fsm->device.mqtt_client->loop();
        xSemaphoreGive( sem_mqqt_busy );

        taskYIELD();
        xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil( &xLastWakeTime, xFrequency );
    }
}


void gett_matrix(void * parameters) {

    std::array<std::array<float, 8>, 8> matrix;// увеличивает потребление памяти, надо поискать в инете норм ли x3
    Serial.println("Matrix getter started");
    while(true){
        xEventGroupWaitBits( eg, evtADCreading, pdTRUE, pdFALSE, portMAX_DELAY );
        matrix = fsm->device.amg8833.get_matrix();
        xQueueOverwrite( q_matrix, (void *) &matrix );

    }
}


void send_matrix(void * parameters) {
    std::array<std::array<float, 8>, 8> r_matrix;
    uint64_t last_time = esp_timer_get_time();
    uint64_t sleep_time = AMG8833_FREQ*1000;

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> matrix_array(cJSON_CreateArray(), cJSON_Delete);
    log_debug("Send matrix started");
    while(true){

        if(esp_timer_get_time() - last_time >= sleep_time){
            
            last_time = esp_timer_get_time();
            xEventGroupSetBits( eg, evtADCreading );
        }
        if(xQueueReceive(q_matrix, &r_matrix, 0) == pdTRUE){
            
            xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
            fsm->device.mqtt_client->publish(AMG8833_TOPIC.c_str(), reinterpret_cast<const uint8_t*>(&r_matrix), sizeof(r_matrix));
            xSemaphoreGive( sem_mqqt_busy );

        }
            
        taskYIELD();
    }

}
void setup_amg8833() {

    fsm->device.amg8833.init(); //shiiiiiiet
    fsm->device.amg8833.reset();
    fsm->device.amg8833.set_framerate(FRAMERATE::FPS_10);
}
void update_topic_names(){
    lock_ctx();
    String id = fsm->device.id;
    unlock_ctx();
    SETTINGS_TOPIC = String(id+"/settings");
    CONTROL_TOPIC = id+"/control";
    DEVICE_STATUS_TOPIC = id+"/status";
    AMG8833_TOPIC = id+"/amg8833";
}
void get_ready() {
    vTaskDelete(discovery_task_handle);
    setup_amg8833();
    xTaskCreatePinnedToCore(gett_matrix, "gett_matrix", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(send_matrix, "send_matrix", 4096, NULL, 2, &send_matrix_task, 1);

    xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
    vTaskSuspend(send_matrix_task);
    xSemaphoreGive( sem_mqqt_busy );

}

void start_stream(){
    log_info("Start stream");

    log_info("semaphore taken");
    vTaskResume(send_matrix_task);
    log_info("Resumed task");
    lock_mqtt();
    fsm->device.mqtt_client->publish(DEVICE_STATUS_TOPIC.c_str(), "active");
    unlock_mqtt();
    log_info("Sent confirmation");


}
void stop_stream(){

    vTaskSuspend(send_matrix_task);
    lock_mqtt();
    fsm->device.mqtt_client->publish(DEVICE_STATUS_TOPIC.c_str(), "connected");
    unlock_mqtt();

}


void connectMqtt(){
    int count = 0;

    lock_ctx();
    fsm->device.mqtt_client = new PubSubClient(fsm->device.wifi);;

    
    log_debug("Setting MQTT server to: " + fsm->device.server_ip);
    log_debug(fsm->device.server_ip);
    unlock_ctx();   
    fsm->device.mqtt_client->setServer(fsm->device.server_ip.c_str(), 1883);
    fsm->device.mqtt_client->setKeepAlive(20);
    fsm->device.mqtt_client->setBufferSize(PAYLOAD_LEN);
    Serial.print("Attempting MQTT connection... - ");

    sem_mqqt_busy = xSemaphoreCreateMutex();
    update_topic_names();   
    while(!fsm->device.mqtt_client->connected()){
        lock_mqtt();
        fsm->device.mqtt_client->disconnect();
        unlock_mqtt();
        log_debug("Disconnected from MQTT");
        lock_mqtt();
        fsm->device.mqtt_client->connect("ESP32Client", "rmuser", "pass", DEVICE_STATUS_TOPIC.c_str(), LASTWILL_QOS, 0, LASTWILL_MESSAGE);
        unlock_mqtt();
        log_debug("tried connecting");
        Serial.println(count);
        vTaskDelay(1000/portTICK_PERIOD_MS); 
        if(count++ == 20){
            log_info("Restarting ESP32");
            fsm->post_event(DeviceEvent::MQTT_FAIL);
        }
    }

    fsm->device.mqtt_client->setCallback(mqtt_callback);

    q_Message = xQueueCreate( 1, sizeof(struct struct_message) );
    eg = xEventGroupCreate();
    q_matrix = xQueueCreate( 1, sizeof(std::array<std::array<float, 8>, 8>) );
    
    

    fsm->device.mqtt_client->subscribe(SETTINGS_TOPIC.c_str(),1);
    fsm->device.mqtt_client->subscribe(DEVICE_STATUS_TOPIC.c_str(),1);
    fsm->device.mqtt_client->subscribe(CONTROL_TOPIC.c_str(),1);
    fsm->device.mqtt_client->subscribe(SERVER_STATUS_TOPIC.c_str(),1);

    fsm->post_event(DeviceEvent::MQTT_OK);
}

void discover_loop(void * parameters){
    lock_ctx();
    String id = fsm->device.id;
    String ip = fsm->device.ip;
    unlock_ctx();
    String message = id+":"+ip;
    log_info(message);
    Serial.println("Discovery loop started");
    while(true){
        fsm->device.mqtt_client->publish(DISCOVERY_TOPIC.c_str(), message.c_str());
        vTaskDelay(5000/portTICK_PERIOD_MS); 
    }
}

void discoverServer(){

    xTaskCreatePinnedToCore(discover_loop, "discover_loop", 4096, NULL, 2, &discovery_task_handle, 1);
    


}


}