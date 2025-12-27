#include "communications.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "lidar.h"
#include "esp_websocket_client.h"




// Denna variabel används i din task
static esp_websocket_client_handle_t client;


void websocket_task(void *pvParameters) {
    lidar_data_t batch_buffer[LIDAR_PACKET_LENGTH];
    uint16_t count = 0;

    while(1) {
        if (xQueueReceive(lidar_packet_queue, &batch_buffer[count], portMAX_DELAY)) {
            count++;
            if (count >= LIDAR_PACKET_LENGTH) {
                // Skicka binärt via ESP-IDF WebSocket-klient/server
                // Vi castar vår struct till en uint8_t-pekare
                esp_websocket_client_send_bin(
                    client, 
                    (const char *)&batch_buffer, 
                    sizeof(lidar_data_t) * LIDAR_PACKET_LENGTH, 
                    portMAX_DELAY
                );
                count = 0;
            }
        }
    }
}

void websocket_app_start(void)
{
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = "ws://din-server-ip:port"; // Ändra till din server

    client = esp_websocket_client_init(&websocket_cfg);
    
    // Starta klienten
    esp_websocket_client_start(client);
    
    // Skapa din task här eller efter att anslutning bekräftats via events
    xTaskCreate(websocket_task, "ws_task", 4096, NULL, 5, NULL);
}