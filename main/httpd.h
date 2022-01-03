#ifndef _HTTPD_H_
#define _HTTPD_H_

#include <esp_http_server.h>

httpd_handle_t modelcar_httpd_start_webserver(void);
float modelcar_httpd_get_servo1_factor();
float modelcar_httpd_get_servo2_factor();
int modelcar_httpd_get_servo1_offset();
int modelcar_httpd_get_servo2_offset();
float modelcar_httpd_get_servo1_limit();
float modelcar_httpd_get_servo2_limit();

#endif