#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "esp_camera.h" 
#include "esp_log.h"
#include "esp_attr.h"
#include "log.h"
#include <driver/rtc_io.h>
#include "fsm.h"

#include <settings/camera_pins.h>

#include <ov2640.h>

const char HEADER[] = "HTTP/1.1 200 OK\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Content-Type: multipart/x-mixed-replace; boundary=frameframe\r\n\r\n";

const char BOUNDARY[] = "frameframe";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);

extern OV2640 camera;
namespace cam_server{
    
    extern volatile int FPS;
    extern volatile size_t frame_size;   
    extern volatile uint8_t* frame_buffer;    

    uint8_t* allocate_memory(uint8_t* requested_ptr, size_t requested_size);

    void camera_setup();

    void handle_jpeg_stream_simple();

    void handleNotFound();

    void loop_server(void* pvParameters); 


    void mjpeg_start(void* pvParameters );

}