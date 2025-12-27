#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstdio>
#include "lidar.h"
#include "tachometer.h"
#include "driver/gpio.h"

// static const char *TAG = "MAIN";


// void test_task(void *pvParameters) {
//     ESP_LOGI(TAG, "Test task started");
//     gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
//     gpio_set_pull_mode(GPIO_NUM_13, GPIO_FLOATING);

//     while (1) {
//         if (gpio_get_level(GPIO_NUM_13) == 0) {
//             ESP_LOGI(TAG, "GPIO 13 Interrupted");
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }
//         taskYIELD();
//     }
// }


extern "C" void app_main() {

    init_lidar(4096, 5, 1);
    init_tachometer(4096, 5, 1);
    // xTaskCreatePinnedToCore(
    //     &test_task,
    //     "test_task",
    //     2048,
    //     NULL,
    //     0,
    //     NULL,
    //     0
    // );
}
