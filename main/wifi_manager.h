#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define WIFI_AP_SSID      "Epson-Control"
#define WIFI_AP_PASS      "12345678"
#define WIFI_AP_CHANNEL   1
#define WIFI_AP_MAX_CONN  4
#define WIFI_STA_RETRIES  10

typedef void (*wifi_connected_cb_t)(void);

esp_err_t wifi_manager_init_ap(void);
esp_err_t wifi_manager_init_sta(const char *ssid, const char *pass,
                                wifi_connected_cb_t on_connected);
bool      wifi_manager_is_connected(void);
void      wifi_manager_get_ip(char *buf, size_t len);
