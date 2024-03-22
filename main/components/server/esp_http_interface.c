#include <esp_http_server.h>
#include "esp_event.h"
#include "./esp_http_interface.h"
#include "esp_log.h"
#include "wifi_setup.h"
#include "../../settings.h"
#include "esp_timer.h"
static const char *TAG = "HTTP_SERVER";
udp_clients *udp_client_list;
SemaphoreHandle_t udp_client_list_mutex;
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
/*
esp_err_t get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    buf_len = httpd_req_get_url_query_len(req) + 1;

    if (!(buf_len > 1))
    {
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    buf = malloc(buf_len);

    if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK)
    {
        free(buf);
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // ?port=12345
    char *port_str = NULL;
    port_str = strtok(buf, "=");
    port_str = strtok(NULL, "=");

    if (port_str == NULL)
    {
        free(buf);
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }

    struct sockaddr_in6 addr;
    socklen_t addr_size = sizeof(addr);

    int socket_fd = httpd_req_to_sockfd(req);

    if (getpeername(socket_fd, (struct sockaddr *)&addr, &addr_size) < 0)
    {
        free(buf);
        ESP_LOGE(TAG, "Error getting client IP");
        return ESP_FAIL;
    }

    udp_clients *new_client = (udp_clients *)malloc(sizeof(udp_clients));
    new_client->port = atoi(port_str);
    new_client->next = NULL;

    if ((new_client->socket_fd = socket(addr.sin6_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        free(buf);
        ESP_LOGE(TAG, "socket failed");
        return ESP_FAIL;
    }

    xSemaphoreTake(udp_client_list_mutex, portMAX_DELAY);
    if (udp_client_list == NULL)
    {
        udp_client_list = new_client;
    }
    else
    {
        udp_clients *temp = udp_client_list;
        while (temp->next != NULL)
        {
            temp = temp->next;
        }
        temp->next = new_client;
    }
    xSemaphoreGive(udp_client_list_mutex);

    free(buf);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
*/
/*
esp_err_t get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {

            // ?port=12345
            char *port_str = NULL;
            port_str = strtok(buf, "=");
            port_str = strtok(NULL, "=");

            if (port_str != NULL)
            {
                struct sockaddr_in6 addr;
                socklen_t addr_size = sizeof(addr);

                int socket_fd = httpd_req_to_sockfd(req);

                if (getpeername(socket_fd, (struct sockaddr *)&addr, &addr_size) < 0)
                {
                    ESP_LOGE(TAG, "Error getting client IP");
                    return ESP_FAIL;
                }
                close(socket_fd);
                // Convert port_str to integer
                // Populate udp_clients structure
                udp_clients *new_client = (udp_clients *)calloc(sizeof(udp_clients), 1);
                new_client->port = atoi(port_str);
                new_client->next = NULL;
                new_client->addr = addr;
                if ((new_client->socket_fd = socket(addr.sin6_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
                {
                    free(buf);
                    ESP_LOGE(TAG, "socket failed");
                    return ESP_FAIL;
                }

                xSemaphoreTake(udp_client_list_mutex, portMAX_DELAY);
                if (udp_client_list == NULL)
                {
                    udp_client_list = new_client;
                }
                else
                {
                    udp_clients *temp = udp_client_list;
                    while (temp->next != NULL)
                    {
                        temp = temp->next;
                    }
                    temp->next = new_client;
                }
                xSemaphoreGive(udp_client_list_mutex);
            }
        }
        free(buf);
    }
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
// HTTP server configuration
httpd_uri_t uri_add_client = {
    .uri = "/add_client",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL};
*/

esp_err_t add_client(httpd_req_t *req)
{

    size_t recv_size = MIN(req->content_len, 100);
    char *content = calloc(sizeof(char), recv_size);
    // IP AND PORT IN BIG ENDIAN RIGHT NEXT TO EACH OTHER
    if (recv_size != (sizeof(in_port_t) + (4 * sizeof(char))))
    {
        ESP_LOGE(TAG, "INVALID SIZE %i", recv_size);
        httpd_resp_send_408(req);

        return ESP_FAIL;
    }
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    char ip[IP4ADDR_STRLEN_MAX];
    inet_pton(AF_INET, content, ip);
    ESP_LOGI(TAG, "ADD CLIENT REQUEST %s %i", ip, htons(*((in_port_t *)(content + 4))));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = *(struct in_addr *)content;
    server_addr.sin_port = *((in_port_t *)(content + 4));

    udp_clients *new_client = (udp_clients *)calloc(sizeof(udp_clients), 1);
    new_client->next = NULL;
    new_client->addr = server_addr;
    new_client->last_update_seconds = esp_timer_get_time() / 1000000;
    create_id(new_client->connection_id, CONNECTION_ID_SIZE);

    xSemaphoreTake(udp_client_list_mutex, portMAX_DELAY);
    if (udp_client_list == NULL)
    {
        udp_client_list = new_client;
    }
    else
    {
        int len = 0;
        udp_clients *temp = udp_client_list;
        while (temp->next != NULL)
        {
            len++;
            temp = temp->next;
        }
        if (len > MAX_UDP_CLIENTS)
        {
            ESP_LOGI(TAG, "Client added, won't stream to it until, one disconnects");
        }
        temp->next = new_client;
    }
    xSemaphoreGive(udp_client_list_mutex);

    httpd_resp_send(req, new_client->connection_id, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* URI handler structure for POST /uri */
httpd_uri_t uri_add_client = {
    .uri = "/add_client",
    .method = HTTP_POST,
    .handler = add_client,
    .user_ctx = NULL};

esp_err_t keep_client(httpd_req_t *req)
{

    size_t recv_size = MIN(req->content_len, 100);
    char *content = calloc(sizeof(char), recv_size);
    // IP AND PORT IN BIG ENDIAN RIGHT NEXT TO EACH OTHER
    if (recv_size != CONNECTION_ID_SIZE) // one for the /0
    {
        ESP_LOGE(TAG, "INVALID SIZE %i != %i", recv_size, CONNECTION_ID_SIZE);
        free(content);
        httpd_resp_send_408(req);

        return ESP_FAIL;
    }
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "TEST");

    ESP_LOGI(TAG, "%.*s", 10, content);

    bool found = false;
    xSemaphoreTake(udp_client_list_mutex, portMAX_DELAY);
    udp_clients *curr = udp_client_list;
    while (curr)
    {
        ESP_LOGI(TAG, "%.*s", 10, curr->connection_id);
        ESP_LOGI(TAG, "%.*s", 10, content);

        if (memcmp(curr->connection_id, content, CONNECTION_ID_SIZE) == 0)
        {
            found = true;
            ESP_LOGI(TAG, "UPDATED");
            curr->last_update_seconds = esp_timer_get_time() / 1000000;
            break;
        }
        curr = curr->next;
    }

    xSemaphoreGive(udp_client_list_mutex);
    if (found)
    {
        httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        httpd_resp_send(req, "Not found", HTTPD_RESP_USE_STRLEN);
    }

    return ESP_OK;
}

/* URI handler structure for POST /uri */
httpd_uri_t uri_keep_client = {
    .uri = "/keep_client",
    .method = HTTP_POST,
    .handler = keep_client,
    .user_ctx = NULL};

/* Our URI handler function to be called during GET /uri request */
esp_err_t get_settings(httpd_req_t *req)
{
    ESP_LOGI(TAG, "SETTINGS REQUEST");
    int resp_max_size = 1024 * 3;
    char *resp = calloc(sizeof(char), resp_max_size);
    int index = 0;

    index += snprintf(&resp[index], resp_max_size, "{");
    index += snprintf(&resp[index], resp_max_size, "\"gyro_row_len\":\"%s\"", ORDER);
    index += snprintf(&resp[index], resp_max_size, ",\"buffer_max_size\":%i\n", BUFFER_SIZE);
    index += snprintf(&resp[index], resp_max_size, ",\"hz\":%i", HZ);
    index += snprintf(&resp[index], resp_max_size, ",\"readings_per_packet\":%i", READINGS_PER_PACKET);
    index += snprintf(&resp[index], resp_max_size, ",\"stream_port\":%i", STREAM_PORT);
    index += snprintf(&resp[index], resp_max_size, ",\"session_id\":\"%s\"", SESSION_ID);
    index += snprintf(&resp[index], resp_max_size, "}");

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    free(resp);
    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_get_settings = {
    .uri = "/get_settings",
    .method = HTTP_GET,
    .handler = get_settings,
    .user_ctx = NULL};

esp_err_t is_alive(httpd_req_t *req)
{
    ESP_LOGI(TAG, "IS ALIVE REQUEST %s", SESSION_ID);

    httpd_resp_send(req, SESSION_ID, sizeof(SESSION_ID));
    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_is_alive = {
    .uri = "/is_alive",
    .method = HTTP_GET,
    .handler = is_alive,
    .user_ctx = NULL};

/* Function for starting the webserver */
httpd_handle_t start_webserver(void)
{
    udp_client_list_mutex = xSemaphoreCreateMutex();
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK)
    {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get_settings);
        httpd_register_uri_handler(server, &uri_add_client);
        httpd_register_uri_handler(server, &uri_is_alive);
        httpd_register_uri_handler(server, &uri_keep_client);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

/* Function for stopping the webserver */
void stop_webserver(httpd_handle_t server)
{
    if (server)
    {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}
