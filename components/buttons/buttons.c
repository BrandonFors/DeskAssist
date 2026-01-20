#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "buttons.h"
#include "board.h"


void buttons_init(){
  //configure button 1 
  gpio_reset_pin(BUT_1_PIN);
  gpio_set_direction(BUT_1_PIN, GPIO_MODE_INPUT);

  gpio_pullup_en(BUT_1_PIN);
  gpio_pulldown_dis(BUT_1_PIN);

  gpio_set_intr_type(BUT_1_PIN, GPIO_INTR_NEGEDGE);
  
  //configure button 2
  gpio_reset_pin(BUT_2_PIN);
  gpio_set_direction(BUT_2_PIN, GPIO_MODE_INPUT);

  gpio_pullup_en(BUT_2_PIN);
  gpio_pulldown_dis(BUT_2_PIN);

  gpio_set_intr_type(BUT_2_PIN, GPIO_INTR_NEGEDGE);

  gpio_install_isr_service(0);

}

