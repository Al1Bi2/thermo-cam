#pragma once
#include <amg8833.h>
#include <json_generator.h>
#include <PubSubClient.h>
#define MQTTBroker "192.168.0.5"

WiFiClient      wifiClient;
PubSubClient    mqtt_client(wifiClient);
AMG8833 amg8833;
std::array<std::array<float, 8>, 8> matrix;
int board_id = 11;
void connect_mqtt(){
    mqtt_client.setBufferSize(1000);
    int count = 0;
    mqtt_client.setServer(MQTTBroker, 1883);
    while(!mqtt_client.connected())
    {
        mqtt_client.disconnect();
        mqtt_client.connect("ESP32Client", "rmuser", "qwezxc113239");
        Serial.print("Attempting MQTT connection... - ");
        Serial.println(count);
        vTaskDelay(1000/portTICK_PERIOD_MS); 
        if(count==5){
            Serial.println("Restarting ESP32");
            ESP.restart();
        }

    }
}
void loopMQTT(void * parameters) {
    connect_mqtt();
    amg8833.init();
    amg8833.reset();
    delay(250);
    amg8833.set_framerate(FRAMERATE::FPS_10);
    Serial.println("Connected to MQTT Broker");
    Serial.println("Starting MQTT Loop");
    while(true){
        mqtt_client.loop();
        
        matrix = amg8833.get_matrix();

        String json = "{ \"board\":" + String(board_id) + ", \"matrix\": [";
        for(int i = 0; i < 8; i++){
            json+= "[";
            for(int j = 0; j < 8; j++){
                json += matrix[i][j];
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
        //Serial.println("Publishing");
        mqtt_client.publish("amg8833", json.c_str());
        //Serial.println(millis());
        vTaskDelay(2000/portTICK_PERIOD_MS);

    }
    vTaskDelete(NULL);
}