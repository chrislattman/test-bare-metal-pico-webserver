#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

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

static void close_client(int client_socket)
{
    shutdown(client_socket, SHUT_WR);
    close(client_socket);
}

// One task per connection: read the request, answer it, close.
static void thread_task(void *params)
{
    char client_message[1024], server_message[512], content[128], content_length[32];
    char *headers_begin;
    int client_socket, n;
    struct sockaddr_in client_connection;
    socklen_t client_connection_size = (socklen_t)sizeof(client_connection);

    // The socket is passed by value, so main_task can accept() again right away
    client_socket = (int)(intptr_t)params;

    // Reading happens here rather than in main_task so that a slow or silent
    // client only stalls its own task, not the accept loop
    n = recv(client_socket, client_message, sizeof(client_message) - 1, 0);
    if (n <= 0) {
        close_client(client_socket);
        vTaskDelete(NULL);
    }
    client_message[n] = '\0';

    headers_begin = strstr(client_message, "\r\n");
    if (headers_begin != NULL) {
        *headers_begin = '\0';
    }
    // Only accepting HTTP GET requests
    if (strncmp(client_message, "GET", 3) != 0) {
        close_client(client_socket);
        vTaskDelete(NULL);
    }
    printf("%s\n", client_message);

    if (xSemaphoreTake(mutex, 10)) {
        ++counter;
        xSemaphoreGive(mutex);
    }

    strcpy(server_message, "HTTP/1.1 200 OK\r\n");
    strcat(server_message, "Server: Raspberry Pi Pico 2 W\r\n");
    strcat(server_message, "Last-Modified: Thu, 18 Mar 2026 05:35:18 GMT\r\n");
    strcat(server_message, "Accept-Ranges: bytes\r\n");

    strcpy(content, "What's up? This server was written in embedded C. Your IP address is ");
    if (getpeername(client_socket, (struct sockaddr *)&client_connection, &client_connection_size) == 0) {
        inet_ntop(AF_INET, &client_connection.sin_addr, content + strlen(content), INET_ADDRSTRLEN);
    } else {
        strcat(content, "unknown");
    }
    strcat(content, "\r\n");

    sprintf(content_length, "Content-Length: %zu\r\n", strlen(content));
    strcat(server_message, content_length);
    strcat(server_message, "Content-Type: text/html\r\n\r\n");
    strcat(server_message, content);

    // Always close the socket, even if send fails, so lwIP doesn't run out of sockets
    send(client_socket, server_message, strlen(server_message), 0);
    close_client(client_socket);
    vTaskDelete(NULL);
}

// A good resource on socket programming for FreeRTOS/lwIP specifically:
// https://visualgdb.com/tutorials/raspberry/pico_w/freertos/
static void main_task(void *params)
{
    int status, server_socket, client_socket;
    struct sockaddr_in server_connection, client_connection;
    socklen_t client_connection_size;
    
    stdio_init_all();
    status = cyw43_arch_init();
    if (status != PICO_OK) {
        vTaskDelete(NULL);
    }

    // Enable Wi-Fi station (as compared to access point) mode
    cyw43_arch_enable_sta_mode();
    status = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (status != PICO_OK) {
        // If the link fails, blink the LED once
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
        vTaskDelete(NULL);
    }

    // If the link succeeds, blink the LED twice and quickly
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

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
        client_connection_size = (socklen_t)sizeof(client_connection);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_connection, &client_connection_size)) < 0) {
            // Usually a temporary shortage of lwIP sockets/memory; keep serving
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        if (xTaskCreate(thread_task, "ClientThread", 2048, (void *)(intptr_t)client_socket,
                THREAD_TASK_PRIORITY, NULL) != pdPASS) {
            // Out of heap for a new task; drop this connection but keep serving
            close_client(client_socket);
        }
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
