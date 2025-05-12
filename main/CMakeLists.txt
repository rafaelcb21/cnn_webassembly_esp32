idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS .
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_psram esp_http_server esp_http_client spiffs
)