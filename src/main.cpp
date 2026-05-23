#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstdio>
#include "lidar.h"
#include "driver/gpio.h"
#include "communications.h"
#include "encoder.h"
#include "esp_timer.h"

static const char *TAG = "MAIN";


void test_task(void *pvParameters) {
    ESP_LOGI(TAG, "Test task started");

    int64_t start_time = esp_timer_get_time();
    int16_t last_count = 0;
    Encoder test_encoder = Encoder(GPIO_NUM_4, GPIO_NUM_13, 32*4);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_25),      // Select GPIO 2
        .mode = GPIO_MODE_INPUT,            // Set as output
        .pull_up_en = GPIO_PULLUP_DISABLE,  // Disable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Disable pull-down
        .intr_type = GPIO_INTR_DISABLE             // Disable interrupts
    };

    gpio_config(&io_conf);


    while (1) {

        float angle = test_encoder.get_angle_deg();
        float time_since_last = (float)(esp_timer_get_time() - start_time) / 1000000.0f;
        start_time = esp_timer_get_time();

        float speed = (test_encoder.get_count() - last_count) / time_since_last; // counts per second
        last_count = test_encoder.get_count();
        const char* fork = (gpio_get_level(GPIO_NUM_25) == 1) ? "yes" : "no";

        ESP_LOGI(TAG, "Angle: %f deg   Count: %d    Speed: %f   Fork: %s", angle, test_encoder.get_count(), speed, fork);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    // gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT);
    // gpio_set_pull_mode(GPIO_NUM_13, GPIO_FLOATING);

    // while (1) {
    //     if (gpio_get_level(GPIO_NUM_13) == 0) {
    //         ESP_LOGI(TAG, "GPIO 13 Interrupted");
    //         vTaskDelay(pdMS_TO_TICKS(100));
    //     }
    //     taskYIELD();
    // }
}



extern "C" void app_main() {

    // init_wifi();

    vTaskDelay(pdMS_TO_TICKS(1000));

    // init_lidar(4096, 5, 1);
    // websocket_init();

    
    
    
    xTaskCreatePinnedToCore(
        &test_task,
        "test_task",
        2048,
        NULL,
        0,
        NULL,
        0
    );
}
