#include "camera_handler.h"
OV2640 camera = OV2640();

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
        camera.run();
        int size = camera.get_fb_size();
        const uint8_t* buffer = camera.get_buffer();

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
    if (camera.init() != ESP_OK) {
        Serial.println("Error initializing the camera");
        delay(1000);
        fsm->post_event(DeviceEvent::REBOOT);
    }
}

uint8_t* cam_server::allocate_memory(uint8_t* requested_ptr, size_t requested_size){
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

void cam_server::mjpeg_start(void* pvParameters ){
    camera_setup();       
    log_info("Starting MJPEG server");


    const TickType_t xFrequency = pdMS_TO_TICKS(100);

    lock_ctx();
    if(fsm->device.web_server != nullptr){
        delete fsm->device.web_server;
    }
    fsm->device.web_server = new WebServer(80);
    unlock_ctx();
    
    fsm->device.web_server->on("/mjpeg/1", HTTP_GET, handle_jpeg_stream_simple);
    fsm->device.web_server->begin();
    log_info("Server loop started");
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(true) {
        //Serial.println("CH:Looping");
        fsm->device.web_server->handleClient();
        //taskYIELD();
        //vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }


}