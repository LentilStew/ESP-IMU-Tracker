#include "./mpu.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "mpu6050.h"
#include "string.h"
#include "../../settings.h"
#include "esp_log.h"

static const char *TAG = "MPUPACKET";

// returns size of the header
size_t mpu_packet_fill_header(MpuPacket *packet, char *extra_data)
{
    size_t index = 0;
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, "{");
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, "\"device_id\":%i", packet->device_id);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"gyro_sensitivity\":%f", packet->gyro_sensitivity);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"order\":\"%s\"", packet->order);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"acc_sensitivity\":%f\n", packet->acc_sensitivity);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"hz\":%i", packet->hz);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"timestamp_sec\":%lli", packet->timestamp.tv_sec);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"timestamp_nsec\":%li", packet->timestamp.tv_nsec);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"nb_readings\":%i", packet->nb_readings);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"packet_index\":%lli", packet->packet_index);
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"session_id\":\"%s\"", SESSION_ID);

    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",\"press_index\":[");

    for (int i = 0; i < packet->nb_readings; i++)
    {
        index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, "\"%c\"", packet->press_index[i]);

        if (i != packet->nb_readings - 1)
            index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, ",");
    }
    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, "]");

    if (extra_data != NULL)
        index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, extra_data);

    index += snprintf((char *)&packet->packet_buffer[index], packet->packet_buffer_size, "}");
    return index + 1;
}

void mpu_packet_fill_press_index_header(MpuPacket packet, char *buffer)
{

    char *press_index_array = strstr((char *)buffer, "press_index");
    press_index_array += sizeof("press_index") + 3;

    //"press_index:["0","0","0","0","0"]"
    //"............							(sizeof(press_index))
    //"  		   ...    					3
    //"  		      ....					4

    for (int i = 0; i < packet.nb_readings; i++)
    {
        if (packet.press_index[i] == 1)
            press_index_array[0] = '1';
        press_index_array += 4;
    }
}
// if packet_buffer is null, will use dinamic buffers
esp_err_t packet_alloc(MpuPacket *packet, uint8_t *packet_buffer)
{
    if (packet_buffer == NULL)
    {
        packet->packet_buffer = calloc(sizeof(char), BUFFER_SIZE);
    }
    else
    {
        packet->packet_buffer = packet_buffer;
    }

    if (packet->packet_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to alloc packet\n");
        return ESP_FAIL;
    }
    size_t header_len = mpu_packet_fill_header(packet, NULL);
    packet->readings = (int16_t *)&packet->packet_buffer[header_len];
    return ESP_OK;
}