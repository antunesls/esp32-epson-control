#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define NVS_NAMESPACE "config"

typedef struct {
    char     wifi_ssid[64];
    char     wifi_pass[64];
    uint16_t udp_port;
    int      uart_tx_pin;
    int      uart_rx_pin;
    bool     provisioned;
} app_config_t;

esp_err_t nvs_storage_init(void);
esp_err_t nvs_storage_load(app_config_t *cfg);
esp_err_t nvs_storage_save(const app_config_t *cfg);
esp_err_t nvs_storage_clear(void);
