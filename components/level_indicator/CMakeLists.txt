set(srcs)
set(include_dirs "include")



list(APPEND srcs "level_indicator.c") 



idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       PRIV_REQUIRES board esp_driver_gpio esp_driver_spi) 
# PRIV_REQUIRES tells espidf the component dependancies of this component