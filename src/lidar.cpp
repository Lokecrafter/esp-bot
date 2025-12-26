# include "lidar.h"
# include "esp_log.h"
# include "freertos/FreeRTOS.h"
# include "freertos/task.h"
# include "freertos/semphr.h"
# include "driver/gpio.h"
# include "driver/i2c_master.h"
# include <string>


# define LIDAR_INTERRUPT_PIN GPIO_NUM_13
# define TIMEOUT_MS 10

static const char *TAG = "LIDAR";

SemaphoreHandle_t sensor_sem = NULL;
i2c_master_bus_handle_t i2c_bus;
i2c_master_dev_handle_t lidar_dev;

void write_to_lidar_register(uint8_t reg, uint8_t value){
    uint8_t data[2] = {reg, value};
    i2c_master_transmit(lidar_dev, data, 2, TIMEOUT_MS);
}
uint8_t read_from_lidar_register(uint8_t reg){
    uint8_t buffer = 0;
    i2c_master_transmit_receive(lidar_dev, &reg, 1, &buffer, 1, TIMEOUT_MS);
    return buffer;
}
void start_distance_measurement(bool bias_correction){
    # define ACQ_COMMAND_REG 0x00
    if(bias_correction)     write_to_lidar_register(ACQ_COMMAND_REG, 0x04); // with receiver bias correction
    else                    write_to_lidar_register(ACQ_COMMAND_REG, 0x03); // without
}
uint16_t read_distance_measurement() {
    uint8_t reg = 0x8f; // 0x0f med bit 7 satt för auto-increment
    uint8_t data[2];

    i2c_master_transmit_receive(lidar_dev, &reg, 1, data, 2, TIMEOUT_MS);
    
    return (data[0] << 8) | data[1];
}
uint16_t start_and_read_distance_measurement(bool bias_correction){
    start_distance_measurement(bias_correction);
    vTaskDelay(pdMS_TO_TICKS(20));  // Wait for measurement to complete
    return read_distance_measurement();
}
uint8_t read_lidar_status(){
    # define STATUS_REG 0x01
    return read_from_lidar_register(STATUS_REG);
}

// Format an 8-bit value as a binary string and return it
static std::string format_binary8(uint8_t value) {
    std::string out(8, '0');
    for (int i = 0; i < 8; ++i) {
        out[7 - i] = (value & (1 << i)) ? '1' : '0';
    }
    return out;
}



// ISR for LIDAR measurement ready interrupt
static void IRAM_ATTR lidar_measurement_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(sensor_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
void sensor_read_task(void *pvParameters) {
    while (1) {
        start_distance_measurement(false);

        if (xSemaphoreTake(sensor_sem, pdMS_TO_TICKS(200)) == pdTRUE) {
            uint16_t distance = read_distance_measurement();
            ESP_LOGI(TAG, "%d cm", distance); 
        }
        else {
            uint16_t distance = read_distance_measurement();
            uint8_t status = read_lidar_status();
            ESP_LOGW(TAG, "Timeout. Distance: %d cm.   Status: %s   Monitor pin: %d", distance, format_binary8(status).c_str(), gpio_get_level(LIDAR_INTERRUPT_PIN));
        }
    }
}



void init_lidar(uint32_t stack_size, uint8_t priority, uint8_t core_id) {
    ESP_LOGI(TAG, "Initializing LIDAR...");

    // Configure I2C for LIDAR
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.scl_io_num = GPIO_NUM_22;
    bus_config.sda_io_num = GPIO_NUM_21;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x62,
        .scl_speed_hz = 400000, // 400kHz
        .scl_wait_us = 0,
        .flags = {}
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_config, &lidar_dev));
    
    // Reset LIDAR registers
    vTaskDelay(pdMS_TO_TICKS(100));
    write_to_lidar_register(0x00, 0x00);
    ESP_LOGI(TAG, "LIDAR SERIAL NUMBER: %d", (read_from_lidar_register(0x16) << 8) | read_from_lidar_register(0x17));
    vTaskDelay(pdMS_TO_TICKS(100));


    // Define LIDAR register addresses
    # define ACQ_CONFIG_REG 0x04
    # define MEASURE_DELAY_REG 0x45
    # define OUTER_LOOP_COUNT_REG 0x11
    # define REF_COUNT_VAL_REG 0x12
    # define THRESHOLD_BYPASS_REG 0x1c

    // Configure LIDAR registers
    uint8_t new_acq_config = 0;
    new_acq_config |= 0b01; // Status output mode
    // new_acq_config |= (1 << 5);
    write_to_lidar_register(ACQ_CONFIG_REG, new_acq_config);    ESP_LOGI(TAG, "ACQ Config set to %s", format_binary8(read_from_lidar_register(ACQ_CONFIG_REG)).c_str());
    // write_to_lidar_register(MEASURE_DELAY_REG, 0);              ESP_LOGI(TAG, "MEASURE_DELAY_REG set to %d", read_from_lidar_register(MEASURE_DELAY_REG));
    write_to_lidar_register(OUTER_LOOP_COUNT_REG, 0x00);        ESP_LOGI(TAG, "OUTER_LOOP_COUNT_REG set to %d", read_from_lidar_register(OUTER_LOOP_COUNT_REG));
    
    // Configure GPIO and ISR for LIDAR interrupts
    sensor_sem = xSemaphoreCreateBinary();
    gpio_install_isr_service(0);
    gpio_set_direction(LIDAR_INTERRUPT_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(LIDAR_INTERRUPT_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_pull_mode(LIDAR_INTERRUPT_PIN, GPIO_FLOATING);
    gpio_isr_handler_add(LIDAR_INTERRUPT_PIN, lidar_measurement_isr_handler, NULL);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Create task for reading after interrupts
    xTaskCreatePinnedToCore(
        &sensor_read_task,
        "sensor_read_task",
        stack_size,
        NULL,
        priority,
        NULL,
        core_id
    );

    ESP_LOGI(TAG, "init_lidar complete");
}