#include "epson_protocol.h"
#include "uart_rs232.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "epson";

/* ESC/VP21 uses CR (\r) as terminator */
#define CR  "\r"

typedef struct {
    epson_cmd_id_t id;
    const char    *name;
    const char    *frame;   /* complete command string sent over RS232 */
} epson_cmd_entry_t;

static const epson_cmd_entry_t CMD_TABLE[EPSON_CMD_COUNT] = {
    { EPSON_CMD_PWR_ON,       "PWR ON",      "PWR ON"      CR },
    { EPSON_CMD_PWR_OFF,      "PWR OFF",     "PWR OFF"     CR },
    { EPSON_CMD_SOURCE_HDMI1, "SOURCE HDMI1","SOURCE 30"   CR },
    { EPSON_CMD_SOURCE_HDMI2, "SOURCE HDMI2","SOURCE A0"   CR },
    { EPSON_CMD_SOURCE_VGA,   "SOURCE VGA",  "SOURCE 11"   CR },
    { EPSON_CMD_SOURCE_COMP,  "SOURCE COMP", "SOURCE 14"   CR },
    { EPSON_CMD_VOL_UP,       "VOL+",        "VOL INC"     CR },
    { EPSON_CMD_VOL_DOWN,     "VOL-",        "VOL DEC"     CR },
    { EPSON_CMD_MUTE_ON,      "MUTE ON",     "MUTE ON"     CR },
    { EPSON_CMD_MUTE_OFF,     "MUTE OFF",    "MUTE OFF"    CR },
    { EPSON_CMD_BLANK_ON,     "BLANK ON",    "MSEL ON"     CR },
    { EPSON_CMD_BLANK_OFF,    "BLANK OFF",   "MSEL OFF"    CR },
    { EPSON_CMD_QUERY_PWR,    "PWR?",        "PWR?"        CR },
    { EPSON_CMD_QUERY_SOURCE, "SOURCE?",     "SOURCE?"     CR },
    { EPSON_CMD_QUERY_LAMP,   "LAMP?",       "LAMP?"       CR },
};

/* Send raw bytes and wait for response */
static esp_err_t send_and_wait(const char *frame, char *resp_buf, size_t resp_len)
{
    if (!uart_rs232_is_initialized()) {
        ESP_LOGE(TAG, "UART not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Flush RX before sending */
    uart_rs232_receive((uint8_t *)resp_buf, resp_len, 10);

    esp_err_t ret = uart_rs232_send((const uint8_t *)frame, strlen(frame));
    if (ret != ESP_OK) return ret;

    if (resp_buf && resp_len > 0) {
        int rx = uart_rs232_receive((uint8_t *)resp_buf, resp_len - 1u, 2000);
        if (rx > 0) {
            resp_buf[rx] = '\0';
            ESP_LOGD(TAG, "Response (%d bytes): %s", rx, resp_buf);
        } else {
            resp_buf[0] = '\0';
        }
    }
    return ESP_OK;
}

esp_err_t epson_send_command(epson_cmd_id_t cmd_id)
{
    if (cmd_id < 0 || cmd_id >= EPSON_CMD_COUNT) return ESP_ERR_INVALID_ARG;
    const epson_cmd_entry_t *e = &CMD_TABLE[cmd_id];
    ESP_LOGI(TAG, "Sending: %s", e->name);
    char resp[64];
    return send_and_wait(e->frame, resp, sizeof(resp));
}

epson_cmd_id_t epson_cmd_from_str(const char *str)
{
    if (!str) return EPSON_CMD_UNKNOWN;
    for (int i = 0; i < EPSON_CMD_COUNT; i++) {
        if (strcasecmp(str, CMD_TABLE[i].name) == 0)
            return CMD_TABLE[i].id;
    }
    return EPSON_CMD_UNKNOWN;
}

esp_err_t epson_send_command_str(const char *cmd_str)
{
    epson_cmd_id_t id = epson_cmd_from_str(cmd_str);
    if (id == EPSON_CMD_UNKNOWN) {
        ESP_LOGW(TAG, "Unknown command string: %s", cmd_str);
        return ESP_ERR_NOT_FOUND;
    }
    return epson_send_command(id);
}

esp_err_t epson_query_status(epson_status_t *status)
{
    if (!status) return ESP_ERR_INVALID_ARG;
    memset(status, 0, sizeof(*status));

    char resp[64];

    send_and_wait(CMD_TABLE[EPSON_CMD_QUERY_PWR].frame,    resp, sizeof(resp));
    /* Response format: "PWR=01\r:" or similar */
    char *p = strchr(resp, '=');
    if (p) strlcpy(status->pwr, p + 1, sizeof(status->pwr));
    /* strip trailing CR/: */
    for (char *c = status->pwr; *c; c++) {
        if (*c == '\r' || *c == ':') { *c = '\0'; break; }
    }

    send_and_wait(CMD_TABLE[EPSON_CMD_QUERY_SOURCE].frame, resp, sizeof(resp));
    p = strchr(resp, '=');
    if (p) strlcpy(status->source, p + 1, sizeof(status->source));
    for (char *c = status->source; *c; c++) {
        if (*c == '\r' || *c == ':') { *c = '\0'; break; }
    }

    send_and_wait(CMD_TABLE[EPSON_CMD_QUERY_LAMP].frame,   resp, sizeof(resp));
    p = strchr(resp, '=');
    if (p) strlcpy(status->lamp, p + 1, sizeof(status->lamp));
    for (char *c = status->lamp; *c; c++) {
        if (*c == '\r' || *c == ':') { *c = '\0'; break; }
    }

    return ESP_OK;
}

const char *epson_cmd_name(epson_cmd_id_t id)
{
    if (id < 0 || id >= EPSON_CMD_COUNT) return "UNKNOWN";
    return CMD_TABLE[id].name;
}
