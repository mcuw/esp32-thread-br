#pragma once

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

bool ot_br_web_api_check_auth(httpd_req_t *req);

#ifdef __cplusplus
}
#endif