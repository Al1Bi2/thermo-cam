#pragma once
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_attr.h"
#include <array>

#include <settings/camera_pins.h>

camera_config_t default_config{
  .pin_pwdn = PWDN_GPIO_NUM,
  .pin_reset = RESET_GPIO_NUM,
  .pin_xclk = XCLK_GPIO_NUM,
  .pin_sccb_sda = SIOD_GPIO_NUM,
  .pin_sccb_scl = SIOC_GPIO_NUM,
 
  .pin_d7 = Y9_GPIO_NUM,
  .pin_d6 = Y8_GPIO_NUM,
  .pin_d5 = Y7_GPIO_NUM,
  .pin_d4 = Y6_GPIO_NUM,
  .pin_d3 = Y5_GPIO_NUM,
  .pin_d2 = Y4_GPIO_NUM,
  .pin_d1 = Y3_GPIO_NUM,
  .pin_d0 = Y2_GPIO_NUM,
  
  .pin_vsync = VSYNC_GPIO_NUM,
  .pin_href = HREF_GPIO_NUM,
  .pin_pclk = PCLK_GPIO_NUM,

  .xclk_freq_hz = 20000000,


  .ledc_timer = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,

  .pixel_format = PIXFORMAT_JPEG,  // for streaming
  .frame_size = FRAMESIZE_VGA,

  .jpeg_quality = 12,
  .fb_count = 2,
  .fb_location = CAMERA_FB_IN_PSRAM,

  .grab_mode = CAMERA_GRAB_WHEN_EMPTY

};
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
private:
    camera_config_t config;
    camera_fb_t* fb;


};
