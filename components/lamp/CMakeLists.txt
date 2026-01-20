set(srcs)
set(include_dirs "include")



list(APPEND srcs "lamp.c") 



idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       PRIV_REQUIRES board esp_driver_ledc) 
# PRIV_REQUIRES tells espidf the component dependancies of this component