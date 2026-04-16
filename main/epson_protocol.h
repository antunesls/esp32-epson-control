#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* -----------------------------------------------------------------------
 * ESC/VP21 command IDs
 * ----------------------------------------------------------------------- */
typedef enum {
    EPSON_CMD_PWR_ON = 0,
    EPSON_CMD_PWR_OFF,
    EPSON_CMD_SOURCE_HDMI1,
    EPSON_CMD_SOURCE_HDMI2,
    EPSON_CMD_SOURCE_VGA,
    EPSON_CMD_SOURCE_COMP,
    EPSON_CMD_VOL_UP,
    EPSON_CMD_VOL_DOWN,
    EPSON_CMD_MUTE_ON,
    EPSON_CMD_MUTE_OFF,
    EPSON_CMD_BLANK_ON,
    EPSON_CMD_BLANK_OFF,
    EPSON_CMD_QUERY_PWR,
    EPSON_CMD_QUERY_SOURCE,
    EPSON_CMD_QUERY_LAMP,
    EPSON_CMD_COUNT,
    EPSON_CMD_UNKNOWN = -1,
} epson_cmd_id_t;

/* State returned by status queries */
typedef struct {
    char pwr[16];
    char source[16];
    char lamp[16];
} epson_status_t;

esp_err_t      epson_send_command(epson_cmd_id_t cmd_id);
esp_err_t      epson_send_command_str(const char *cmd_str);
epson_cmd_id_t epson_cmd_from_str(const char *str);
esp_err_t      epson_query_status(epson_status_t *status);
const char    *epson_cmd_name(epson_cmd_id_t id);
