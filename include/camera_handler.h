#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "esp_camera.h" 
#include "esp_log.h"
#include "esp_attr.h"

#include <driver/rtc_io.h>

#include <settings/camera_pins.h>

#include <ov2640.h>

const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                      "Access-Control-Allow-Origin: *\r\n" \
                      "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);

OV2640 camera;
volatile int FPS = 25;

volatile size_t frame_size;   
volatile uint8_t* frame_buffer;    

WebServer cam_server(80);

TaskHandle_t tMjpeg;   // handles client connections to the webserver
TaskHandle_t tCam;     // handles getting picture frames from the camera and storing them locally
TaskHandle_t tStream;  // actually streaming frames to all connected clients

SemaphoreHandle_t frame_busy = NULL;


QueueHandle_t streamingClients;

uint8_t* allocate_memory(uint8_t* requested_ptr, size_t requested_size);
void camera_setup(){
    if (camera.init() != ESP_OK) {
        Serial.println("Error initializing the camera");
        delay(1000);
        ESP.restart();
      }
}


void camera_task(void* pvParameters){
    camera_setup();
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS/10);
    uint8_t* fb_buffer[2];
    size_t fSize[2] = { 0, 0 };
    int fb_idx = 0;

    portMUX_TYPE xSemaphore = portMUX_INITIALIZER_UNLOCKED;

    xLastWakeTime = xTaskGetTickCount();
    Serial.println("Set up camera task");
    while(1){
        camera.run();
        size_t fb_size = camera.get_fb_size();
        if(fSize[fb_idx] < fb_size){
            fSize[fb_idx] = fb_size * 5 / 4;
            fb_buffer[fb_idx] = allocate_memory(fb_buffer[fb_idx], fSize[fb_idx]);
        }

        uint8_t* frame = camera.get_buffer();
        memcpy(fb_buffer[fb_idx],frame,fb_size);
        taskYIELD();

        //xTaskDelayUntil(&xLastWakeTime, xFrequency);

        //Serial.println("Camera, entering critical section");

        xSemaphoreTake(frame_busy, portMAX_DELAY);

        portENTER_CRITICAL(&xSemaphore);
        frame_buffer = fb_buffer[fb_idx];
        frame_size = fb_size;
        fb_idx++;
        fb_idx &= 1;  
        portEXIT_CRITICAL(&xSemaphore);

        xSemaphoreGive(frame_busy);
        
        xTaskNotifyGive( tStream );
        //Serial.print("CH: Camera Task: sent notify to mjpeg task");
        taskYIELD();

        //Serial.println("Camera, exited critical section");

        if ( eTaskGetState( tStream ) == eSuspended ) {
            Serial.println("Suspending camera task");
            vTaskSuspend(NULL);  
          }
    }
}

uint8_t* allocate_memory(uint8_t* requested_ptr, size_t requested_size){
    if(requested_ptr!=NULL){
        free(requested_ptr);
    }
    size_t free_heap = esp_get_free_heap_size();
    uint8_t* new_ptr = NULL;

    if( requested_size > free_heap*3/4){
        if(psramFound() && ESP.getFreePsram() > requested_size){
            new_ptr = (uint8_t*) ps_malloc(requested_size);
        }
    }else{
        new_ptr = (uint8_t*) malloc(requested_size);
    }

    if(new_ptr == NULL){
        ESP.restart();
    }
    return new_ptr;
}

void mjpeg_task(void* pvParameters){
    char buf[16];
    TickType_t xLastWakeTime;
    TickType_t xFrequency;
  
    Serial.println("CH:waiting for client connections");
    ulTaskNotifyTake( pdTRUE, portMAX_DELAY ); 
    xLastWakeTime = xTaskGetTickCount();
    Serial.println("Set up mjpeg task");
    while(true){
        xFrequency = pdMS_TO_TICKS(1000 / FPS);
        UBaseType_t active_clients = uxQueueMessagesWaiting(streamingClients);
        //Serial.print("CH: active clients: "); Serial.println(active_clients);
        if(active_clients){
            xFrequency /= active_clients;

            WiFiClient* client;
            xQueueReceive(streamingClients, static_cast<void*>(&client),0);
            if (!client->connected()) {
                delete client;
            }else{

                xSemaphoreTake(frame_busy, portMAX_DELAY);
                client->write(CTNTTYPE, cntLen);
                sprintf( buf, "%d\r\n\r\n", frame_size);
                client->write(buf, strlen(buf));
                client->write(const_cast<uint8_t*>(frame_buffer), frame_size);
                client->write(BOUNDARY, bdrLen);

                xQueueSend(streamingClients, (void *) &client, 0);
                
                xSemaphoreGive(frame_busy);
                taskYIELD();
            }

        }else{
            Serial.println("No active clients, suspending mjpeg task");
            vTaskSuspend(NULL);

        }
        taskYIELD();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
void handle_jpeg_stream(){
    Serial.println("CH: handle_jpeg_stream:new client");
    WiFiClient* client = new WiFiClient();
    *client = cam_server.client();

    client->write(HEADER, hdrLen);
    client->write(BOUNDARY, bdrLen);
    xQueueSend(streamingClients,(void *) &client,0);

    if(eTaskGetState(tCam) == eSuspended ){
        Serial.println("CH: handle_jpeg_stream:resuming camera task");
        vTaskResume(tCam);
    }
    if(eTaskGetState(tStream) == eSuspended ){
        Serial.println("CH: handle_jpeg_stream:resuming mjpeg task");
        vTaskResume(tStream);
    }
}

void handle_jpeg_stream_simple(){
    char buf[32];
    int size = 0;
    WiFiClient* client = new WiFiClient();
    *client = cam_server.client();

    client->write(HEADER, hdrLen);
    client->write(BOUNDARY, bdrLen);
    while (true)
    {  
        if(!client->connected()) break;
        camera.run();
        size = camera.get_fb_size();

        client->write(CTNTTYPE, cntLen);
        sprintf( buf, "%d\r\n\r\n", size);
        client->write(buf, strlen(buf));
        client->write(const_cast<uint8_t*>(camera.get_buffer()), size);
        client->write(BOUNDARY, bdrLen);
        delay(10);
        Serial.println("poop");
    }
    
}
void handleNotFound()
{
  String message = "Server is running!\n\n";
  message += "URI: ";
  message += cam_server.uri();
  message += "\nMethod: ";
  message += (cam_server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += cam_server.args();
  message += "\n";
  cam_server.send(200, "text / plain", message);
}
void streaming_task(void* pvParameters){
    camera_setup();
    Serial.println("Streaming task started");
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(100);

    frame_busy = xSemaphoreCreateBinary();
    xSemaphoreGive(frame_busy);

    streamingClients = xQueueCreate( 10, sizeof(WiFiClient*) );
    Serial.println("Starting camera task ...");
   /* xTaskCreatePinnedToCore(
        camera_task,        
        "camera_task",       
        4096,         
        NULL,         
        2,            
        &tCam,       
        1);     

        //  Creating task to push the stream to all connected clients
    Serial.println("Starting mjpeg task ...");
    xTaskCreatePinnedToCore(
        mjpeg_task,
        "mjpeg_task",
        4096,
        NULL, 
        3,
        &tStream,
        1);
 */

    //  Registering webcam_server handling routines
    cam_server.on("/mjpeg/1", HTTP_GET, handle_jpeg_stream_simple);
    cam_server.onNotFound(handleNotFound);


    cam_server.begin();

    xLastWakeTime = xTaskGetTickCount();
    Serial.println("GoingLoop");
    while(true) {
        //Serial.println("CH:Looping");
        cam_server.handleClient();
        //taskYIELD();
        //vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}