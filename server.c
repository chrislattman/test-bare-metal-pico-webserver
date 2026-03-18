#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <string.h>

#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY      ( tskIDLE_PRIORITY + 2UL )
#define THREAD_TASK_PRIORITY    ( tskIDLE_PRIORITY + 1UL )

static const unsigned short PORT_NUMBER = 8080;
static SemaphoreHandle_t mutex;
static unsigned int counter = 0;

typedef struct sock_info {
    int             client_socket;
    struct in_addr  client_address;
} sock_info;

static void thread_task(void *params)
{
    char server_message[512], content[128], content_length[32];
    sock_info *socket_info;
    int client_socket;
    struct in_addr client_address;

    if (xSemaphoreTake(mutex, 10) == pdTRUE) {
        ++counter;
        xSemaphoreGive(mutex);
    }

    socket_info = (sock_info *)params;
    client_socket = socket_info->client_socket;
    client_address = socket_info->client_address;

    strcpy(server_message, "HTTP/1.1 200 OK\r\n");
    strcat(server_message, "Server: Raspberry Pi Pico 2 W\r\n");
    strcat(server_message, "Last-Modified: Thu, 18 Mar 2026 05:35:18 GMT\r\n");
    strcat(server_message, "Accept-Ranges: bytes\r\n");

    strcpy(content, "What's up? This server was written in embedded C. Your IP address is ");
    inet_ntop(AF_INET, &client_address, content + strlen(content), INET_ADDRSTRLEN);
    strcat(content, "\r\n");

    sprintf(content_length, "Content-Length: %zu\r\n", strlen(content));
    strcat(server_message, content_length);
    strcat(server_message, "Content-Type: text/html\r\n\r\n");
    strcat(server_message, content);

    if (send(client_socket, server_message, strlen(server_message), 0) < 0) {
        vTaskDelete(NULL);
    }
    if (shutdown(client_socket, SHUT_WR) < 0) {
        vTaskDelete(NULL);
    }
    if (close(client_socket) < 0) {
        vTaskDelete(NULL);
    }
    vTaskDelete(NULL);
}

// A good resource on socket programming for FreeRTOS/lwIP specifically:
// https://visualgdb.com/tutorials/raspberry/pico_w/freertos/
static void main_task(void *params)
{
    int status, server_socket, client_socket, request_line_length;
    struct sockaddr_in server_connection, client_connection;
    char client_message[1024], *headers_begin;
    sock_info socket_info;
    socklen_t client_connection_size = (socklen_t)sizeof(client_connection);
    
    stdio_init_all();
    status = cyw43_arch_init();
    if (status != PICO_OK) {
        vTaskDelete(NULL);
    }

    // Enable Wi-Fi station (as compared to access point) mode
    cyw43_arch_enable_sta_mode();
    status = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (status != PICO_OK) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
        vTaskDelete(NULL);
    }

    mutex = xSemaphoreCreateMutex();

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        vTaskDelete(NULL);
    }
    // No need to set SO_REUSEADDR

    server_connection.sin_family = AF_INET;
    server_connection.sin_port = htons(PORT_NUMBER);
    server_connection.sin_addr.s_addr = 0;
    if (bind(server_socket, (struct sockaddr *)&server_connection, (socklen_t)sizeof(server_connection)) < 0) {
        goto cleanup;
    }

    if (listen(server_socket, 1) < 0) {
        goto cleanup;
    }

    while (1) {
        memset(&client_connection, 0, sizeof(client_connection));
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_connection, &client_connection_size)) < 0) {
            goto cleanup;
        }

        if (recv(client_socket, client_message, sizeof(client_message), 0) < 0) {
            goto cleanup;
        }

        headers_begin = strstr(client_message, "\r\n");
        request_line_length = headers_begin - client_message;
        client_message[request_line_length] = '\0';
        // Only accepting HTTP GET requests
        if (strncmp(client_message, "GET", 3) != 0) {
            if (shutdown(client_socket, SHUT_WR) < 0) {
                goto cleanup;
            }
            if (close(client_socket) < 0) {
                goto cleanup;
            }
        }

        socket_info = (sock_info) {
            .client_socket = client_socket,
            .client_address = client_connection.sin_addr,
        };
        xTaskCreate(thread_task, "ClientThread", 2048, &socket_info, THREAD_TASK_PRIORITY, NULL);
    }

cleanup:
    close(server_socket);
    vTaskDelete(NULL);
}

int main()
{
    TaskHandle_t task;

    xTaskCreate(main_task, "MainThread", 4096, NULL, MAIN_TASK_PRIORITY, &task);

    vTaskStartScheduler();

    return 0;
}
