#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include <netdb.h>
int wifi_init_sta(const char *ssid, const char *password, unsigned int nb_retries);
int get_socketfd(const char *ip, const char *port, struct addrinfo **_servinfo, struct addrinfo *hints);
int get_socket_server_fd(int port, struct sockaddr_in **_serv_addr);
void create_id(char *buf, size_t len);
#endif // WIFI_SETUP_H