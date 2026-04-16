#include "web_server.h"
#include "web_pages.h"
#include "epson_protocol.h"
#include "nvs_storage.h"
#include "wifi_manager.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG  = "web_server";
static httpd_handle_t s_server = NULL;

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */
#define RESP_JSON(req, code, json_str) \
    httpd_resp_set_type(req, "application/json"); \
    httpd_resp_set_status(req, code); \
    httpd_resp_sendstr(req, json_str)

static esp_err_t read_body(httpd_req_t *req, char *buf, size_t buf_len)
{
    int remaining = req->content_len;
    if (remaining <= 0 || (size_t)remaining >= buf_len) {
        buf[0] = '\0';
        return ESP_ERR_INVALID_SIZE;
    }
    int received = 0;
    while (remaining > 0) {
        int r = httpd_req_recv(req, buf + received, remaining);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received  += r;
        remaining -= r;
    }
    buf[received] = '\0';
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * GET /  (dashboard or setup page)
 * ----------------------------------------------------------------------- */
static esp_err_t handler_root_get(httpd_req_t *req)
{
    bool ap_mode = (bool)(uintptr_t)req->user_ctx;
    httpd_resp_set_type(req, "text/html");

    if (ap_mode) {
        httpd_resp_sendstr(req, HTML_SETUP);
    } else {
        char ip[16];
        wifi_manager_get_ip(ip, sizeof(ip));
        /* Send header, then substitute IP inline */
        httpd_resp_send_chunk(req, HTML_DASHBOARD_PRE, HTTPD_RESP_USE_STRLEN);
        httpd_resp_send_chunk(req, ip, HTTPD_RESP_USE_STRLEN);
        httpd_resp_send_chunk(req, HTML_DASHBOARD_POST, HTTPD_RESP_USE_STRLEN);
        httpd_resp_send_chunk(req, NULL, 0);
    }
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * POST /cmd  { "cmd": "PWR ON" }
 * ----------------------------------------------------------------------- */
static esp_err_t handler_cmd_post(httpd_req_t *req)
{
    char body[128];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        RESP_JSON(req, "400 Bad Request", "{\"status\":\"error\",\"message\":\"body too large\"}");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        RESP_JSON(req, "400 Bad Request", "{\"status\":\"error\",\"message\":\"invalid JSON\"}");
        return ESP_OK;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd_item)) {
        cJSON_Delete(root);
        RESP_JSON(req, "400 Bad Request", "{\"status\":\"error\",\"message\":\"missing cmd\"}");
        return ESP_OK;
    }

    const char *cmd_str = cmd_item->valuestring;
    esp_err_t   ret     = epson_send_command_str(cmd_str);
    cJSON_Delete(root);

    if (ret == ESP_OK) {
        RESP_JSON(req, "200 OK", "{\"status\":\"ok\"}");
    } else if (ret == ESP_ERR_NOT_FOUND) {
        RESP_JSON(req, "400 Bad Request", "{\"status\":\"error\",\"message\":\"unknown command\"}");
    } else if (ret == ESP_ERR_INVALID_STATE) {
        RESP_JSON(req, "503 Service Unavailable",
                  "{\"status\":\"error\",\"message\":\"UART not initialized — configure pins first\"}");
    } else {
        RESP_JSON(req, "500 Internal Server Error", "{\"status\":\"error\",\"message\":\"uart error\"}");
    }
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * GET /status
 * ----------------------------------------------------------------------- */
static esp_err_t handler_status_get(httpd_req_t *req)
{
    epson_status_t st = {0};
    epson_query_status(&st);

    char resp[128];
    snprintf(resp, sizeof(resp),
             "{\"pwr\":\"%s\",\"source\":\"%s\",\"lamp\":\"%s\"}",
             st.pwr[0]    ? st.pwr    : "N/A",
             st.source[0] ? st.source : "N/A",
             st.lamp[0]   ? st.lamp   : "N/A");

    RESP_JSON(req, "200 OK", resp);
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * GET /config
 * ----------------------------------------------------------------------- */
static esp_err_t handler_config_get(httpd_req_t *req)
{
    bool ap_mode = (bool)(uintptr_t)req->user_ctx;
    if (ap_mode) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req, HTML_SETUP);
        return ESP_OK;
    }

    app_config_t cfg = {0};
    nvs_storage_load(&cfg);

    char nums[64];
    httpd_resp_set_type(req, "text/html");
    /* Send config page with substituted values via chunks */
    httpd_resp_send_chunk(req, HTML_CONFIG_PRE, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, cfg.wifi_ssid, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_CONFIG_MID1, HTTPD_RESP_USE_STRLEN);
    snprintf(nums, sizeof(nums), "%d", cfg.uart_tx_pin);
    httpd_resp_send_chunk(req, nums, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_CONFIG_MID2, HTTPD_RESP_USE_STRLEN);
    snprintf(nums, sizeof(nums), "%d", cfg.uart_rx_pin);
    httpd_resp_send_chunk(req, nums, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_CONFIG_MID3, HTTPD_RESP_USE_STRLEN);
    snprintf(nums, sizeof(nums), "%d", cfg.udp_port);
    httpd_resp_send_chunk(req, nums, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_CONFIG_POST, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * POST /config  { ssid, pass, tx, rx, udp }
 * ----------------------------------------------------------------------- */
static esp_err_t handler_config_post(httpd_req_t *req)
{
    char body[256];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        RESP_JSON(req, "400 Bad Request", "{\"status\":\"error\",\"message\":\"body too large\"}");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        RESP_JSON(req, "400 Bad Request", "{\"status\":\"error\",\"message\":\"invalid JSON\"}");
        return ESP_OK;
    }

    app_config_t cfg = {0};
    nvs_storage_load(&cfg);   /* load existing to preserve unset fields */

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "ssid")) && cJSON_IsString(item))
        strlcpy(cfg.wifi_ssid, item->valuestring, sizeof(cfg.wifi_ssid));
    if ((item = cJSON_GetObjectItem(root, "pass")) && cJSON_IsString(item) && strlen(item->valuestring) > 0)
        strlcpy(cfg.wifi_pass, item->valuestring, sizeof(cfg.wifi_pass));
    if ((item = cJSON_GetObjectItem(root, "tx")) && cJSON_IsNumber(item))
        cfg.uart_tx_pin = (int)item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "rx")) && cJSON_IsNumber(item))
        cfg.uart_rx_pin = (int)item->valuedouble;
    if ((item = cJSON_GetObjectItem(root, "udp")) && cJSON_IsNumber(item))
        cfg.udp_port = (uint16_t)item->valuedouble;

    cfg.provisioned = (cfg.wifi_ssid[0] != '\0');

    cJSON_Delete(root);
    esp_err_t ret = nvs_storage_save(&cfg);

    if (ret == ESP_OK) {
        RESP_JSON(req, "200 OK", "{\"status\":\"ok\",\"message\":\"saved, rebooting\"}");
        vTaskDelay(pdMS_TO_TICKS(800));
        esp_restart();
    } else {
        RESP_JSON(req, "500 Internal Server Error", "{\"status\":\"error\",\"message\":\"nvs write failed\"}");
    }
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * GET /ota
 * ----------------------------------------------------------------------- */
static esp_err_t handler_ota_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, HTML_OTA);
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * POST /ota  — binary firmware upload
 * ----------------------------------------------------------------------- */
static esp_err_t handler_ota_post(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "No OTA partition found");
        return ESP_OK;
    }

    esp_err_t ret = esp_ota_begin(update_part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (ret != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, esp_err_to_name(ret));
        return ESP_OK;
    }

    char   buf[1024];
    int    remaining = req->content_len;
    bool   header_checked = false;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf,
                                     MIN((size_t)remaining, sizeof(buf)));
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (recv_len < 0) {
            esp_ota_abort(ota_handle);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_sendstr(req, "receive error");
            return ESP_OK;
        }

        /* Validate first chunk contains valid ESP image header */
        if (!header_checked && recv_len >= (int)sizeof(esp_image_header_t)) {
            esp_image_header_t *img = (esp_image_header_t *)buf;
            if (img->magic != ESP_IMAGE_HEADER_MAGIC) {
                esp_ota_abort(ota_handle);
                httpd_resp_set_status(req, "400 Bad Request");
                httpd_resp_sendstr(req, "invalid firmware file");
                return ESP_OK;
            }
            header_checked = true;
        }

        ret = esp_ota_write(ota_handle, buf, recv_len);
        if (ret != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_sendstr(req, esp_err_to_name(ret));
            return ESP_OK;
        }
        remaining -= recv_len;
        ESP_LOGD(TAG, "OTA: %d bytes remaining", remaining);
    }

    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, esp_err_to_name(ret));
        return ESP_OK;
    }

    ret = esp_ota_set_boot_partition(update_part);
    if (ret != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, esp_err_to_name(ret));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA complete, rebooting...");
    httpd_resp_sendstr(req, "OTA OK, rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * Start / stop
 * ----------------------------------------------------------------------- */
esp_err_t web_server_start(bool ap_mode)
{
    if (s_server) return ESP_OK;

    httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.stack_size       = 8192;

    ESP_ERROR_CHECK(httpd_start(&s_server, &config));

    void *ctx = (void *)(uintptr_t)ap_mode;

    httpd_uri_t root_get   = { .uri = "/",       .method = HTTP_GET,  .handler = handler_root_get,   .user_ctx = ctx };
    httpd_uri_t cmd_post   = { .uri = "/cmd",     .method = HTTP_POST, .handler = handler_cmd_post,   .user_ctx = ctx };
    httpd_uri_t status_get = { .uri = "/status",  .method = HTTP_GET,  .handler = handler_status_get, .user_ctx = ctx };
    httpd_uri_t cfg_get    = { .uri = "/config",  .method = HTTP_GET,  .handler = handler_config_get, .user_ctx = ctx };
    httpd_uri_t cfg_post   = { .uri = "/config",  .method = HTTP_POST, .handler = handler_config_post,.user_ctx = ctx };
    httpd_uri_t ota_get    = { .uri = "/ota",     .method = HTTP_GET,  .handler = handler_ota_get,    .user_ctx = ctx };
    httpd_uri_t ota_post   = { .uri = "/ota",     .method = HTTP_POST, .handler = handler_ota_post,   .user_ctx = ctx };

    httpd_register_uri_handler(s_server, &root_get);
    httpd_register_uri_handler(s_server, &cfg_get);
    httpd_register_uri_handler(s_server, &cfg_post);

    if (!ap_mode) {
        httpd_register_uri_handler(s_server, &cmd_post);
        httpd_register_uri_handler(s_server, &status_get);
        httpd_register_uri_handler(s_server, &ota_get);
        httpd_register_uri_handler(s_server, &ota_post);
    }

    ESP_LOGI(TAG, "Web server started (ap_mode=%d)", ap_mode);
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (!s_server) return ESP_OK;
    esp_err_t ret = httpd_stop(s_server);
    s_server = NULL;
    return ret;
}
