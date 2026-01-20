set(srcs)
set(include_dirs "include")



list(APPEND srcs "display.c") 



idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       PRIV_REQUIRES u8g2 u8g2-hal-esp-idf board) 
# PRIV_REQUIRES tells espidf the component dependancies of this component