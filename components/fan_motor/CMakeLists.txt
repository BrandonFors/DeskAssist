set(srcs)
set(include_dirs "include")



list(APPEND srcs "fan_motor.c") 



idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       PRIV_REQUIRES board esp_driver_gpio) 
# PRIV_REQUIRES tells espidf the component dependancies of this component