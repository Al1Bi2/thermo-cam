#pragma once
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_attr.h"
#include <array>
#include <cstring>
#include <settings/camera_pins.h>

extern camera_config_t default_config;
class OV2640{
public: 
  OV2640(){
    fb = NULL;
    config = default_config;
  }
  esp_err_t init(){
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        printf("Camera probe failed with error 0x%x", err);
        return err;
    }
    load_config();
    return ESP_OK;
  }
  void run(){
    if(fb){
      esp_camera_fb_return(fb);
    }
    fb = esp_camera_fb_get();
  }
  void run_with_check(){
    if(!fb){
      run();
    }
  }

  size_t get_fb_size(){
    run_with_check();
    return fb->len;
  }
  std::array<uint8_t,2> get_size(){
    std::array<uint8_t,2> size;
    size[0] = fb->width;
    size[1] = fb->height;
    return size;
  }
  uint8_t get_height(){
    run_with_check();
    return static_cast<uint8_t>(fb->height);
  }

  uint8_t get_width(){
    run_with_check();
    return static_cast<uint8_t>(fb->width);
  }

  uint8_t* get_buffer(){
    run_with_check();
    return fb->buf;
  }
  void free_buffer(){
    esp_camera_fb_return(fb);
  }
  void save_config(){
    esp_camera_save_to_nvs("OV2640");
  }
  void load_config(){
    esp_camera_load_from_nvs("OV2640");
  }
  framesize_t get_frame_size(){
    return config.frame_size;
  }
  void set_frame_size(framesize_t frame_size){
    config.frame_size = frame_size;
  }
  void set_settings(const char* key,int value){
    sensor_t* s = esp_camera_sensor_get();
    if(strcmp(key,"quality") == 0){
      s->set_quality(s, value);
    }
    if(strcmp(key,"framesize") == 0){
      s->set_framesize(s, static_cast<framesize_t>(value));
    }
    if(strcmp(key,"brightness") == 0){
      s->set_brightness(s, value);
    }

    save_config();
  }
private:
    camera_config_t config;
    camera_fb_t* fb;


};
