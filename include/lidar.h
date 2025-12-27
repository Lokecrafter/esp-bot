# ifndef LIDAR_H
# define LIDAR_H
# include <stdint.h>
# include "freertos/FreeRTOS.h"
# include "freertos/queue.h"

# define LIDAR_PACKET_LENGTH 100
# define LIDAR_PACKET_QUEUE_MAX_AMOUNT 10

typedef struct __attribute__((packed)) {
    float angle;
    uint16_t distance;
} lidar_data_t;

QueueHandle_t lidar_packet_queue;

void init_lidar(uint32_t stack_size, uint8_t priority, uint8_t core_id);
# endif // LIDAR_H