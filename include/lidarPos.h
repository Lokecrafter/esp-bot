# ifndef LIDARPOS_H
# define LIDARPOS_H

# include <freertos/FreeRTOS.h>
# include <freertos/task.h>
# include <driver/gpio.h>
# include <driver/adc.h>
# include <freertos/semphr.h>

#define PHOTO_PIN ADC1_CHANNEL_5
static TickType_t _last_passage_time = 0;
static uint32_t _average_lap_time = 0;
static SemaphoreHandle_t _encoder_mutex;

void update_lidar_pos();

float get_lidar_angle();

void lidarPos_task(void *pvParameters);
#endif