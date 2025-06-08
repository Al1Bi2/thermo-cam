#include "camera_handler.h"


volatile int cam_server::FPS = 25;
volatile size_t cam_server::frame_size;   
volatile uint8_t* cam_server::frame_buffer;    


void cam_server::handle_jpeg_stream_simple(){
    char buf[32];
    log_info("Connection to Web");
    lock_ctx();
    WiFiClient* client = new WiFiClient();
    *client = fsm->device.web_server->client();
    unlock_ctx();
    client->write(HEADER, hdrLen);
    while(true){
        if (!client->connected()) break;
        lock_ctx();
        fsm->device.camera.run();
        int size =  fsm->device.camera.get_fb_size();
        const uint8_t* buffer =  fsm->device.camera.get_buffer();
        unlock_ctx();

        client->printf("--%s\r\n", BOUNDARY);
        client->write(CTNTTYPE, cntLen);
        sprintf(buf, "%d\r\n\r\n", size);
        client->write(buf, strlen(buf));
        client->write(buffer, size);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}



void cam_server::loop_server(void* pvParameters){
    const TickType_t xFrequency = pdMS_TO_TICKS(10);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (true) {
        fsm->device.web_server->handleClient();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void cam_server::camera_setup(){
    if (fsm->device.camera.init() != ESP_OK) {
        Serial.println("Error initializing the camera");
        delay(1000);
        fsm->post_event(DeviceEvent::REBOOT);
    }
}


void cam_server::handleNotFound()
{
    String message = "Server is running!\n\n";
    message += "URI: ";
    message += fsm->device.web_server->uri();
    message += "\nMethod: ";
    message += (fsm->device.web_server->method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += fsm->device.web_server->args();
    message += "\n";
    fsm->device.web_server->send(200, "text / plain", message);
}

void cam_server::mjpeg_start(){
    //camera_setup();       
    log_info("Starting MJPEG server");


    const TickType_t xFrequency = pdMS_TO_TICKS(10);

    lock_ctx();
    if(fsm->device.web_server != nullptr){
        delete fsm->device.web_server;
    }
    fsm->device.web_server = new WebServer(80);
    unlock_ctx();
    
    fsm->device.web_server->on("/mjpeg/1", HTTP_GET, handle_jpeg_stream_simple);
    fsm->device.web_server->onNotFound(handleNotFound);
    fsm->device.web_server->begin();
    log_info("Server loop started");
    xTaskCreatePinnedToCore(cam_server::loop_server, "cam_server", 4906, NULL, 2, NULL, 1);
}

