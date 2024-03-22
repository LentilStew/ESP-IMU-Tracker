#ifndef MPU_H
#define MPU_H
#pragma once

#include <stdint.h>
#include <time.h>
#include "../../settings.h"

typedef struct MpuPacket
{
    uint8_t device_id;
    struct timespec timestamp;

    int hz;
    int nb_readings; // 0    1    2    3    4    5
    uint8_t press_index[READINGS_PER_PACKET];

    float acc_sensitivity;
    float gyro_sensitivity;
    char order[7];


    int16_t *readings; // xyz  xyz  xyz  xyz  xyz  xyz


    uint64_t packet_index;

    uint8_t *packet_buffer;
    size_t packet_buffer_size;
    struct MpuPacket *next;
} MpuPacket;
esp_err_t packet_alloc(MpuPacket *packet, uint8_t *packet_buffer)
;
size_t mpu_packet_fill_header(MpuPacket *packet, char *extra_data);
void mpu_packet_fill_press_index_header(MpuPacket packet, char *buffer);
#endif // MPU_H