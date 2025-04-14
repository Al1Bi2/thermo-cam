#pragma once
#include <amg8833.h>
#include <json_generator.h>
#include <PubSubClient.h>
#include "cJSON.h"
#include "OV2640.h"
#define LASTWILL_MESSAGE "offline"
#define LASTWILL_QOS 1
 


#define evtADCreading      ( 1 << 3 )
EventGroupHandle_t eg; // variable for the event group handle
String MQTTBrokerIP = "127.0.0.1";

WiFiClient      wifiClient;
PubSubClient    mqtt_client(wifiClient);
AMG8833 amg8833;
extern String unique_id;
extern OV2640 camera;
const int AMG8833_FREQ = 2000;

SemaphoreHandle_t mut_connected;

bool isConnected = false;
uint8_t notConnectedCount = 6;

TaskHandle_t send_matrix_task;

String CONNECT_TOPIC = String(unique_id+"/connect");
String SETTINGS_TOPIC = String(unique_id+"/settings");

struct struct_message{
    char payload [500] = {'\0'};
    String topic;
} incoming_message;

QueueHandle_t q_Message;
QueueHandle_t q_matrix;

SemaphoreHandle_t sem_mqqt_busy;
//SemaphoreHandle_t sem_mqqt_busy;



void IRAM_ATTR mqtt_callback(char* topic, byte * payload, unsigned int length){

    incoming_message.topic = topic;
    int i = 0;
    for ( i; i < length; i++){
        incoming_message.payload[i] = ((char)payload[i]);
    }
    incoming_message.payload[i] = '\0';
    xQueueOverwrite( q_Message, (void *) &incoming_message );
    
}

void connect_mqtt(){
    Serial.println("Connecting to MQTT...");
    mqtt_client.setBufferSize(1000);
    int count = 0;
    mqtt_client.setServer(MQTTBrokerIP.c_str(), 1883);
    String LASTWILL_TOPIC =  String(unique_id+"/status");
    while(!mqtt_client.connected()){
        mqtt_client.disconnect();
        mqtt_client.connect("ESP32Client", "rmuser", "pass", LASTWILL_TOPIC.c_str(), LASTWILL_QOS, 0, LASTWILL_MESSAGE);
        Serial.print("Attempting MQTT connection... - ");
        Serial.println(count);
        vTaskDelay(1000/portTICK_PERIOD_MS); 
        if(count==5){
            Serial.println("Restarting ESP32");
            ESP.restart();
        }
    }
    CONNECT_TOPIC = String(unique_id+"/connect");
    SETTINGS_TOPIC = String(unique_id+"/settings");
    mqtt_client.setCallback(mqtt_callback);
    Serial.println(mqtt_client.subscribe("server/test",1));
    Serial.println(mqtt_client.subscribe(CONNECT_TOPIC.c_str(),0));
    Serial.println(mqtt_client.subscribe(SETTINGS_TOPIC.c_str(),1));
    String message = unique_id+":"+WiFi.localIP().toString();
    Serial.println(mqtt_client.publish("discovery", message.c_str()));
    
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
                camera.set_settings(item->string, item->valueint);
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
            if(incoming_message.topic == CONNECT_TOPIC){
                xSemaphoreTake( mut_connected, portMAX_DELAY );
                notConnectedCount = 0;
                isConnected = true;
                vTaskResume(send_matrix_task);
                xSemaphoreGive( mut_connected );
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
        //if ( (wifiClient.connected()) && (WiFi.status() == WL_CONNECTED) )
       // {
        xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY ); 
        mqtt_client.connected();
        mqtt_client.loop();
        xSemaphoreGive( sem_mqqt_busy );
       /* }
        else {
            if ( !(wifiClient.connected()) || !(WiFi.status() == WL_CONNECTED) )
            {
                connectToWiFi(unique_id);
            }
            connect_mqtt();
        }*/
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
        Serial.println("Sending matrix");
        matrix = amg8833.get_matrix();
        xQueueOverwrite( q_matrix, (void *) &matrix );

    }
}

void check_connection(void * parameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);
    const int maxNoConnection = 5;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    int noConnection = 0;
    while(true){
        vTaskDelayUntil( &xLastWakeTime, xFrequency );
        xSemaphoreTake( mut_connected, portMAX_DELAY );
        notConnectedCount++;
        if(notConnectedCount >= maxNoConnection){
            isConnected = true;
            Serial.println("Server dont want to connect");
            vTaskSuspend( send_matrix_task );
        }
        xSemaphoreGive( mut_connected );
    }

}
void send_matrix(void * parameters) {
    std::array<std::array<float, 8>, 8> r_matrix;
    uint64_t last_time = esp_timer_get_time();
    uint64_t sleep_time = 200*1000;

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> matrix_array(cJSON_CreateArray(), cJSON_Delete);
    while(true){

        if(esp_timer_get_time() - last_time >= sleep_time){
            Serial.println("Send matrix");
            last_time = esp_timer_get_time();
            xEventGroupSetBits( eg, evtADCreading );
            Serial.println(xEventGroupGetBits( eg ));
        }
        if(xQueueReceive(q_matrix, &r_matrix, 0) == pdTRUE){
            uint32_t t1 = micros();
            cJSON *root = cJSON_CreateObject();
            cJSON *matrix_array = cJSON_CreateArray();
            //Serial.println("Recieved matrix");
            
            for(int i = 0; i < 8; i++){
                cJSON *row_array = cJSON_CreateArray();
                for(int j = 0; j < 8; j++){
                    cJSON_AddItemToArray(row_array, cJSON_CreateNumber(r_matrix[i][j]));
                }
                cJSON_AddItemToArray(matrix_array, row_array);
            }
            cJSON_AddItemToObject(root, "matrix", matrix_array);
            char *json_string = cJSON_PrintUnformatted(root);
            

            xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
            String topic = unique_id + "/amg8833";
            Serial.println(sizeof(r_matrix));      
            mqtt_client.publish(topic.c_str(), reinterpret_cast<const uint8_t*>(&r_matrix), sizeof(r_matrix));
            uint32_t t2 = micros();
            Serial.printf("JSON parse: %d μs\n", t2 - t1);
            xSemaphoreGive( sem_mqqt_busy );
            cJSON_Delete(root);
            free(json_string);  
        }
            
        taskYIELD();
    }

}
void setup_mqtt() {
    amg8833.init(); //shiiiiiiet
    amg8833.reset();
    amg8833.set_framerate(FRAMERATE::FPS_10);
    eg = xEventGroupCreate();
    q_matrix = xQueueCreate( 1, sizeof(std::array<std::array<float, 8>, 8>) );
    q_Message = xQueueCreate( 1, sizeof(struct struct_message) );
    mut_connected = xSemaphoreCreateMutex();
    Serial.printf("Size of matrix: %d\n",sizeof(std::array<std::array<float, 8>, 8>)); 
    
    xTaskCreatePinnedToCore(check_connection, "check_conn", 4906, NULL, 2, NULL, 1);

    xTaskCreatePinnedToCore(loop_mqtt, "loop_mqtt", 4906, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(handle_message, "handle_message", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(gett_matrix, "gett_matrix", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(send_matrix, "send_matrix", 4096, NULL, 2, &send_matrix_task, 1);

}


