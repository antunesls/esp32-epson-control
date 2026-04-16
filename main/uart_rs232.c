#include "uart_rs232.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "uart_rs232";

#define RS232_UART_NUM    UART_NUM_1
#define RS232_BAUD        9600
#define RS232_BUF_SIZE    256
#define RS232_QUEUE_SIZE  10

static bool s_initialized = false;

esp_err_t uart_rs232_init(int tx_pin, int rx_pin)
{
    if (tx_pin < 0 || rx_pin < 0) {
        ESP_LOGE(TAG, "Invalid UART pins: tx=%d rx=%d", tx_pin, rx_pin);
        return ESP_ERR_INVALID_ARG;
    }

    uart_config_t cfg = {
        .baud_rate  = RS232_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(RS232_UART_NUM,
                                        RS232_BUF_SIZE * 2,
                                        RS232_BUF_SIZE * 2,
                                        RS232_QUEUE_SIZE, NULL, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(uart_param_config(RS232_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(RS232_UART_NUM, tx_pin, rx_pin,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    s_initialized = true;
    ESP_LOGI(TAG, "UART RS232 initialized: tx=%d rx=%d baud=%d", tx_pin, rx_pin, RS232_BAUD);
    return ESP_OK;
}

esp_err_t uart_rs232_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    esp_err_t ret = uart_driver_delete(RS232_UART_NUM);
    s_initialized = false;
    return ret;
}

esp_err_t uart_rs232_send(const uint8_t *data, size_t len)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    int written = uart_write_bytes(RS232_UART_NUM, (const char *)data, len);
    if (written < 0) return ESP_FAIL;
    ESP_LOGD(TAG, "Sent %d bytes", written);
    return ESP_OK;
}

int uart_rs232_receive(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    if (!s_initialized) return -1;
    return uart_read_bytes(RS232_UART_NUM, buf, max_len,
                           pdMS_TO_TICKS(timeout_ms));
}

bool uart_rs232_is_initialized(void)
{
    return s_initialized;
}
