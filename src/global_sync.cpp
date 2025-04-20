#include "global_sync.h"

SemaphoreHandle_t ctx_mutex = nullptr;

void init_ctx_mutex() {
  if (ctx_mutex == nullptr) {
    ctx_mutex = xSemaphoreCreateMutex();
  }
}