idf_component_register(
    SRCS "ot_br_web_api.c" "ot_br_web_api_auth.c" "ot_br_web_api_handlers.c" "ot_br_web_api_ota.c" "ot_br_web_api_static.c" "ot_br_web_api_setup_mode.c" "ot_br_web_api_command.c" "ot_br_web_api_coap_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server openthread nvs_flash esp_hw_support esp_app_format mbedtls espressif__cjson app_update esp_https_ota esp_http_client joltwallet__littlefs esp_driver_gpio esp_timer ot_coap_client
)