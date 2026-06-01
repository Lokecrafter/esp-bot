#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "neopixel.h"

static led_strip_handle_t led_strip;

void neopixel_init()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = NEOPIXEL_GPIO,                         
        .max_leds = NUM_LEDS,                                 
        .led_model = LED_MODEL_WS2812,                        
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, 
        .flags = {
            .invert_out = false,                              
        }
    };

    
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,                       
        .resolution_hz = 10000000,                            
        .mem_block_symbols = 0,                               
        .flags = {
            .with_dma = false,                                
        }
    };


    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

void set_leds(uint8_t* rgb_data){
    for (int i = 0; i < NUM_LEDS; i++) {
        led_strip_set_pixel(
            led_strip, 
            i, 
            rgb_data[3*i + 0], 
            rgb_data[3*i + 1], 
            rgb_data[3*i + 2]
        );
    }
    led_strip_refresh(led_strip);
}

void clear_leds(){
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}