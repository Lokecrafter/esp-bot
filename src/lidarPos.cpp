# include "lidarPos.h"
# include "esp_adc/adc_continuous.h"
# include "esp_timer.h"

# define PHOTO_RESISTOR_ANALOG_CHANNEL ADC_CHANNEL_5

adc_continuous_handle_t handle = NULL;

volatile uint32_t _last_passage_time = 0;
volatile uint32_t _average_lap_time = 0;


float get_lidar_angle() {
    uint32_t current_time = esp_timer_get_time();
    uint32_t time_since_passage = current_time - _last_passage_time;
    uint32_t average_lap_time = _average_lap_time;

    float angle_degrees = (time_since_passage/(float)average_lap_time) * 360;
    if (angle_degrees >= 360){
        angle_degrees -= 360;
    }

    return angle_degrees;
    // return -1;
}



static bool IRAM_ATTR lidar_pos_callback(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data) {
    adc_digi_output_data_t *raw_data = (adc_digi_output_data_t *)edata->conv_frame_buffer;
    uint32_t num_samples = edata->size / sizeof(adc_digi_output_data_t);
    uint32_t now = (uint32_t)esp_timer_get_time();

    for (int i = 0; i < num_samples; i++) {
        if (raw_data[i].type1.data > 3500) {
            // Debounce: Kontrollera att det gått minst t.ex. 50ms (50000 us)
            if (now - _last_passage_time > 50000) {
                uint32_t new_lap_time = now - _last_passage_time;
                _last_passage_time = now;
                
                // Medelvärdesberäkningen är nu också säker
                _average_lap_time = ((_average_lap_time * 9) + new_lap_time) / 10;
            }
            break; 
        }
    }
    return false;
}


void init_adc_monitor() {
    // Create ADC continuous handle
    adc_continuous_handle_cfg_t adc_initial_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = 256,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_initial_config, &handle));

    // Define channel to continuously read.
    adc_digi_pattern_config_t adc_pattern[1];
    adc_pattern[0].atten = ADC_ATTEN_DB_12;
    adc_pattern[0].channel = PHOTO_RESISTOR_ANALOG_CHANNEL;
    adc_pattern[0].unit = ADC_UNIT_1;
    adc_pattern[0].bit_width = ADC_BITWIDTH_12;
    adc_continuous_config_t adc_config = {
        .pattern_num = 1,
        .adc_pattern = adc_pattern,      // Pass array here
        .sample_freq_hz = 20 * 1000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    ESP_ERROR_CHECK(adc_continuous_config(handle, &adc_config));

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = lidar_pos_callback,
    };

    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));
}
