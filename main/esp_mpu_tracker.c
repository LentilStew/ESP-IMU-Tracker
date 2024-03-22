#include <stdio.h>
#include "./settings.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "mpu6050.h"
#include "esp_system.h"
#include "esp_log.h"
#include <freertos/event_groups.h>
#include "esp_timer.h"
#include <string.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_netif.h"
#include "./components/mpu/mpu.h"
#include "./components/server/wifi_setup.h"
#include "./components/server/esp_http_interface.h"
#include <netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "MAIN";

char SESSION_ID[17];
MpuPacket *packet = NULL;

MpuPacket *packet_list = NULL;

SemaphoreHandle_t packet_mutex;

mpu6050_handle_t mpu6050 = NULL;

EventGroupHandle_t new_packet_event;

const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;

static int reading_nb = 0;

static inline void fill_packet_info(MpuPacket *packet)
{
    sprintf(packet->order, ORDER);
    ESP_ERROR_CHECK(mpu6050_get_acce_sensitivity(mpu6050, &packet->acc_sensitivity));
    ESP_ERROR_CHECK(mpu6050_get_gyro_sensitivity(mpu6050, &packet->gyro_sensitivity));
    ESP_ERROR_CHECK(mpu6050_get_deviceid(mpu6050, &packet->device_id));
    memset(packet->press_index, '0', sizeof(packet->press_index));
    packet->hz = HZ;
    packet->nb_readings = READINGS_PER_PACKET;
    clock_gettime(CLOCK_MONOTONIC, &packet->timestamp);
    packet->packet_buffer_size = BUFFER_SIZE;
}

void i2c_bus_init(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));

    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

void i2c_sensor_mpu6050_init(void)
{
    i2c_bus_init();
    mpu6050 = mpu6050_create(I2C_MASTER_NUM, MPU6050_I2C_ADDRESS);
    if (mpu6050 == NULL)
    {
        printf("FAILED TO CREATE MPU6050");
        exit(1);
    }
    ESP_ERROR_CHECK(mpu6050_config(mpu6050, ACCE_FS_2G, GYRO_FS_500DPS));

    ESP_ERROR_CHECK(mpu6050_wake_up(mpu6050));
}

static inline void fill_row(void *_)
{
    if (reading_nb % READINGS_PER_PACKET == 0)
    {
        xSemaphoreTake(packet_mutex, portMAX_DELAY);
        if (packet != NULL)
        {
            MpuPacket **curr = &packet_list;
            while (curr[0])
            {
                curr = &curr[0]->next;
            }
            curr[0] = packet;
            packet = NULL;
        }

        packet = calloc(1, sizeof(MpuPacket));
        fill_packet_info(packet);
        packet->packet_index = reading_nb / READINGS_PER_PACKET;

        ESP_ERROR_CHECK(packet_alloc(packet, NULL));

        xSemaphoreGive(packet_mutex);
        // ESP_LOGI(TAG, "FINISHED PACKET");

        xEventGroupSetBits(new_packet_event, BIT0);
    }
    int packet_reading_number = reading_nb % READINGS_PER_PACKET;
    mpu6050_raw_acce_value_t acce;
    mpu6050_raw_gyro_value_t gyro;

    mpu6050_get_raw_acce(mpu6050, &acce);
    mpu6050_get_raw_gyro(mpu6050, &gyro);
    packet->readings[packet_reading_number + (packet->nb_readings * 0)] = htons(acce.raw_acce_x);
    packet->readings[packet_reading_number + (packet->nb_readings * 1)] = htons(acce.raw_acce_y);
    packet->readings[packet_reading_number + (packet->nb_readings * 2)] = htons(acce.raw_acce_z);
    packet->readings[packet_reading_number + (packet->nb_readings * 3)] = htons(gyro.raw_gyro_x);
    packet->readings[packet_reading_number + (packet->nb_readings * 4)] = htons(gyro.raw_gyro_y);
    packet->readings[packet_reading_number + (packet->nb_readings * 5)] = htons(gyro.raw_gyro_z);

    // ESP_ERROR_CHECK(mpu6050_read(mpu6050, 0x3Bu, (uint8_t *)&packet.accelerometer_readings[reading_nb * ACCELEROMETER_ROW_LEN], 6));
    // ESP_ERROR_CHECK(mpu6050_read(mpu6050, 0x43u, (uint8_t *)&packet.gyro_readings[reading_nb * ACCELEROMETER_ROW_LEN], 6));

    if (gpio_get_level(PRESSED_PIN))
    {
        packet->press_index[reading_nb] = 1;
    }
    reading_nb++;
}

void app_main(void)
{

    packet_mutex = xSemaphoreCreateMutex();
    new_packet_event = xEventGroupCreate();
    if (new_packet_event == NULL)
    {
        // Handle event group creation failure
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    // Init all esp stuff
    ESP_ERROR_CHECK(gpio_set_direction(PRESSED_PIN, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(PRESSED_PIN, GPIO_PULLDOWN_ONLY));
    i2c_sensor_mpu6050_init();

    // socket and wifi stuff
    wifi_init_sta(DEFAULT_SSID, DEFAULT_PASSWORD, 5);

    create_id(SESSION_ID, sizeof(SESSION_ID));

    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        ESP_LOGE(TAG, "socket failed");
        return;
    }

    const esp_timer_create_args_t fill_row_timer_args = {
        .callback = &fill_row,
        .skip_unhandled_events = true,
    };

    esp_timer_handle_t fill_row_timer;
    ESP_ERROR_CHECK(esp_timer_create(&fill_row_timer_args, &fill_row_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(fill_row_timer, 1000000 / HZ));

    httpd_handle_t *handler = start_webserver();

    while (1)
    {
        // EventBits_t bits =
        xEventGroupWaitBits(new_packet_event, BIT0, pdTRUE, pdTRUE, portMAX_DELAY);
        uint64_t curr_time_seconds = esp_timer_get_time() / 1000000;
        // ESP_LOGI(TAG, "Sending new packet\n");
        xSemaphoreTake(udp_client_list_mutex, portMAX_DELAY);
        xSemaphoreTake(packet_mutex, portMAX_DELAY);
        udp_clients *curr_server = udp_client_list;
        udp_clients **last_next = &udp_client_list;
        while (packet_list)
        {
            while (curr_server)
            {
                if (curr_time_seconds - curr_server->last_update_seconds > MAX_KEEP_ALIVE_TIME_SEC)
                {
                    last_next[0] = curr_server->next;
                    free(curr_server);
                    curr_server = last_next[0];
                    continue;
                }

                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(curr_server->addr.sin_addr), ip, INET_ADDRSTRLEN);

                int numbytes = sendto(socket_fd, (void *)packet_list->packet_buffer, BUFFER_SIZE, 0,
                                      (struct sockaddr *)&curr_server->addr,
                                      sizeof(struct sockaddr_in));

                ESP_LOGI(TAG, "\t%s:%d SENT %i\n", ip, htons(curr_server->addr.sin_port), numbytes);
                last_next = &curr_server->next;
                curr_server = curr_server->next;
            }

            MpuPacket *next = packet_list->next;
            free(packet_list->packet_buffer);
            free(packet_list);
            packet_list = next;
        }
        xSemaphoreGive(udp_client_list_mutex);
        xSemaphoreGive(packet_mutex);
    }

    mpu6050_delete(mpu6050);
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
}
