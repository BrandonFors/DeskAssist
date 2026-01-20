set(srcs)
set(include_dirs "include")

# set this through menuconfig
if(CONFIG_WIFI_STA_CONNECT)
  list(APPEND srcs 
       "wifi_sta.c") 
endif()


idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       PRIV_REQUIRES esp_wifi esp_netif) 
# PRIV_REQUIRES tells espidf the component dependancies of this component