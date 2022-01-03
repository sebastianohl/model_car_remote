#ifndef _MODELCAR_H_
#define _MODELCAR_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct modelcar_input_channel_s
{
    uint32_t val_begin_of_sample;
    uint32_t val_end_of_sample;
    uint8_t portnum;
    uint8_t channel_idx;
    xQueueHandle *gpio_evt_queue;
};
typedef struct modelcar_input_channel_s modelcar_input_channel_t;

struct modelcar_output_channel_s
{
    uint8_t portnum;
    uint8_t ledchannel;
};
typedef struct modelcar_output_channel_s modelcar_output_channel_t;

enum drive_mode_e
{
    NEUTRAL=0,
    FORWARD=1,
    NEUTRAL_FORWARD=2,
    BACKWARDS=3,
    BREAK=4,
    BREAK_BACKWARDS=5,
    MAX
};
typedef enum drive_mode_e drive_mode_t;

struct modelcar_config_s
{
    xQueueHandle gpio_evt_queue;
    uint8_t input_channel_count;
    modelcar_input_channel_t input_channel[4];
    drive_mode_t drive_mode[4];
    uint8_t output_channel_count;
    modelcar_output_channel_t output_channel[4];
};
typedef struct modelcar_config_s modelcar_config_t;

struct modelcar_queue_value_s
{
    uint32_t pulse_width;
    uint8_t channel_idx;
};
typedef struct modelcar_queue_value_s modelcar_queue_value_t;

void modelcar_init_output_channel(modelcar_output_channel_t *channel, uint8_t portnum, uint8_t ledchannel);
void modelcar_init_input_channel(modelcar_input_channel_t *channel, uint8_t portnum);
void modelcar_init(modelcar_config_t *config);

uint32_t modelcar_update_output_by_us(modelcar_output_channel_t *channel, uint32_t us, float scale, int offset, float limit);
void modelcar_update_drivemode(drive_mode_t *channel, uint32_t us, int offset);

uint32_t DutyCyclePercentageToDuty(float per);
float DutyCycleUsToPercentage(int32_t us);


#endif