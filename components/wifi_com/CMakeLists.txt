set(srcs)
set(include "include")

list(APPEND srcs "wifi_com.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include}"
                       PRIV_REQUIRES wifi_sta esp_netif nvs_flash json esp_netif)

