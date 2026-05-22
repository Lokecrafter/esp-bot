#pragma once

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

class Encoder {
private:
    pcnt_unit_handle_t pcnt_unit;
    pcnt_channel_handle_t pcnt_chan_a;
    pcnt_channel_handle_t pcnt_chan_b;
    uint32_t counts_per_rev;

public:
    Encoder(gpio_num_t pin_a, gpio_num_t pin_b, uint32_t counts_per_rev);
    ~Encoder();

    int get_count();
    void clear_count(); // Vi lägger till denna så externa klasser kan nollställa!
    float get_angle_deg();
};