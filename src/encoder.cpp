#include "Encoder.h"
#include "esp_log.h"

static const char *TAG = "encoder";

Encoder::Encoder(gpio_num_t pin_a, gpio_num_t pin_b, uint32_t _counts_per_rev) {
    counts_per_rev = _counts_per_rev;

    pcnt_unit_config_t unit_config = {};
    unit_config.high_limit = 32767;
    unit_config.low_limit = -32768;
    unit_config.intr_priority = 0;
    pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = { .edge_gpio_num = pin_a, .level_gpio_num = pin_b };
    pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    
    pcnt_chan_config_t chan_b_config = { .edge_gpio_num = pin_b, .level_gpio_num = pin_a };
    pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

Encoder::~Encoder() {
    pcnt_del_channel(pcnt_chan_a);
    pcnt_del_channel(pcnt_chan_b);
    pcnt_unit_disable(pcnt_unit);
    pcnt_del_unit(pcnt_unit);
}

int Encoder::get_count() {
    int pulse_count = 0;
    ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulse_count));
    return pulse_count;
}

void Encoder::clear_count() {
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
}

float Encoder::get_angle_deg() {
    int16_t normalized_modulo = ((get_count() % counts_per_rev) + counts_per_rev) % counts_per_rev;
    return normalized_modulo * 360.0f / (float)counts_per_rev;
}