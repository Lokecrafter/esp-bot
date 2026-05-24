# pragma once
# ifndef LIDAR_H
# define LIDAR_H
# include <stdint.h>
# include "freertos/FreeRTOS.h"
# include "freertos/queue.h"

# define LIDAR_PACKET_LENGTH 100
# define LIDAR_PACKET_QUEUE_MAX_AMOUNT 10
# define ENCODER_STEPS 128

// typedef struct __attribute__((packed)) {
//     float angle;
//     uint16_t distance;
// } lidar_data_t;
typedef struct {
    uint16_t distances[ENCODER_STEPS];
    uint8_t valid[ENCODER_STEPS]; // 1 = giltig mätning, 0 = tom/filtrerad
} lidar_scan_t;

// extern QueueHandle_t lidar_packet_queue;
extern QueueHandle_t lidar_scan_queue;

void init_lidar(uint32_t stack_size, uint8_t priority, uint8_t core_id);
# endif // LIDAR_H