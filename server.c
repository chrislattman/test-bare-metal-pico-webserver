#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
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

typedef struct sock_info {
    int             client_socket;
    struct in_addr  client_address;
} sock_info;

// Note: errno here is lwIP's (LWIP_PROVIDE_ERRNO=1 is set by
// pico_cyw43_arch_lwip_sys_freertos), so <errno.h> must not be included

/**
 * @brief Thread that handles each client connection.
 *
 * @param params pointer to a heap-allocated sock_info struct (freed here)
 */
static void client_handler(void *params)
{
    char server_message[512], content[128], content_length[32];
    char client_message[1024], *headers_begin;
    sock_info *socket_info;
    int client_socket;
    struct in_addr client_address;
    ssize_t n;

    // Each thread gets its own copy of sock_info, so main_task can't
    // overwrite it with the next connection's socket before we read it
    socket_info = (sock_info *) params;
    client_socket = socket_info->client_socket;
    client_address = socket_info->client_address;
    vPortFree(socket_info);

    // Reading the request happens here rather than in main_task so that a
    // slow or silent client only holds up its own task, not the accept loop.
    // pass MSG_WAITALL in 4th argument to recv to wait for client_message to fill up entirely
    if ((n = recv(client_socket, client_message, sizeof(client_message) - 1, 0)) <= 0) {
        if (n < 0) {
            fprintf(stderr, "recv: %s\n", strerror(errno));
        }
        goto close_client;
    }
    client_message[n] = '\0';
    if ((headers_begin = strstr(client_message, "\r\n")) != NULL) {
        *headers_begin = '\0';
    }
    // Only accepting HTTP GET requests
    if (strncmp(client_message, "GET", 3) != 0) {
        goto close_client;
    }
    printf("%s\n", client_message);

    // critical section
    xSemaphoreTake(mutex, portMAX_DELAY); // using a mutex
    ++counter;
    printf("Handling request #%u\n", counter);
    xSemaphoreGive(mutex);

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
        fprintf(stderr, "send: %s\n", strerror(errno));
    }

close_client:
    // Always close the socket, even after an error, so lwIP doesn't run out of sockets
    if (shutdown(client_socket, SHUT_WR) < 0 && errno != ENOTCONN) {
        fprintf(stderr, "shutdown: %s\n", strerror(errno));
    }
    if (close(client_socket) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
    }
    vTaskDelete(NULL);
}

// A good resource on socket programming for FreeRTOS/lwIP specifically:
// https://visualgdb.com/tutorials/raspberry/pico_w/freertos/
/**
 * @brief Main server loop for the web server.
 *
 * @param params unused
 */
static void main_task(__attribute__((unused)) void *params)
{
    int status, server_socket, client_socket;
    struct sockaddr_in server_connection = {0}, client_connection;
    socklen_t client_connection_size;
    sock_info *socket_info;
    struct timeval recv_timeout = { .tv_sec = 10, .tv_usec = 0 };

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

    if ((mutex = xSemaphoreCreateMutex()) == NULL) {
        fprintf(stderr, "xSemaphoreCreateMutex: out of memory\n");
        vTaskDelete(NULL);
    }

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        vTaskDelete(NULL);
    }
    // No need to set SO_REUSEADDR

    server_connection.sin_family = AF_INET;
    server_connection.sin_port = htons(PORT_NUMBER);
    server_connection.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_socket, (struct sockaddr *) &server_connection,
            (socklen_t) sizeof(server_connection)) < 0) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        goto cleanup;
    }

    if (listen(server_socket, 1) < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        goto cleanup;
    }

    while (1) {
        memset(&client_connection, 0, sizeof(client_connection));
        client_connection_size = (socklen_t) sizeof(client_connection);
        if ((client_socket = accept(server_socket,
                (struct sockaddr *) &client_connection,
                &client_connection_size)) < 0) {
            // Errors like ENFILE (out of lwIP sockets) only affect one
            // connection; back off briefly and keep serving
            fprintf(stderr, "accept: %s\n", strerror(errno));
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // Bound how long an idle client can tie up its task
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
            fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        }

        // Allocated per connection; client_handler frees it
        if ((socket_info = pvPortMalloc(sizeof(*socket_info))) == NULL) {
            fprintf(stderr, "pvPortMalloc: out of memory\n");
            close(client_socket);
            continue;
        }
        *socket_info = (sock_info) {
            .client_socket = client_socket,
            .client_address = client_connection.sin_addr,
        };
        // FreeRTOS tasks delete themselves when done (like detached pthreads).
        //
        // lwIP's rule is that threads which call lwIP should be created with
        // sys_thread_new(), so a port can do per-thread setup (e.g. the
        // per-thread semaphores of LWIP_NETCONN_SEM_PER_THREAD). This port's
        // sys_thread_new() is just a wrapper around xTaskCreate() and that
        // option is off, so xTaskCreate() is equivalent here. It's used
        // because sys_thread_new() asserts when out of memory instead of
        // returning an error, and this way the connection can be dropped
        // cleanly. If LWIP_NETCONN_SEM_PER_THREAD is ever enabled, switch to
        // sys_thread_new() (note its stack size is in bytes, not words).
        if (xTaskCreate(client_handler, "ClientThread", 2048, socket_info,
                THREAD_TASK_PRIORITY, NULL) != pdPASS) {
            fprintf(stderr, "xTaskCreate: out of memory\n");
            vPortFree(socket_info);
            close(client_socket);
            continue;
        }
    }

cleanup:
    if (close(server_socket) < 0) {
        fprintf(stderr, "close: %s\n", strerror(errno));
    }
    vTaskDelete(NULL);
}

int main()
{
    TaskHandle_t task;

    // xTaskCreate() rather than sys_thread_new(): see the note in main_task
    xTaskCreate(main_task, "MainThread", 4096, NULL, MAIN_TASK_PRIORITY, &task);

    vTaskStartScheduler();

    // vTaskStartScheduler() only returns if there wasn't enough FreeRTOS heap
    // to create the idle or timer task. Returning from main() on bare metal
    // isn't meaningful, so stop here instead.
    while (1);
    return 0;
}
