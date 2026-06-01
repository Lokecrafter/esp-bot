#include "lidar.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include <string>
#include "esp_timer.h"
#include "encoder.h"
#include <math.h>
#include <string.h> // För memset

#define LIDAR_INTERRUPT_PIN GPIO_NUM_32
#define TIMEOUT_MS 10
#define LIDAR_ENCODER_PIN_A GPIO_NUM_36
#define LIDAR_ENCODER_PIN_B GPIO_NUM_39
#define LIDAR_INDEX_PIN GPIO_NUM_35

static const char *TAG = "LIDAR";

// Köer och semaforer
QueueHandle_t lidar_scan_queue = NULL;
SemaphoreHandle_t sensor_sem = NULL;

// Dubbelbuffring för varvdata
static lidar_scan_t scan_buffer_A;
static lidar_scan_t scan_buffer_B;
static lidar_scan_t* current_write_buffer = &scan_buffer_A;

// I2C-hantering och globala variabler
i2c_master_bus_handle_t i2c_bus;
i2c_master_dev_handle_t lidar_dev;
uint8_t measurement_counter = 0;

Encoder lidar_encoder = Encoder(LIDAR_ENCODER_PIN_A, LIDAR_ENCODER_PIN_B, 32*4);

// -------------------------------------------------------------------------
// FORWARD DECLARATIONS (Utan IRAM_ATTR för att undvika sektionskonflikter)
// -------------------------------------------------------------------------
void write_to_lidar_register(uint8_t reg, uint8_t value);
uint8_t read_from_lidar_register(uint8_t reg);
void start_distance_measurement(bool bias_correction);
uint16_t read_distance_measurement();
static std::string format_binary8(uint8_t value);
void lidar_index_isr_handler(void* arg);
void sensor_read_task(void *pvParameters);


// -------------------------------------------------------------------------
// ISR för mätgaffeln (Index) med inbyggd debounce och säker dubbelbuffring
// -------------------------------------------------------------------------
void IRAM_ATTR lidar_index_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    int64_t current_time = esp_timer_get_time();
    static int64_t last_index_time = 0;
    
    // DEBOUNCE: Om tornet snurrar i t.ex. 5-10 Hz (200-100ms per varv), 
    // kan index-avbrottet inte ske oftare än var 30:e millisekund (30000 us).
    if (current_time - last_index_time < 30000) {
        return; // Ignorera falsk trigger/elektrisk studs i mätgaffeln
    }
    last_index_time = current_time;

    // Spara undan pekaren till den buffert som vi JUST NU har fyllt klart
    lidar_scan_t* finished_buffer = current_write_buffer;

    // Skifta direkt till den ANDRA bufferten så att read_task kan fortsätta skriva direkt
    if (current_write_buffer == &scan_buffer_A) {
        current_write_buffer = &scan_buffer_B;
    } else {
        current_write_buffer = &scan_buffer_A;
    }
    
    // Nollställ den NYA aktiva skrivbuffertens flaggor inför det nya varvet
    for(int i = 0; i < ENCODER_STEPS; i++) {
        current_write_buffer->valid[i] = 0; 
    }
    
    // Skicka det färdiga varvets adress till nätverkskön
    xQueueSendFromISR(lidar_scan_queue, &finished_buffer, &xHigherPriorityTaskWoken);
    
    // Återställ hårdvaru-encodern via dess publika metod
    lidar_encoder.clear_count();

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// -------------------------------------------------------------------------
// Task för att läsa sensorn i realtid
// -------------------------------------------------------------------------
void sensor_read_task(void *pvParameters) {
    // Nollställ båda buffertarna från allra första start
    memset(&scan_buffer_A, 0, sizeof(lidar_scan_t));
    memset(&scan_buffer_B, 0, sizeof(lidar_scan_t));

    while (1) {
        if (measurement_counter >= LIDAR_PACKET_LENGTH) measurement_counter = 0;
        start_distance_measurement(measurement_counter == 0);
        measurement_counter++;
        
        esp_rom_delay_us(50); 
        
        if (xSemaphoreTake(sensor_sem, pdMS_TO_TICKS(200)) == pdTRUE) {
            uint16_t current_distance = read_distance_measurement();
            
            float current_angle = std::fmod(90.0f + 360.0f - lidar_encoder.get_angle_deg(), 360.0f);
            
            // Filtrera bort interna robotreflektioner och brus
            if (current_distance <= 12) {
                continue; 
            }

            int angle_index = (int)(current_angle / (360.0f / ENCODER_STEPS)) % ENCODER_STEPS;
            if (angle_index < 0) angle_index += ENCODER_STEPS;
            // ESP_LOGI(TAG, "Läste avstånd: %d cm, Vinkel-index: %d", current_distance, angle_index);
            // Skriv direkt till den aktuella säkra bufferten
            if (current_write_buffer->valid[angle_index] == 0) {
                current_write_buffer->distances[angle_index] = current_distance;
                current_write_buffer->valid[angle_index] = 1;
            } else {
                // Rullande medelvärde om vi läser samma index igen under samma varv
                uint16_t old_dist = current_write_buffer->distances[angle_index];
                current_write_buffer->distances[angle_index] = (old_dist + current_distance) / 2;
            }
        } else {
            read_distance_measurement(); 
        }
    }
}

// -------------------------------------------------------------------------
// Lidar hårdvaru-hjälpfunktioner
// -------------------------------------------------------------------------
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
    xSemaphoreTake(sensor_sem, 0);
    # define ACQ_COMMAND_REG 0x00
    if(bias_correction)     write_to_lidar_register(ACQ_COMMAND_REG, 0x04); // med kalibrering
    else                    write_to_lidar_register(ACQ_COMMAND_REG, 0x03); // utan
}

uint16_t read_distance_measurement() {
    uint8_t reg = 0x8f; 
    uint8_t data[2];
    i2c_master_transmit_receive(lidar_dev, &reg, 1, data, 2, TIMEOUT_MS);
    return (data[0] << 8) | data[1];
}

uint16_t start_and_read_distance_measurement(bool bias_correction){
    start_distance_measurement(bias_correction);
    vTaskDelay(pdMS_TO_TICKS(20));  
    return read_distance_measurement();
}

uint8_t read_lidar_status(){
    # define STATUS_REG 0x01
    return read_from_lidar_register(STATUS_REG);
}

static std::string format_binary8(uint8_t value) {
    std::string out(8, '0');
    for (int i = 0; i < 8; ++i) {
        out[7 - i] = (value & (1 << i)) ? '1' : '0';
    }
    return out;
}

static void IRAM_ATTR lidar_measurement_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sensor_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// -------------------------------------------------------------------------
// Initialisering
// -------------------------------------------------------------------------
void force_i2c_bus_recovery(gpio_num_t scl_pin, gpio_num_t sda_pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << scl_pin) | (1ULL << sda_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, // Open-drain är viktigt för I2C
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Om SDA hålls låg av sensorn, pulsa SCL för att frigöra den
    for (int i = 0; i < 9; i++) {
        gpio_set_level(scl_pin, 0);
        esp_rom_delay_us(5);
        gpio_set_level(scl_pin, 1);
        esp_rom_delay_us(5);
    }
    
    // Ge bussen ett kort ögonblick att stabiliseras
    esp_rom_delay_us(100);
    ESP_LOGI("I2C_RECOVERY", "I2C-bussen har återställts manuellt.");
}
void init_lidar(uint32_t stack_size, uint8_t priority, uint8_t core_id) {
    ESP_LOGI(TAG, "Initializing LIDAR...");

    force_i2c_bus_recovery(GPIO_NUM_22, GPIO_NUM_21); // SCL, SDA

    // Skapa kön för hela varv-skanningar
    lidar_scan_queue = xQueueCreate(4, sizeof(lidar_scan_t*)); 

    // Konfigurera I2C
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
        .scl_speed_hz = 400000, 
        .scl_wait_us = 0,
        .flags = {}
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_config, &lidar_dev));
    
    vTaskDelay(pdMS_TO_TICKS(100));
    write_to_lidar_register(0x00, 0x00);
    ESP_LOGI(TAG, "LIDAR SERIAL NUMBER: %d", (read_from_lidar_register(0x16) << 8) | read_from_lidar_register(0x17));
    vTaskDelay(pdMS_TO_TICKS(100));

    # define ACQ_CONFIG_REG 0x04
    write_to_lidar_register(ACQ_CONFIG_REG, 0b01);    
    
    // Registrera avbrott för datamätning
    sensor_sem = xSemaphoreCreateBinary();
    gpio_install_isr_service(0);
    gpio_set_direction(LIDAR_INTERRUPT_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(LIDAR_INTERRUPT_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_pull_mode(LIDAR_INTERRUPT_PIN, GPIO_FLOATING);
    gpio_isr_handler_add(LIDAR_INTERRUPT_PIN, lidar_measurement_isr_handler, NULL);

    // Registrera avbrott för mätgaffeln (Index)
    gpio_set_direction(LIDAR_INDEX_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(LIDAR_INDEX_PIN, GPIO_INTR_NEGEDGE);
    gpio_set_pull_mode(LIDAR_INDEX_PIN, GPIO_FLOATING);
    gpio_isr_handler_add(LIDAR_INDEX_PIN, lidar_index_isr_handler, NULL);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Starta mättasken
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