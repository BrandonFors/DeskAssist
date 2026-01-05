#include <stdio.h>
#include "board.h"
#include "rtos_setup.h"
#include "level_indicator.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const BaseType_t app_cpu = 0;


void startUp(){
  //set led pin to output mode
  gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);

  //initialize periferals
  indicator_init();

}

void level_indicator(void *parameters){
  while(1){
    for(int i = 1; i <= 5; i++){
      set_level(i);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }

}


void blink_led(void *parameters) {
  while(1){
    gpio_set_level(LED_BUILTIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(LED_BUILTIN, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  
}

void app_main() {
  startUp();
  

  xTaskCreatePinnedToCore(
    blink_led,
    "Blink LED",
    1024,
    NULL,
    1,
    NULL,
    app_cpu
  );

  xTaskCreatePinnedToCore(
    level_indicator,
    "Level Indicator",
    1024,
    NULL,
    1,
    NULL,
    app_cpu
  );

  vTaskDelete(NULL);
}