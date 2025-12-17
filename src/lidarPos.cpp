# include "lidarPos.h"
# include <Arduino.h>

void update_lidar_pos() {
    TickType_t current_time = xTaskGetTickCount();
    uint32_t new_lap_time = pdTICKS_TO_MS(current_time - _last_passage_time);

    if (xSemaphoreTake(_encoder_mutex, portMAX_DELAY)) {
        _last_passage_time = xTaskGetTickCount();
        
        _average_lap_time = (uint32_t)((_average_lap_time * 0.9) + (new_lap_time * 0.1)); 
        
        xSemaphoreGive(_encoder_mutex);
    }
    Serial.print("New lap time");
    Serial.println(new_lap_time);
}

float get_lidar_angle() {
    TickType_t current_time = xTaskGetTickCount();

    if (xSemaphoreTake(_encoder_mutex, portMAX_DELAY)) {
        uint32_t time_since_passage = pdTICKS_TO_MS(current_time - _last_passage_time);
        uint32_t average_lap_time = _average_lap_time;
        xSemaphoreGive(_encoder_mutex);

        float angle_degrees = (time_since_passage/(float)average_lap_time) * 360;
        if (angle_degrees >= 360){
            angle_degrees -= 360;
        }

        return angle_degrees;
    }
    return -1;
}

void lidarPos_task(void *pvParameters) {
    gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);

    while(1){
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(PHOTO_PIN, ADC_ATTEN_DB_12);
        uint16_t value = adc1_get_raw(PHOTO_PIN);
        
        if (value > 3500){
            gpio_set_level(GPIO_NUM_18, 1);
            update_lidar_pos();
        }
        else{
            gpio_set_level(GPIO_NUM_18, 0);
        }        
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

