#ifndef WIFI_STA_H
#define WIFI_STA_H

#include "esp_err.h"

// Bits for event group (flags for when an event occurs)
#define WIFI_STA_CONNECTED_BIT BIT0
#define WIFI_STA_IPV4_OBTAINED_BIT BIT1
#define WIFI_STA_IPV6_OBTAINED_BIT BIT2

//initialize wifi station mode
//esp_netif_init() and esp_event_loop_create_default() must be called before calling this function
esp_err_t wifi_sta_init(EventGroupHandle_t event_group);

//attempt to reconnect wifi
esp_err_t wifi_sta_reconnect(void);


#endif