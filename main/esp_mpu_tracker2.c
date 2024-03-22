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

MpuPacket packet;
uint8_t *packet_buffer;
size_t packet_buffer_size = BUFFER_SIZE;

static mpu6050_handle_t mpu6050 = NULL;

EventGroupHandle_t packet_finished_event;
#define BIT_PACKET_FINISHED (1 << 0)

EventGroupHandle_t packet_restarted_event;
#define BIT_PACKET_RESTARTED (1 << 1)
const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;

static int reading_nb = 0;

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
    if (reading_nb == READINGS_PER_PACKET)
    {
        reading_nb = 0;
        xEventGroupSetBits(packet_finished_event, BIT_PACKET_FINISHED);

        xEventGroupWaitBits(packet_restarted_event,
                            BIT_PACKET_RESTARTED,
                            pdTRUE,
                            pdTRUE,
                            xTicksToWait);
    }
    mpu6050_raw_acce_value_t acce;
    mpu6050_raw_gyro_value_t gyro;

    mpu6050_get_raw_acce(mpu6050,&acce);
    mpu6050_get_raw_gyro(mpu6050,&gyro);
    packet.accelerometer_readings[reading_nb * ACCELEROMETER_ROW_LEN]     = acce.raw_acce_x;
    packet.accelerometer_readings[(reading_nb * ACCELEROMETER_ROW_LEN)+1] = acce.raw_acce_y;
    packet.accelerometer_readings[(reading_nb * ACCELEROMETER_ROW_LEN)+2] = acce.raw_acce_z;
    packet.gyro_readings[reading_nb * GYRO_ROW_LEN]     = gyro.raw_gyro_x;
    packet.gyro_readings[(reading_nb * GYRO_ROW_LEN)+1] = gyro.raw_gyro_y;
    packet.gyro_readings[(reading_nb * GYRO_ROW_LEN)+2] = gyro.raw_gyro_z;

    //ESP_ERROR_CHECK(mpu6050_read(mpu6050, 0x3Bu, (uint8_t *)&packet.accelerometer_readings[reading_nb * ACCELEROMETER_ROW_LEN], 6));
    //ESP_ERROR_CHECK(mpu6050_read(mpu6050, 0x43u, (uint8_t *)&packet.gyro_readings[reading_nb * ACCELEROMETER_ROW_LEN], 6));

    if (gpio_get_level(PRESSED_PIN))
    {
        packet.press_index[reading_nb] = 1;
    }
    reading_nb++;
}

 
void app_main(void)
{

    ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
    // Init all esp stuff
    ESP_ERROR_CHECK(gpio_set_direction(PRESSED_PIN, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(PRESSED_PIN, GPIO_PULLDOWN_ONLY));
    i2c_sensor_mpu6050_init();




    //socket and wifi stuff
    wifi_init_sta(DEFAULT_SSID, DEFAULT_PASSWORD, 5);
    esp_fill_random(SESSION_ID, sizeof(SESSION_ID));

	struct addrinfo *servinfo;
	int socketfd = get_socketfd(UDP_HOST_IP_ADDR,STREAM_PORT_STR, &servinfo);

    //setup global stuff
    packet_finished_event = xEventGroupCreate();
    packet_restarted_event = xEventGroupCreate();
    packet_buffer = calloc(sizeof(uint8_t), packet_buffer_size);
    if (packet_finished_event == NULL || packet_restarted_event == NULL || packet_buffer == NULL)
    {
        ESP_LOGE(TAG, "ERROR CREATING GROUP EVENT");
        exit(0);
    }


    //setup packet settings
    sprintf(packet.accel_order, "xyz");
    sprintf(packet.gyro_order, "xyz");
    ESP_ERROR_CHECK(mpu6050_get_acce_sensitivity(mpu6050, &packet.acc_sensitivity));
    ESP_ERROR_CHECK(mpu6050_get_gyro_sensitivity(mpu6050, &packet.gyro_sensitivity));
    ESP_ERROR_CHECK(mpu6050_get_deviceid(mpu6050, &packet.device_id));
    memset(packet.press_index, '0', sizeof(packet.press_index));
    packet.hz = HZ;
    packet.nb_readings = READINGS_PER_PACKET;
    packet.packet_index = 0;
	clock_gettime(CLOCK_MONOTONIC, &packet.timestamp);
    size_t header_len = mpu_packet_fill_header(packet, (char *)packet_buffer, packet_buffer_size, NULL);
    packet.accelerometer_readings = (int16_t *)&packet_buffer[header_len];
    packet.gyro_readings = (int16_t *)&packet_buffer[header_len +
                                                     ((packet.nb_readings) * ACCELEROMETER_ROW_LEN * sizeof(int16_t))];


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
        xEventGroupWaitBits(packet_finished_event,
                            BIT_PACKET_FINISHED,
                            pdTRUE,
                            pdTRUE,
                            xTicksToWait);

        packet.packet_index++;
        mpu_packet_fill_press_index_header(packet,(char *) packet_buffer);

        udp_clients *curr = udp_client_list;
        ESP_LOGI(TAG,"Sending new packet\n");
        xSemaphoreTake(udp_client_list_mutex, portMAX_DELAY);
        while(curr)
        {
            char ip[INET6_ADDRSTRLEN];  
            inet_ntop(AF_INET6, &(curr->addr.sin6_addr), ip, INET6_ADDRSTRLEN);
            ESP_LOGI(TAG,"\t%s:%d\n",ip,curr->port);

            int numbytes = sendto(curr->socket_fd, (void *)packet_buffer, packet_buffer_size, 0, 
            servinfo->ai_addr, 
            servinfo->ai_addrlen);
            curr = curr->next;
        }
        xSemaphoreGive(udp_client_list_mutex);


        memset(packet_buffer, 0, packet_buffer_size);
        memset(packet.press_index, '0', sizeof(packet.press_index));
	    clock_gettime(CLOCK_MONOTONIC, &packet.timestamp);
        
        size_t header_len = mpu_packet_fill_header(packet, (char *)packet_buffer, packet_buffer_size, NULL);


        packet.accelerometer_readings = (int16_t *)&packet_buffer[header_len];
        packet.gyro_readings = (int16_t *)&packet_buffer[header_len +
                                                         ((packet.nb_readings) * ACCELEROMETER_ROW_LEN * sizeof(int16_t))];

        xEventGroupSetBits(packet_restarted_event, BIT_PACKET_RESTARTED);
    }

    mpu6050_delete(mpu6050);
    ESP_ERROR_CHECK(i2c_driver_delete(I2C_MASTER_NUM));
}
