set(srcs)
set(include_dirs "include")



list(APPEND srcs "temp_sensor.c") 



idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       PRIV_REQUIRES board esp_adc adc_manager) 
# PRIV_REQUIRES tells espidf the component dependancies of this component