#pragma once
#include <amg8833.h>
#include <json_generator.h>
#include <PubSubClient.h>
#define evtADCreading      ( 1 << 3 )
EventGroupHandle_t eg; // variable for the event group handle
String MQTTBrokerIP = "192.168.0.5";

WiFiClient      wifiClient;
PubSubClient    mqtt_client(wifiClient);
AMG8833 amg8833;
extern String unique_id;

const int AMG8833_FREQ = 2000;

struct struct_message{
    char payload [300] = {'\0'};
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
    while(!mqtt_client.connected()){
        mqtt_client.disconnect();
        mqtt_client.connect("ESP32Client", "rmuser", "pass");
        Serial.print("Attempting MQTT connection... - ");
        Serial.println(count);
        vTaskDelay(1000/portTICK_PERIOD_MS); 
        if(count==5){
            Serial.println("Restarting ESP32");
            ESP.restart();
        }
    }
    mqtt_client.setCallback(mqtt_callback);
    mqtt_client.subscribe("server/test");
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
        }
    }
}

void loop_mqtt(void * parameters) {
    Serial.println("MQTT loop started");
    sem_mqqt_busy = xSemaphoreCreateBinary();
    xSemaphoreGive(sem_mqqt_busy);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = 500; 
    while(true){
        //if ( (wifiClient.connected()) && (WiFi.status() == WL_CONNECTED) )
       // {
        xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY ); 
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
void loop_matrix(void * parameters) {
    uint64_t last_time = esp_timer_get_time();
    uint64_t sleep_time = 2000*1000;
    std::array<std::array<float, 8>, 8> matrix;// увеличивает потребление памяти, надо поискать в инете норм ли
    Serial.println("Matrix loop started");
    float mm=0;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = AMG8833_FREQ; 

    portMUX_TYPE xSemaphore = portMUX_INITIALIZER_UNLOCKED;
    while(true){
        if(esp_timer_get_time() - last_time >= sleep_time){
            last_time = esp_timer_get_time();
            xEventGroupSetBits( eg, evtADCreading );
        }
        //xQueueReceive(xQ_RM, &RemainingMoisture, 0 );
        
        xQueueOverwrite( q_matrix, (void *) &matrix );

        xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil( &xLastWakeTime, xFrequency );
    }
}
void send_matrix(void * parameters) {
    std::array<std::array<float, 8>, 8> r_matrix;// увеличивает потребление памяти, надо поискать в инете норм ли — x2
    uint64_t last_time = esp_timer_get_time();
    uint64_t sleep_time = 2000*1000;
    while(true){
        if(esp_timer_get_time() - last_time >= sleep_time){
            Serial.println("Send matrix");
            last_time = esp_timer_get_time();
            xEventGroupSetBits( eg, evtADCreading );
            Serial.println(xEventGroupGetBits( eg ));
        }
        if(xQueueReceive(q_matrix, &r_matrix, 0) == pdTRUE){
            Serial.println("Recieved matrix");
            String json = "{ \"board\":" + unique_id + ", \"matrix\": [";
            
            for(int i = 0; i < 8; i++){
                json+= "[";
                for(int j = 0; j < 8; j++){
                    json += r_matrix[i][j];
                    if(j != 7){
                        json += ",";
                    }
                }
                json += "]";
                if(i != 7){
                    json += ",";                
                }
            }
            json += "]}";
            Serial.println(json);
            xSemaphoreTake( sem_mqqt_busy, portMAX_DELAY );
            mqtt_client.publish("amg8833", json.c_str());
            xSemaphoreGive( sem_mqqt_busy );
        }
            
                
    }

}
void setup_mqtt() {
    amg8833.init();
    amg8833.reset();
    amg8833.set_framerate(FRAMERATE::FPS_1);
    eg = xEventGroupCreate();
    q_matrix = xQueueCreate( 1, sizeof(std::array<std::array<float, 8>, 8>) );
    q_Message = xQueueCreate( 1, sizeof(struct struct_message) );
   
    Serial.printf("Size of matrix: %d\n",sizeof(std::array<std::array<float, 8>, 8>));
    xTaskCreatePinnedToCore(loop_mqtt, "loop_mqtt", 4906, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(handle_message, "handle_message", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(gett_matrix, "gett_matrix", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(send_matrix, "send_matrix", 4096, NULL, 2, NULL, 1);

}