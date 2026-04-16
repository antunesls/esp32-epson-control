#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "nvs_storage";

esp_err_t nvs_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erase required, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t nvs_storage_load(app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    /* defaults */
    memset(cfg, 0, sizeof(*cfg));
    cfg->udp_port    = 4210;
    cfg->uart_tx_pin = 5;   /* GPIO5 = TX on ESP32-S3-Zero */
    cfg->uart_rx_pin = 6;   /* GPIO6 = RX on ESP32-S3-Zero */
    cfg->provisioned = false;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No config namespace found, using defaults");
        return ESP_OK;
    }
    if (ret != ESP_OK) return ret;

    size_t len;

    len = sizeof(cfg->wifi_ssid);
    nvs_get_str(h, "wifi_ssid", cfg->wifi_ssid, &len);

    len = sizeof(cfg->wifi_pass);
    nvs_get_str(h, "wifi_pass", cfg->wifi_pass, &len);

    uint16_t udp_port = 4210;
    if (nvs_get_u16(h, "udp_port", &udp_port) == ESP_OK)
        cfg->udp_port = udp_port;

    int32_t tx = -1, rx = -1;
    if (nvs_get_i32(h, "uart_tx_pin", &tx) == ESP_OK) cfg->uart_tx_pin = (int)tx;
    if (nvs_get_i32(h, "uart_rx_pin", &rx) == ESP_OK) cfg->uart_rx_pin = (int)rx;

    uint8_t prov = 0;
    if (nvs_get_u8(h, "provisioned", &prov) == ESP_OK) cfg->provisioned = (bool)prov;

    nvs_close(h);
    ESP_LOGI(TAG, "Config loaded: ssid=%s provisioned=%d udp=%d tx=%d rx=%d",
             cfg->wifi_ssid, cfg->provisioned, cfg->udp_port,
             cfg->uart_tx_pin, cfg->uart_rx_pin);
    return ESP_OK;
}

esp_err_t nvs_storage_save(const app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_str(h, "wifi_ssid", cfg->wifi_ssid);
    nvs_set_str(h, "wifi_pass", cfg->wifi_pass);
    nvs_set_u16(h, "udp_port",  cfg->udp_port);
    nvs_set_i32(h, "uart_tx_pin", (int32_t)cfg->uart_tx_pin);
    nvs_set_i32(h, "uart_rx_pin", (int32_t)cfg->uart_rx_pin);
    nvs_set_u8 (h, "provisioned", cfg->provisioned ? 1 : 0);

    ret = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Config saved");
    return ret;
}

esp_err_t nvs_storage_clear(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_erase_all(h);
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}
