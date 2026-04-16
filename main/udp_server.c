#include "udp_server.h"
#include "epson_protocol.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <string.h>

static const char *TAG = "udp_server";

#define UDP_BUF_SIZE 128

static TaskHandle_t s_task   = NULL;
static int          s_sock   = -1;
static uint16_t     s_port   = 0;

static void udp_server_task(void *arg)
{
    struct sockaddr_in dest = {
        .sin_family      = AF_INET,
        .sin_port        = htons(s_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(s_sock, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        ESP_LOGE(TAG, "bind() failed: errno %d", errno);
        close(s_sock);
        s_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP server listening on port %d", s_port);

    char buf[UDP_BUF_SIZE];
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);

    while (1) {
        int len = recvfrom(s_sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&src, &src_len);
        if (len < 0) {
            if (s_sock < 0) break; /* server stopped */
            ESP_LOGW(TAG, "recvfrom error: errno %d", errno);
            continue;
        }

        /* strip trailing whitespace / newlines */
        while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n' ||
                            buf[len - 1] == ' '))
            len--;
        buf[len] = '\0';

        ESP_LOGI(TAG, "UDP cmd: '%s'", buf);

        const char *reply;
        esp_err_t   ret = epson_send_command_str(buf);
        if (ret == ESP_OK)
            reply = "OK\n";
        else if (ret == ESP_ERR_NOT_FOUND)
            reply = "ERR:unknown\n";
        else if (ret == ESP_ERR_INVALID_STATE)
            reply = "ERR:uart_not_init\n";
        else
            reply = "ERR:uart\n";

        sendto(s_sock, reply, strlen(reply), 0,
               (struct sockaddr *)&src, src_len);
    }

    close(s_sock);
    s_sock = -1;
    vTaskDelete(NULL);
}

esp_err_t udp_server_start(uint16_t port)
{
    if (s_task) return ESP_OK;
    s_port = port;
    BaseType_t rc = xTaskCreate(udp_server_task, "udp_srv", 4096, NULL, 5, &s_task);
    return (rc == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t udp_server_stop(void)
{
    if (s_sock >= 0) {
        shutdown(s_sock, SHUT_RDWR);
        close(s_sock);
        s_sock = -1;
    }
    s_task = NULL;
    return ESP_OK;
}
