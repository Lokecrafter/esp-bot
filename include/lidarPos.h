# ifndef LIDARPOS_H
# define LIDARPOS_H
#include <stdint.h>


#define PHOTO_PIN ADC1_CHANNEL_5

extern volatile uint32_t _last_passage_time;
extern volatile uint32_t _average_lap_time;

float get_lidar_angle();
void init_adc_monitor();

#endif