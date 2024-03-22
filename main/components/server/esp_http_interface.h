#ifndef HTTP_SERVER
#define HTTP_SERVER

#include <esp_http_server.h>
#include "esp_event.h"
#include "lwip/sockets.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <netinet/in.h> 
httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);


typedef struct udp_clients {
  struct sockaddr_in addr;
  uint64_t last_update_seconds;
#define CONNECTION_ID_SIZE 10
  char connection_id[CONNECTION_ID_SIZE+1];
  struct udp_clients *next;
}udp_clients;

#define MAX_UDP_CLIENTS 2
extern udp_clients *udp_client_list;
extern SemaphoreHandle_t udp_client_list_mutex;
#endif //HTTP_SERVER