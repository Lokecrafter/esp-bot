# pragma once
#include <stdio.h>

#define NEOPIXEL_GPIO GPIO_NUM_5
#define NUM_LEDS   24

void neopixel_init();

void set_leds(uint8_t* rgb_data);

void clear_leds();