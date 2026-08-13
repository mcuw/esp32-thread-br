#pragma once

#include "esp_http_server.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

void send_json(httpd_req_t *req, cJSON *json);
void send_json_error(httpd_req_t *req, const char *status, const char *msg);
char *read_body(httpd_req_t *req);

#ifdef __cplusplus
}
#endif