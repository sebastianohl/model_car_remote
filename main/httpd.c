#include "httpd.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi-captive-portal/wifi-captive-portal-esp-idf-httpd.h"

#define TAG "modelcar httpd"
#define STORAGE_NAMESPACE "storage"

struct nvs_data_s {
    float servo1_factor;
    float servo2_factor;
    int servo1_offset;
    int servo2_offset;
    float servo1_limit;
    float servo2_limit;
};

static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t save_get_handler(httpd_req_t *req);

static esp_err_t servo_read_get_handler(httpd_req_t *req);

static struct nvs_data_s nvs_data = {
    .servo1_factor = 1.0f,
    .servo2_factor = 1.0f,
    .servo1_offset = 0,
    .servo2_offset = 0,
    .servo1_limit = 1.0f,
    .servo2_limit = 1.0f,
};

static httpd_uri_t common_get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = rest_common_get_handler,
    .user_ctx = NULL,
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

static const httpd_uri_t uri_servo_read_get_handler = {
    .uri       = "/read",
    .method    = HTTP_GET,
    .handler   = servo_read_get_handler,
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
            if (httpd_query_key_value(buf, "servo1_offset", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => servo1_offset=%s", param);
                sscanf(param, "%d", &nvs_data.servo1_offset);
                ESP_LOGI(TAG, "servo1_offset updated to %d", nvs_data.servo1_offset);
            }
            if (httpd_query_key_value(buf, "servo2_offset", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => servo2_offset=%s", param);
                sscanf(param, "%d", &nvs_data.servo2_offset);
                ESP_LOGI(TAG, "servo2_offset updated to %d", nvs_data.servo2_offset);
            }
            if (httpd_query_key_value(buf, "servo1_limit", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => servo1_limit=%s", param);
                sscanf(param, "%f", &nvs_data.servo1_limit);
                ESP_LOGI(TAG, "servo1_limit updated to %f", nvs_data.servo1_limit);
            }
            if (httpd_query_key_value(buf, "servo2_limit", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => servo2_limit=%s", param);
                sscanf(param, "%f", &nvs_data.servo2_limit);
                ESP_LOGI(TAG, "servo2_limit updated to %f", nvs_data.servo2_limit);
            }
        }
        free(buf);
    }

    nvs_handle_t my_handle;

    ESP_LOGI(TAG, "store controller params to NVS");
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle));

    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo1_factor", &nvs_data.servo1_factor, sizeof(nvs_data.servo1_factor)));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo2_factor", &nvs_data.servo2_factor, sizeof(nvs_data.servo1_factor)));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo1_offset", &nvs_data.servo1_offset, sizeof(nvs_data.servo1_offset)));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo2_offset", &nvs_data.servo2_offset, sizeof(nvs_data.servo1_offset)));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo1_limit", &nvs_data.servo1_limit, sizeof(nvs_data.servo1_limit)));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, "servo2_limit", &nvs_data.servo2_limit, sizeof(nvs_data.servo1_limit)));

    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);

    const char* resp_str = "<html><body>saved</body></html>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t servo_read_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "servo read handler called");

    char resp_str[128] = {0};

    char* buf = 0;
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[64] = {0};
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
                if (strcmp(param, "servo1_factor") == 0) {
                    snprintf(resp_str, sizeof(resp_str), "%.2f", nvs_data.servo1_factor);
                } else if (strcmp(param, "servo2_factor") == 0) {
                    snprintf(resp_str, sizeof(resp_str), "%.2f", nvs_data.servo2_factor);
                } else if (strcmp(param, "servo1_offset") == 0) {
                    snprintf(resp_str, sizeof(resp_str), "%d", nvs_data.servo1_offset);
                } else if (strcmp(param, "servo2_offset") == 0) {
                    snprintf(resp_str, sizeof(resp_str), "%d", nvs_data.servo2_offset);
                } else if (strcmp(param, "servo1_limit") == 0) {
                    snprintf(resp_str, sizeof(resp_str), "%.2f", nvs_data.servo1_limit);
                } else if (strcmp(param, "servo2_limit") == 0) {
                    snprintf(resp_str, sizeof(resp_str), "%.2f", nvs_data.servo2_limit);
                } else {
                    snprintf(resp_str, sizeof(resp_str), "error value %s not found", param);
                }
            } else {
                snprintf(resp_str, sizeof(resp_str), "error no value found");
            }
        }
        free(buf);
    }

    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static bool my_uri_match_wildcard(const char *uri_template, const char *uri_to_match, size_t match_upto)
{
  ESP_LOGI(TAG, "%s ~= %s", uri_template, uri_to_match);
  return httpd_uri_match_wildcard(uri_template, uri_to_match, match_upto);
}

httpd_handle_t modelcar_httpd_start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = my_uri_match_wildcard;
  
    nvs_handle_t my_handle;

    ESP_LOGI(TAG, "read data params from NVS");
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle) == ESP_OK)
    {
        size_t s = sizeof(float);
        nvs_get_blob(my_handle, "servo1_factor", &nvs_data.servo1_factor, &s);
        nvs_get_blob(my_handle, "servo2_factor", &nvs_data.servo2_factor, &s);
        nvs_get_blob(my_handle, "servo1_offset", &nvs_data.servo1_offset, &s);
        nvs_get_blob(my_handle, "servo2_offset", &nvs_data.servo2_offset, &s);
        nvs_get_blob(my_handle, "servo1_limit", &nvs_data.servo1_limit, &s);
        nvs_get_blob(my_handle, "servo2_limit", &nvs_data.servo2_limit, &s);
        nvs_close(my_handle);
    }

    ESP_LOGI(TAG, "servo1_factor is %.2f", nvs_data.servo1_factor);
    ESP_LOGI(TAG, "servo2_factor is %.2f", nvs_data.servo2_factor);
    ESP_LOGI(TAG, "servo1_offset is %d", nvs_data.servo1_offset);
    ESP_LOGI(TAG, "servo2_offset is %d", nvs_data.servo2_offset);
    ESP_LOGI(TAG, "servo1_limit is %.2f", nvs_data.servo1_limit);
    ESP_LOGI(TAG, "servo2_limit is %.2f", nvs_data.servo2_limit);

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        
        httpd_register_uri_handler(server, &uri_root_handler);
        httpd_register_uri_handler(server, &uri_save_handler);
        httpd_register_uri_handler(server, &uri_servo_read_get_handler);

        common_get_uri.user_ctx = malloc(100);
        snprintf((char *)common_get_uri.user_ctx, 100, "http://%s/", CONFIG_ESP_WIFI_IP);
        httpd_register_uri_handler(server, &common_get_uri);

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

int modelcar_httpd_get_servo1_offset()
{
    return nvs_data.servo1_offset;
}

int modelcar_httpd_get_servo2_offset()
{
    return nvs_data.servo2_offset;
}

float modelcar_httpd_get_servo1_limit()
{
    return nvs_data.servo1_limit;
}

float modelcar_httpd_get_servo2_limit()
{
    return nvs_data.servo2_limit;
}
