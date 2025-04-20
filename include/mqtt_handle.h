#pragma once
#include <amg8833.h>
#include <json_generator.h>
#include <PubSubClient.h>
#include "cJSON.h"
#include "OV2640.h"
#include "fsm.h"

#define LASTWILL_MESSAGE "offline"
#define LASTWILL_QOS 1
extern OV2640 camera;
namespace mqtt{

#define evtADCreading      ( 1 << 3 )
EventGroupHandle_t eg; // variable for the event group handle



AMG8833 amg8833;


const int AMG8833_FREQ = 2000;


TaskHandle_t send_matrix_task;

String CONNECT_TOPIC = "/connect";
String SETTINGS_TOPIC = "/settings";
String CONTROL_TOPIC = "";
String SERVER_STATUS_TOPIC = "server/status";
String DEVICE_STATUS_TOPIC = "";
String DISCOVERY_TOPIC = "discovery";
String AMG8833_TOPIC = "";

struct struct_message{
    char payload [500] = {'\0'};
    String topic;
} incoming_message;

QueueHandle_t q_Message;
QueueHandle_t q_matrix;

SemaphoreHandle_t sem_mqqt_busy;




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
                    fsm->post_event(DeviceEvent::MQTT_ACK_CONNECT);
                }
                if(strcmp(incoming_message.payload, "start")==0){
                    fsm->post_event(DeviceEvent::MQTT_START_STREAM);
                }
                if(strcmp(incoming_message.payload,  "stop")==0){
                    fsm->post_event(DeviceEvent::MQTT_START_STREAM);
                }

            }
            if(incoming_message.topic == SERVER_STATUS_TOPIC){
                if(strcmp(incoming_message.payload,  "offline")==0 ){
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
    sem_mqqt_busy = xSemaphoreCreateBinary();
    xSemaphoreGive(sem_mqqt_busy);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(500); 
    while(true){
        xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY ); 
        fsm->device.mqtt_client.connected();
        fsm->device.mqtt_client.loop();
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
        matrix = amg8833.get_matrix();
        xQueueOverwrite( q_matrix, (void *) &matrix );

    }
}


void send_matrix(void * parameters) {
    std::array<std::array<float, 8>, 8> r_matrix;
    uint64_t last_time = esp_timer_get_time();
    uint64_t sleep_time = 100*1000;

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> matrix_array(cJSON_CreateArray(), cJSON_Delete);
    while(true){

        if(esp_timer_get_time() - last_time >= sleep_time){
            
            last_time = esp_timer_get_time();
            xEventGroupSetBits( eg, evtADCreading );
        }
        if(xQueueReceive(q_matrix, &r_matrix, 0) == pdTRUE){
            
            xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
            fsm->device.mqtt_client.publish(AMG8833_TOPIC.c_str(), reinterpret_cast<const uint8_t*>(&r_matrix), sizeof(r_matrix));
            xSemaphoreGive( sem_mqqt_busy );

        }
            
        taskYIELD();
    }

}
void setup_amg8833() {
    lock_ctx();
    fsm->device.amg8833 = AMG8833();
    unlock_ctx();
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
void setup_mqtt() {

    eg = xEventGroupCreate();
    q_matrix = xQueueCreate( 1, sizeof(std::array<std::array<float, 8>, 8>) );
    q_Message = xQueueCreate( 1, sizeof(struct struct_message) );

    update_topic_names();

    fsm->device.mqtt_client.setCallback(mqtt_callback);

    fsm->device.mqtt_client.subscribe(SETTINGS_TOPIC.c_str(),1);
    fsm->device.mqtt_client.subscribe(SERVER_STATUS_TOPIC.c_str(),1);
    fsm->device.mqtt_client.subscribe(DEVICE_STATUS_TOPIC.c_str(),1);
    fsm->device.mqtt_client.subscribe(CONTROL_TOPIC.c_str(),1);

    
    
    xTaskCreatePinnedToCore(gett_matrix, "gett_matrix", 4096, fsm, 3, NULL, 1);
    xTaskCreatePinnedToCore(send_matrix, "send_matrix", 4096, fsm, 2, &send_matrix_task, 1);

    xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
    vTaskSuspend(send_matrix_task);
    xSemaphoreGive( sem_mqqt_busy );

}

void start_stream(){
    xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
    vTaskResume(send_matrix_task);
    fsm->device.mqtt_client.publish(DEVICE_STATUS_TOPIC.c_str(), "active");
    xSemaphoreGive( sem_mqqt_busy );

}
void stop_stream(){
    xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
    vTaskSuspend(send_matrix_task);
    fsm->device.mqtt_client.publish(DEVICE_STATUS_TOPIC.c_str(), "connected");
    xSemaphoreGive( sem_mqqt_busy );
}


void connectMqtt(){
    int count = 0;
    lock_ctx();
    fsm->device.wifi = WiFiClient();
    fsm->device.mqtt_client = PubSubClient(fsm->device.wifi);
    unlock_ctx();
    fsm->device.mqtt_client.setServer(fsm->device.server_ip.c_str(), 1883);
    Serial.print("Attempting MQTT connection... - ");
    while(!fsm->device.mqtt_client.connected()){
        fsm->device.mqtt_client.disconnect();
        fsm->device.mqtt_client.connect("ESP32Client", "rmuser", "pass", DEVICE_STATUS_TOPIC.c_str(), LASTWILL_QOS, 0, LASTWILL_MESSAGE);
        Serial.println(count);
        vTaskDelay(1000/portTICK_PERIOD_MS); 
        if(count++ == 10){
            log_info("Restarting ESP32");
            fsm->post_event(DeviceEvent::MQTT_FAIL);
        }
    }

    fsm->post_event(DeviceEvent::MQTT_OK);
}

void discoverServer(){
    fsm->device.mqtt_client.subscribe(SERVER_STATUS_TOPIC.c_str());
    DeviceState state;
    lock_ctx();
    String id = fsm->device.id;
    String ip = fsm->device.ip;
    unlock_ctx();
    String message = id+":"+ip;
    do{
        fsm->device.mqtt_client.publish(DISCOVERY_TOPIC.c_str(), message.c_str());
        vTaskDelay(1000/portTICK_PERIOD_MS);
        lock_ctx();
        state = fsm->get_current_state();
        unlock_ctx();
    }while(state != DeviceState::REGISTERED);
}


}