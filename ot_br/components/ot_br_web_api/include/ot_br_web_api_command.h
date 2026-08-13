#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void ot_br_command_init(void);

esp_err_t ot_br_command_send(const char *address_str, const char *command);

void ot_br_command_build_rloc_address(uint16_t rloc16, char *out, size_t out_len);

esp_err_t ot_br_command_send_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif