# include <stdio.h>
# include <stdlib.h>
# include <freertos/FreeRTOS.h>
# include <freertos/task.h>
# include <Arduino.h>
# include <Wire.h>
# include <driver/gpio.h>
# include <driver/adc.h>
# include <FastLED.h>
# include "lidarPos.h"

// Task-funktion som använder Arduino-funktioner
void arduino_task_example(void *pvParameters) {
    # define LED GPIO_NUM_18

    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    
    while (1) {

        // digitalWrite(LED, HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // gpio_set_level(LED, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }  
}

void fast_led_task(void *pvParameters) {
    #define NUM_LEDS 24
    #define DATA_PIN 5
    #define DELAY_VAL 500


    CRGB leds[NUM_LEDS];
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    // pinMode(A5, INPUT);

    float percentage = 0;

    while(1){
        FastLED.clear();

        for (int i = 0; i < ceilf(percentage * NUM_LEDS); i++)
        {
            leds[i].setRGB(50, 0, 0);
        }
        FastLED.show();

        vTaskDelay(pdMS_TO_TICKS(DELAY_VAL));
        percentage += 0.1;
        if (percentage > 1){
            percentage -= 1;
        }
    }
}

extern "C" void app_main() {
    initArduino();
    Serial.begin(115200);
    Serial.println("Hello");

    xTaskCreatePinnedToCore(
        lidarPos_task,
        "lidarPos_task",       // Task-name
        4096,                  // Stacksize
        NULL,                  // Parameter
        2,                     // Priority
        NULL,                  // Task-handle
        0                      // Core: 0 eller 1
    );
    xTaskCreatePinnedToCore(
        fast_led_task,
        "fast_led_task",       // Task-name
        4096,                  // Stacksize
        NULL,                  // Parameter
        2,                     // Priority
        NULL,                  // Task-handle
        1                      // Core: 0 eller 1
    );
}