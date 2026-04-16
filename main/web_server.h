#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t web_server_start(bool ap_mode);
esp_err_t web_server_stop(void);
