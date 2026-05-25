#include "communications.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "lidar.h"
#include "esp_websocket_client.h"
#include "esp_wifi.h"

#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define SSID          "sapo_overvakningscentral_2"
#define PASSWORD      "sapo_overvakning_123"
#define WEBSOCKET_SERVER_URL "ws://192.168.0.222:8765"

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "COMMUNICATIONS";
static esp_websocket_client_handle_t client = NULL;
static volatile bool is_ws_connected = false;

// WiFi Event Handler - Helt utan blockerande vTaskDelay!
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        is_ws_connected = false;
        ESP_LOGW(TAG, "WiFi bortkopplat! Försöker ansluta igen direkt...");
        esp_wifi_connect(); // ESP-IDF hanterar back-off internt
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi Anslutet! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// WebSocket Event Handler
static void websocket_event_handler(void *handler_args, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket ansluten till Python-servern!");
            is_ws_connected = true;
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket bortkopplad från Python-servern!");
            is_ws_connected = false;
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket-fel uppstod.");
            is_ws_connected = false;
            break;
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, SSID);
    strcpy((char*)wifi_config.sta.password, PASSWORD);
    // Låt scannern själv förhandla authmode (betydligt säkrare och mer kompatibelt)
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN; 

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Väntar på WiFi-anslutning...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}

void init_wifi(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init_sta();
}

void websocket_task(void *pvParameters) {
    lidar_scan_t* scan_to_send = NULL;
    uint32_t dropped_scans = 0;

    while(1) {
        if (xQueueReceive(lidar_scan_queue, &scan_to_send, portMAX_DELAY)) {
            
            if (!is_ws_connected || client == NULL) {
                dropped_scans++;
                if (dropped_scans % 30 == 0) {
                    ESP_LOGW(TAG, "Python-server ej nåbar via WebSocket. Kastat %lu varv.", dropped_scans);
                }
                xQueueReset(lidar_scan_queue); 
                vTaskDelay(pdMS_TO_TICKS(5)); 
                continue;
            }

            dropped_scans = 0;

            int res = esp_websocket_client_send_bin(
                client, 
                (const char *)scan_to_send, 
                sizeof(lidar_scan_t), 
                pdMS_TO_TICKS(30)
            );

            if (res < 0) {
                ESP_LOGE(TAG, "Kunde inte skicka binärpaketet över nätverket.");
            }
        }
    }
}

void websocket_init()
{
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = WEBSOCKET_SERVER_URL;
    
    // Auto-reconnect parametrar till ESP-IDF stacken
    websocket_cfg.reconnect_timeout_ms = 1000;
    websocket_cfg.network_timeout_ms = 4000;

    client = esp_websocket_client_init(&websocket_cfg);

    ESP_ERROR_CHECK(esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL));

    esp_websocket_client_start(client);

    xQueueReset(lidar_scan_queue); 
    xTaskCreate(websocket_task, "ws_task", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "WebSocket startad.");
}