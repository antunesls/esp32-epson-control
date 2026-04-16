#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t uart_rs232_init(int tx_pin, int rx_pin);
esp_err_t uart_rs232_deinit(void);
esp_err_t uart_rs232_send(const uint8_t *data, size_t len);
int       uart_rs232_receive(uint8_t *buf, size_t max_len, uint32_t timeout_ms);
bool      uart_rs232_is_initialized(void);
