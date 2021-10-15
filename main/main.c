
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;

static uint32_t val_begin_of_sample = 0;
static uint32_t val_end_of_sample = 0;
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    if (gpio_get_level(CONFIG_SERVO_INPUT_PORT_NUM)) {
        val_begin_of_sample = esp_timer_get_time();
        val_end_of_sample = val_begin_of_sample;
    } else {
        val_end_of_sample = esp_timer_get_time();
        uint32_t pulse_with = val_end_of_sample - val_begin_of_sample;
        xQueueSendFromISR(gpio_evt_queue, &pulse_with, NULL);
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

void app_main(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL<<CONFIG_SERVO_INPUT_PORT_NUM);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(100, sizeof(uint32_t));

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(CONFIG_SERVO_INPUT_PORT_NUM, gpio_isr_handler, (void*) CONFIG_SERVO_INPUT_PORT_NUM);

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

    ledc_channel_config_t ledc_channel = {
            .channel    = LEDC_CHANNEL_0,
            .duty       = 0,
            .gpio_num   = CONFIG_SERVO_OUTPUT_PORT_NUM,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER_1
    };
    ledc_channel_config(&ledc_channel);
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, DutyCyclePercentageToDuty(7.5f));
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    while(1) {
        uint32_t pulse_width;
        if(xQueueReceive(gpio_evt_queue, &pulse_width, 500/portTICK_RATE_MS)) {
            printf("val: %d us %f %%\n", pulse_width, DutyCycleUsToPercentage(pulse_width));
            ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 
                DutyCyclePercentageToDuty(DutyCycleScale(DutyCycleUsToPercentage(pulse_width), 1.0f)));
            ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        } else {
            printf("timeout\n");
        }
    }
}