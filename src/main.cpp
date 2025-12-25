#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstdio>
#include "lidar.h"
#include "driver/gpio.h"

static const char *TAG = "MAIN";

extern "C" void app_main() {
    gpio_install_isr_service(0);

    init_lidar(2048, 5, 1);
}
