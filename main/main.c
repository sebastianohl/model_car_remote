
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#include "httpd.h"
#include "modelcar.h"
#include "wifi-captive-portal/wifi-captive-portal-esp-idf-dns.h"

#define TAG "modelcar_main"

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event =
            (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac),
                 event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event =
            (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac),
                 event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {.ssid = CONFIG_ESP_WIFI_SSID,
               .ssid_len = strlen(CONFIG_ESP_WIFI_SSID),
               .channel = CONFIG_ESP_WIFI_CHANNEL,
               .password = CONFIG_ESP_WIFI_PASSWORD,
               .max_connection = CONFIG_ESP_MAX_STA_CONN,
               .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(CONFIG_ESP_WIFI_PASSWORD) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD,
             CONFIG_ESP_WIFI_CHANNEL);

    esp_netif_ip_info_t ip_info = {};

    ip_info.ip.addr = esp_ip4addr_aton((const char *)CONFIG_ESP_WIFI_IP);
    ip_info.gw.addr = esp_ip4addr_aton((const char *)CONFIG_ESP_WIFI_IP);
    ip_info.netmask.addr =
        esp_ip4addr_aton((const char *)CONFIG_ESP_WIFI_NETMASK);

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    esp_netif_set_ip_info(ap_netif, &ip_info);
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    ESP_LOGI(TAG, "wifi_network setup finished IP %s Netmask %s",
             CONFIG_ESP_WIFI_IP, CONFIG_ESP_WIFI_NETMASK);

    wifi_captive_portal_esp_idf_dns_init();
}

void app_main(void)
{
    const esp_partition_t *current_partition = esp_ota_get_running_partition();
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    ESP_LOGI(TAG, "running partition %s version %s", current_partition->label,
             app_desc->version);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    modelcar_config_t car_config = {
        .input_channel_count = 2,
        .output_channel_count = 2,
    };
    modelcar_init_output_channel(&car_config.output_channel[0],
                                 CONFIG_SERVO1_OUTPUT_PORT_NUM, LEDC_CHANNEL_0);
    modelcar_init_input_channel(&car_config.input_channel[0],
                                CONFIG_SERVO1_INPUT_PORT_NUM);
    modelcar_init_output_channel(&car_config.output_channel[1],
                                 CONFIG_SERVO2_OUTPUT_PORT_NUM, LEDC_CHANNEL_1);
    modelcar_init_input_channel(&car_config.input_channel[1],
                                CONFIG_SERVO2_INPUT_PORT_NUM);
    modelcar_init(&car_config);

    wifi_init_softap();
    modelcar_httpd_start_webserver();

    ESP_LOGI(TAG, "Minimum free heap size: %d bytes",
             esp_get_minimum_free_heap_size());

    while (1)
    {
        modelcar_queue_value_t value;
        if (xQueueReceive(car_config.gpio_evt_queue, &value,
                          500 / portTICK_RATE_MS))
        {
            switch (value.channel_idx)
            {
            case 0:
            {
                uint32_t modified_dc = modelcar_update_output_by_us(
                    &car_config.output_channel[0], value.pulse_width,
                    modelcar_httpd_get_servo1_factor(),
                    modelcar_httpd_get_servo1_offset(),
                    modelcar_httpd_get_servo1_limit());
#if 1
                ESP_LOGI(TAG, "val servo%d: %d us %d us %f %% %d us",
                         value.channel_idx + 1, value.pulse_width,
                         DutyCyclePercentageToDuty(
                             DutyCycleUsToPercentage(value.pulse_width)),
                         DutyCycleUsToPercentage(value.pulse_width),
                         modified_dc);
#endif
            };
            break;
            case 1:
            {
                modelcar_update_drivemode(&car_config.drive_mode[1],
                                          value.pulse_width,
                                          modelcar_httpd_get_servo2_offset());
                uint32_t modified_dc = modelcar_update_output_by_us(
                    &car_config.output_channel[1], value.pulse_width,
                    (car_config.drive_mode[1] >= BREAK)
                        ? 1
                        : modelcar_httpd_get_servo2_factor(),
                    modelcar_httpd_get_servo2_offset(),
                    modelcar_httpd_get_servo2_limit());
#if 1
                ESP_LOGI(TAG, "val servo%d: %d us %d us %f %% %d us %d mode",
                         value.channel_idx + 1, value.pulse_width,
                         DutyCyclePercentageToDuty(
                             DutyCycleUsToPercentage(value.pulse_width)),
                         DutyCycleUsToPercentage(value.pulse_width),
                         modified_dc, car_config.drive_mode[1]);
#endif
            };
            break;
            default:
                break;
            }
        }
        else
        {
            ESP_LOGW(TAG, "timeout");
        }
    }
}