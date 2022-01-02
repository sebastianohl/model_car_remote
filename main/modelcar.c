#include "modelcar.h"

#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include <math.h>

#define TAG "modelcar modelcar"

#define ESP_INTR_FLAG_DEFAULT 0

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    modelcar_input_channel_t *channel = (modelcar_input_channel_t*)arg;

    if (gpio_get_level(channel->portnum)) {
        channel->val_begin_of_sample = esp_timer_get_time();
        channel->val_end_of_sample = channel->val_begin_of_sample;
    } else {
        channel->val_end_of_sample = esp_timer_get_time();
        modelcar_queue_value_t value = {
            .pulse_width = channel->val_end_of_sample - channel->val_begin_of_sample,
            .channel_idx = channel->channel_idx,
        };
        xQueueSendFromISR(*(channel->gpio_evt_queue), &value, NULL);
    }
}

uint32_t DutyCyclePercentageToDuty(float per)
{
    return per/100.0f * pow(2,LEDC_TIMER_13_BIT);
}

float DutyCycleScale(float per, float scale)
{
    return 7.5+((7.5f-per)*scale);
}

float DutyCycleUsToPercentage(uint32_t us)
{
    return us / 200.0f /* us to percent at 50hz*/;
}

void modelcar_init_output_channel(modelcar_output_channel_t *channel, uint8_t portnum, uint8_t ledchannel)
{
    channel->portnum = portnum;
    channel->ledchannel = ledchannel;
}

void modelcar_init_input_channel(modelcar_input_channel_t *channel, uint8_t portnum)
{
    channel->portnum = portnum;
    channel->val_begin_of_sample = 0;
    channel->val_end_of_sample = 0;
}

void modelcar_init(modelcar_config_t *config)
{
    config->gpio_evt_queue = xQueueCreate(10, sizeof(modelcar_queue_value_t));

    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = 0;
    for (int i = 0;i<config->input_channel_count;++i)
    {
        io_conf.pin_bit_mask |= (1ULL<<config->input_channel[i].portnum);
    } 
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    for (int i = 0;i<config->input_channel_count;++i)
    {
        gpio_isr_handler_add(config->input_channel[i].portnum, gpio_isr_handler, (void*) &(config->input_channel[i]));
        config->input_channel[i].channel_idx = i;
        config->input_channel[i].gpio_evt_queue = &config->gpio_evt_queue;
    }

    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = 50,                      // frequency of PWM signal
        .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
        .timer_num = LEDC_TIMER_1,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    for (int i = 0;i<config->output_channel_count;++i)
    {
        ledc_channel_config_t ledc_channel = {
                .channel    = config->output_channel[i].ledchannel,
                .duty       = 0,
                .gpio_num   = config->output_channel[i].portnum,
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .hpoint     = 0,
                .timer_sel  = LEDC_TIMER_1
        };
        ledc_channel_config(&ledc_channel);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, config->output_channel[i].ledchannel, DutyCyclePercentageToDuty(7.5f));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, config->output_channel[i].ledchannel);
    }
}

void modelcar_update_output_by_us(modelcar_output_channel_t *channel, uint32_t us, float scale)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel->ledchannel, DutyCyclePercentageToDuty(DutyCycleScale(DutyCycleUsToPercentage(us), scale)));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel->ledchannel);
}