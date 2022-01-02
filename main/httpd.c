#include "httpd.h"

#include "esp_log.h"
#include "nvs_flash.h"

#define TAG "modelcar httpd"
#define STORAGE_NAMESPACE "storage"

struct nvs_data_s {
    float servo1_factor;
    float servo2_factor;
};

static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t save_get_handler(httpd_req_t *req);

static esp_err_t servo1_get_handler(httpd_req_t *req);
static esp_err_t servo2_get_handler(httpd_req_t *req);

static struct nvs_data_s nvs_data = {
    .servo1_factor = 1.0f,
    .servo2_factor = 1.0f,
};

static const httpd_uri_t uri_root_handler = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_save_handler = {
    .uri       = "/save",
    .method    = HTTP_GET,
    .handler   = save_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_servo1_get_handler = {
    .uri       = "/read_servo1",
    .method    = HTTP_GET,
    .handler   = servo1_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_servo2_get_handler = {
    .uri       = "/read_servo2",
    .method    = HTTP_GET,
    .handler   = servo2_get_handler,
    .user_ctx  = NULL
};


static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "root handler called");

    const char* resp_str = 
    #include "index.html.txt"
    ;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t save_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    ESP_LOGI(TAG, "save handler called");
    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32] = {0};
            if (httpd_query_key_value(buf, "servo1_factor", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => servo1_factor=%s", param);
                sscanf(param, "%f", &nvs_data.servo1_factor);
                ESP_LOGI(TAG, "servo1_factor updated to %f", nvs_data.servo1_factor);
            }
            if (httpd_query_key_value(buf, "servo2_factor", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => servo2_factor=%s", param);
                sscanf(param, "%f", &nvs_data.servo2_factor);
                ESP_LOGI(TAG, "servo2_factor updated to %f", nvs_data.servo2_factor);
            }
        }
        free(buf);
    }

    nvs_handle_t my_handle;

    ESP_LOGI(TAG, "store controller params to NVS");
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle));

    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo1_factor", &nvs_data.servo1_factor, sizeof(float)));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo2_factor", &nvs_data.servo2_factor, sizeof(float)));

    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    const char* resp_str = "<html><body>saved</body></html>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t servo1_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "servo1 handler called");
    char resp_str[30] = {0};
    snprintf(resp_str, sizeof(resp_str), "%.2f", nvs_data.servo1_factor);
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t servo2_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "servo2 handler called");
    
    char resp_str[30] =  {0};
    snprintf(resp_str, sizeof(resp_str), "%.2f", nvs_data.servo2_factor);
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_handle_t modelcar_httpd_start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    nvs_handle_t my_handle;

    ESP_LOGI(TAG, "read data params from NVS");
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle) == ESP_OK)
    {
        size_t s = sizeof(float);
        nvs_get_blob(my_handle, "servo1_factor", &nvs_data.servo1_factor, &s);
        nvs_get_blob(my_handle, "servo2_factor", &nvs_data.servo2_factor, &s);
        nvs_close(my_handle);
    }

    ESP_LOGI(TAG, "servo1_factor is %f", nvs_data.servo1_factor);
    ESP_LOGI(TAG, "servo2_factor is %f", nvs_data.servo2_factor);

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_root_handler);
        httpd_register_uri_handler(server, &uri_save_handler);

        httpd_register_uri_handler(server, &uri_servo1_get_handler);
        httpd_register_uri_handler(server, &uri_servo2_get_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

float modelcar_httpd_get_servo1_factor()
{
    return nvs_data.servo1_factor;
}

float modelcar_httpd_get_servo2_factor()
{
    return nvs_data.servo2_factor;
}
