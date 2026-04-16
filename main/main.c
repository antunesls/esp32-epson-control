#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "nvs_storage.h"
#include "wifi_manager.h"
#include "uart_rs232.h"
#include "web_server.h"
#include "udp_server.h"

static const char *TAG = "main";

static app_config_t s_cfg;

/* Called by wifi_manager when STA gets an IP */
static void on_wifi_connected(void)
{
    char ip[16];
    wifi_manager_get_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "WiFi connected, IP: %s", ip);

    /* Start full web server (dashboard + OTA + config) */
    web_server_start(false);

    /* Start UDP server */
    udp_server_start(s_cfg.udp_port);

    /* Init UART RS232 if pins are configured */
    if (s_cfg.uart_tx_pin >= 0 && s_cfg.uart_rx_pin >= 0) {
        esp_err_t ret = uart_rs232_init(s_cfg.uart_tx_pin, s_cfg.uart_rx_pin);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "UART RS232 init failed: %s — configure pins via web interface",
                     esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "UART pins not configured — RS232 disabled. Open http://%s/config to set up.", ip);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Epson Projector Controller ===");

    /* 1. NVS */
    ESP_ERROR_CHECK(nvs_storage_init());
    ESP_ERROR_CHECK(nvs_storage_load(&s_cfg));

    if (!s_cfg.provisioned) {
        /* ---- FIRST BOOT: AP mode ---- */
        ESP_LOGI(TAG, "Not provisioned — starting AP mode");
        ESP_ERROR_CHECK(wifi_manager_init_ap());
        /* Start web server with only setup/config routes */
        ESP_ERROR_CHECK(web_server_start(true));
        ESP_LOGI(TAG, "Connect to WiFi SSID '%s' and open http://192.168.4.1", WIFI_AP_SSID);
    } else {
        /* ---- NORMAL BOOT: STA mode ---- */
        ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", s_cfg.wifi_ssid);
        esp_err_t ret = wifi_manager_init_sta(s_cfg.wifi_ssid, s_cfg.wifi_pass,
                                              on_wifi_connected);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "WiFi connection failed — falling back to AP mode");
            /* Fallback: start AP so user can reconfigure */
            wifi_manager_init_ap();
            web_server_start(true);
        }
    }
}
