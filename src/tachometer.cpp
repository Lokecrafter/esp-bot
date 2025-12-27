# include "tachometer.h"
# include "freertos/FreeRTOS.h"
# include "freertos/task.h"
# include "freertos/semphr.h"
# include "esp_adc/adc_continuous.h"
# include "esp_log.h"
# include "freertos/queue.h"
# include "esp_timer.h"

static const char *TAG = "TACHOMETER";

# define TACHOMETER_THRESHOLD 2000
# define DEBOUNCE_TIME_MICROSECONDS 80000  // 80 ms
# define ADC_MAX_AMOUNT_FRAMES 10
# define ADC_FRAME_SIZE 1024
# define ADC_BUFFER_SIZE (ADC_FRAME_SIZE * ADC_MAX_AMOUNT_FRAMES)
# define ADC_SAMPLE_FREQ_HZ 20000


adc_continuous_handle_t handle = NULL;
SemaphoreHandle_t tachometer_sem = NULL;

static bool IRAM_ATTR on_conv_done(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(tachometer_sem, &high_task_wakeup); // Wake up task on measurement done
    return high_task_wakeup == pdTRUE;
}




uint32_t tachometer_last_pulse_time = 0;
uint32_t tachometer_interval_micro_seconds = -1;

void tachometer_task(void *pvParameters) {
    uint8_t result[ADC_FRAME_SIZE] = {0}; 
    uint32_t return_length = 0;

    while (1) {
        // Vänta på semafor
        if (xSemaphoreTake(tachometer_sem, portMAX_DELAY) == pdTRUE) {
            uint32_t finish_time = esp_timer_get_time();


            esp_err_t ret = adc_continuous_read(handle, result, ADC_FRAME_SIZE, &return_length, 100);
            if (ret != ESP_OK || return_length == 0) continue;

            int num_samples = return_length / SOC_ADC_DIGI_RESULT_BYTES;
            
            for (int i = 0; i < num_samples; i++) {
                uint16_t val = *(uint16_t*)&result[i * SOC_ADC_DIGI_RESULT_BYTES];
                val &= 0x0FFF;  // Mask away the metadata bits, keep only the 12-bit ADC value

                if (val > TACHOMETER_THRESHOLD) {
                    uint32_t pulse_time = finish_time - (num_samples - 1 - i) * (1000000 / ADC_SAMPLE_FREQ_HZ);
                    if (pulse_time - tachometer_last_pulse_time > DEBOUNCE_TIME_MICROSECONDS) {
                        uint32_t diff = (pulse_time - tachometer_last_pulse_time);
                        tachometer_interval_micro_seconds = (3 * tachometer_interval_micro_seconds + diff) / 4; // Averaging with weight 90% old, 10% new
                        tachometer_last_pulse_time = pulse_time;
                        
                        ESP_LOGI(TAG, "PULSE DETECTED!   Time: %lu ms Val: %lu Laptime: %lu ms   Finish frame time: %lu   Num_samples: %d", pulse_time/1000, val, tachometer_interval_micro_seconds/1000, finish_time, num_samples);
                    }
                }
            }
        }
    }
}

float tachometer_get_angle_degrees(uint32_t time_micro_seconds) {
    if (tachometer_interval_micro_seconds == (uint32_t)-1) {
        return 0.0f; // No valid measurement yet
    }
    float angle = 360.0f * (float)time_micro_seconds / tachometer_interval_micro_seconds;
    return angle;
}

void init_tachometer(uint32_t stack_size, uint8_t priority, uint8_t core_id) {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = ADC_BUFFER_SIZE,
        .conv_frame_size = ADC_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t config = {};
    config.sample_freq_hz = ADC_SAMPLE_FREQ_HZ;
    config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    config.pattern_num = 1;

    adc_digi_pattern_config_t adc_pattern = {};
    adc_pattern.atten = ADC_ATTEN_DB_12;
    adc_pattern.channel = ADC_CHANNEL_5;
    adc_pattern.unit = ADC_UNIT_1;
    adc_pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    ESP_LOGI(TAG, "adc_pattern.atten is :%"PRIx8, adc_pattern.atten);
    ESP_LOGI(TAG, "adc_pattern.channel is :%"PRIx8, adc_pattern.channel);
    ESP_LOGI(TAG, "adc_pattern.unit is :%"PRIx8, adc_pattern.unit);
    ESP_LOGI(TAG, "adc_pattern.bit_width is :%"PRIx8, adc_pattern.bit_width);
    config.adc_pattern = &adc_pattern;
    config.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    
    ESP_ERROR_CHECK(adc_continuous_config(handle, &config));

    adc_continuous_evt_cbs_t evt_cbs = {};
    evt_cbs.on_conv_done = on_conv_done;
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &evt_cbs, NULL));

    // tachometer_sem = xSemaphoreCreateBinary();
    tachometer_sem = xSemaphoreCreateCounting(ADC_MAX_AMOUNT_FRAMES, 0);
    xTaskCreatePinnedToCore(
        &tachometer_task,
        "tachometer_task",
        stack_size,
        NULL,
        priority,
        NULL,
        core_id
    );

    adc_continuous_start(handle);
}

