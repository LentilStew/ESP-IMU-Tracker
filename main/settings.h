
#ifndef SETTINGS_H
#define SETTINGS_H
#pragma once
#include "driver/gpio.h"

#define UDP_HOST_IP_ADDR "192.168.86.247"
#define UDP_HOST_PORT 3000

#define DEFAULT_SSID "XiaomiMI"
#define DEFAULT_PASSWORD "Caramelo1966"

#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22

#define PRESSED_PIN GPIO_NUM_18

#define BUFFER_SIZE 1024 * 2
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS 1000
#define ORDER "XYZABC"

#define HEADER_SIZE 1024
#define READINGS_PER_PACKET 25
#define HZ 50
#define STREAM_PORT 3000
#define STREAM_PORT_STR "3000"

#define MAX_KEEP_ALIVE_TIME_SEC 10
extern char SESSION_ID[17];  


#endif // SETTINGS_H