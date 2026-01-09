#include "board.h"
#include "rtos_setup.h"
#include "esp_err.h"
#include "esp_log.h"

//outputs
#include "level_indicator.h"
#include "display.h"
#include "fan_motor.h"
#include "lamp.h"
#include "vent.h"

//inputs
#include "potentiometer.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


static const BaseType_t app_cpu = 0;


static char* TAG = "RTOS";

//startup function that will be called at the begeing of mcu running
void start_up(){
  //set led pin to output mode
  gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);

  //initialize peripherals
  indicator_init();
  display_init();
  fan_init();
  lamp_init();
  vent_init();
  potentiometer_init();




}

// task that handles all outputs (vent, fan, lamp, and level indicator)
void actuators(void *parameters){
  while(1){


    vTaskDelay(1000 / portTICK_PERIOD_MS);


  }

}



void read_potentiometer(void *parameters){
  while(1){
    int pct = read_pot_pct();
    // ESP_LOGI(TAG, "Recieved pot reading of %d%%", pct);
    //send to actuator task 
    //actuator task will apply this value whereever it is relevant according to UI
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
  
}

// handles all user interface display functionality and interactions

void user_interface(void *parameters){
  //need structs for all actuator states, should this be global to be accessable by the task that processes commands for actuators
    // pwm value
    // actuator mode
    // whether its on or off
  //need menu array for each menu (actuator menu, action menu)
  //actions of mode and toggle can be implemented using a screen displaying the current state that is switchable through buttons
  //on/off, automatic/manual

  //variables needed for UI logic
  bool home = true;
  //variable to store what button was pressed
  char pressed = NULL;
  homeScreen();

  while(1){
    if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){

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
  start_up();

  //setup handles for queues 

  buttonQueue = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(char));


  
  ESP_LOGI(TAG, "Creating Tasks.");
  
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
    actuators,
    "Actuator Task",
    2048,
    NULL,
    1,
    NULL,
    app_cpu
  );

  xTaskCreatePinnedToCore(
    user_interface,
    "User Interface",
    2048,
    NULL,
    1,
    NULL,
    app_cpu
  );

  xTaskCreatePinnedToCore(
    read_potentiometer,
    "Pot Test",
    4096,
    NULL,
    1,
    NULL,
    app_cpu
  );

  vTaskDelete(NULL);
}