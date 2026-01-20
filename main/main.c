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
#include "buttons.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"


#define NUM_ACTUATORS 3 
#define NUM_ACTIONS 3 //number of actions a user can take given the actuator they have selected
#define ACTUATOR_MENU_LEN (NUM_ACTUATORS + 1)
#define ACTION_MENU_LEN   (NUM_ACTIONS + 1)

static const BaseType_t app_cpu = 0;


static char* TAG = "RTOS";

static uint64_t last_button_time[2] = {0}; // array to hold dobounce times

//button interrupt function
void IRAM_ATTR gpio_isr_handler(void* arg){
  ButtonEvent button_pressed = (ButtonEvent)(uintptr_t)arg;
  uint64_t now = esp_timer_get_time() / 1000;
  BaseType_t task_woken = pdFALSE;

  int button_idx = button_pressed - 1;  // Assuming BUTTON_1=1, BUTTON_2=2
  if(now - last_button_time[button_idx] < DEBOUNCE_TIME_MS) {
    return;
  }
  last_button_time[button_idx] = now;

  xQueueSendFromISR(buttonQueue, &button_pressed, &task_woken);
  vTaskNotifyGiveFromISR(userInterfaceTask, &task_woken); //notify the ui task
  if(task_woken) portYIELD_FROM_ISR();

}

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
  buttons_init();


  //set callback functions for button interrupts
  gpio_isr_handler_add(BUT_1_PIN, gpio_isr_handler, (void *)BUTTON_1);
  gpio_isr_handler_add(BUT_2_PIN, gpio_isr_handler, (void *)BUTTON_2);

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

  MenuItem actuator_menu[ACTUATOR_MENU_LEN] = {
    {"Fan",false}, 
    {"Vent", false},
    {"Lamp", false},
    {"Exit", false},
  };

  Actuator_Id actuator_id_arr[ACTUATOR_MENU_LEN] = {
    FAN,
    VENT,
    LAMP,
    ACTUATOR_NA,
  };
  
  MenuItem action_menu[ACTION_MENU_LEN] = {
    {"Mode",false}, 
    {"Toggle", false},
    {"Adjust", false},
    {"Exit", false},
  };

  Action_Id action_id_arr[ACTION_MENU_LEN] = {
    MODE,
    TOGGLE,
    ADJUST,
    ACTION_NA,
  };
  

  //variables needed for UI logic
  int selected_idx = 0;
  Actuator_Id chosen_actuator = ACTUATOR_NA;
  Action_Id chosen_action = ACTION_NA;
  
  //variable to store what button was pressed
  ButtonEvent pressed = NA;
  
  while(1){
    homeScreen();

    // MENU 1: ACTUATORS ///////////////////////
    if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){
      selected_idx = 0;
      actuator_menu[selected_idx].selected = true;
      displayMenu(actuator_menu, ACTUATOR_MENU_LEN);
    }
    pressed = NA;
    while(pressed != BUTTON_1){ // while the select button is not pressed
      if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){
        if(pressed == BUTTON_2){ // this is the down or move button
          actuator_menu[selected_idx].selected = false;
          selected_idx = (selected_idx+1) % ACTUATOR_MENU_LEN;
          actuator_menu[selected_idx].selected = true;
          displayMenu(actuator_menu, ACTUATOR_MENU_LEN);
        }
      }
    }
    //reset the current menu and shared resources for the next menu
    chosen_actuator = actuator_id_arr[selected_idx];
    actuator_menu[selected_idx].selected = false;

    //If exit was chosen, restart
    if (chosen_actuator == ACTUATOR_NA) continue;
  
    // MENU 2: ACTIONS ///////////////////////
    selected_idx = 0;
    action_menu[selected_idx].selected = true;
    displayMenu(action_menu, ACTION_MENU_LEN);

    pressed = NA;
    // loop will run if an actuator was chosen from the previos menu ie exit was not chosen
    while(pressed != BUTTON_1){ 
      if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){
        if(pressed == BUTTON_2){ // this is the down or move button
          action_menu[selected_idx].selected = false;
          selected_idx = (selected_idx+1) % ACTION_MENU_LEN;
          action_menu[selected_idx].selected = true;
          displayMenu(action_menu, ACTION_MENU_LEN);
        }
      }
    }
    chosen_action = action_id_arr[selected_idx];
    action_menu[selected_idx].selected = false;
    
    // If Exit was chosen, restart
    if(chosen_action == ACTION_NA) continue;
    

    // //INTERFACE WITH CONTROLLER BASED ON DATA
    pressed = NA;
    switch(chosen_action){
      case (MODE):

      case(TOGGLE):
        // displayToggle(actuator_menu[chosen_action], );
        
        break;
      case (ADJUST):
        displayAdjust(actuator_menu[chosen_action]);
        //send data to controller and the controller will start sampling
        while(pressed != BUTTON_1 && pressed != BUTTON_2){ 
          if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){
            //send data to the controller again and it will stop sampling
          }
        }
        break;
      case (ACTION_NA):
        //do nothing, program should never trigger this
        break;
    }
  

    
  }
}

//processes data from UI and interfaces with actuator task
void controller_task(void *parameters){

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

  buttonQueue = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(ButtonEvent));


  
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
    &userInterfaceTask,
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