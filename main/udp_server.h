#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t udp_server_start(uint16_t port);
esp_err_t udp_server_stop(void);
