#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t ctx_mutex;

void init_ctx_mutex();


static inline void lock_ctx() {
    xSemaphoreTake(ctx_mutex, portMAX_DELAY);
  }
  
  static inline void unlock_ctx() {
    xSemaphoreGive(ctx_mutex);
  }