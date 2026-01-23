#include "board.h"
#include "rtos_setup.h"
#include <stdio.h>
#include <time.h>

//esp headders
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/gptimer.h"
#include "driver/ledc.h"

//outputs
#include "level_indicator.h"
#include "display.h"
#include "fan_motor.h"
#include "lamp.h"
#include "vent.h"

//inputs
#include "potentiometer.h"
#include "buttons.h"
#include "photoresistor.h"
#include "temp_sensor.h"

//rtos
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

//wifi
#include "wifi_com.h"


#define NUM_ACTUATORS 3 
#define NUM_ACTIONS 3 //number of actions a user can take given the actuator they have selected
#define ACTUATOR_MENU_LEN (NUM_ACTUATORS + 1)
#define ACTION_MENU_LEN   (NUM_ACTIONS + 1)

static const BaseType_t app_cpu = 1; //core for application purposes
static const BaseType_t pro_cpu = 0; //wifi core

//queue handles
QueueHandle_t buttonQueue = NULL; //handles button interrupts to UI task
QueueHandle_t controllerQueue = NULL; //handles messages sent to controller
QueueHandle_t tempReadingQueue = NULL; //hanldes temp readings to UI for home display
QueueSetHandle_t wifiDataQueue = NULL; //handles sending wifi collected data to UI for home display

//mutexes
SemaphoreHandle_t adcMutex = NULL;

//Task Handles 
TaskHandle_t userInterfaceTask = NULL;
TaskHandle_t potentiometerSampleTask = NULL;
TaskHandle_t tempPhotoSampleTask = NULL;


static char* TAG = "RTOS";


static uint64_t last_button_time[2] = {0}; // array to hold dobounce times

/**************************************
 * Private function prototypes
 */
void gpio_isr_handler(void* arg);
bool signal_sample_pot(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
void potentiometer_task(void *parameters);
void start_up();
void user_interface(void *parameters);
void controller_task(void *parameters);

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

  xQueueSendToBackFromISR(buttonQueue, &button_pressed, &task_woken);
  vTaskNotifyGiveFromISR(userInterfaceTask, &task_woken); //notify the ui task
  if(task_woken) portYIELD_FROM_ISR();

}

//signal the potentiometer_task 
bool IRAM_ATTR signal_sample_pot(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx){
  BaseType_t task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(potentiometerSampleTask, &task_woken);
  return (task_woken == pdTRUE);
}

//startup function that will be called at the begeing of mcu running
void start_up(){

  //initialize peripherals
  indicator_init();
  display_init();
  fan_init();
  lamp_init();
  vent_init();
  potentiometer_init();
  buttons_init();
  photoresistor_init();
  temp_sensor_init();
  wifi_com_init();


  //set callback functions for button interrupts
  gpio_isr_handler_add(BUT_1_PIN, gpio_isr_handler, (void *)BUTTON_1);
  gpio_isr_handler_add(BUT_2_PIN, gpio_isr_handler, (void *)BUTTON_2);


  //install fade functionality for ledc driver
  ESP_ERROR_CHECK(ledc_fade_func_install(0));

  //set level indicator to 0 as potentiometer is not being sampled
  set_level_indicator(0);

}

void setup_isrs(){
  //create an event callback for potentiometer signal
  gptimer_event_callbacks_t pot_signal_callback = {
    .on_alarm = signal_sample_pot,
  };

  //register timer event callback function for sampling potentiometer values
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(pot_timer, &pot_signal_callback, NULL));

  //enable pot timer
  //timer will be started and stopped from the 
  ESP_ERROR_CHECK(gptimer_enable(pot_timer));
}





void potentiometer_task(void *parameters){
  while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    //protect adc unit
    xSemaphoreTake(adcMutex, portMAX_DELAY);
    int pct = read_pot_pct();
    xSemaphoreGive(adcMutex);
    // ESP_LOGI(TAG, "Recieved pot reading of %d%%", pct);
    //send to actuator task 
    //actuator task will apply this value whereever it is relevant according to UI
    ControllerMsg instruction = {
      .pct = pct,
      .sender_id = POTENTIOMETER,
    };
    if(xQueueSendToBack(controllerQueue, &instruction, 0) == pdFAIL){
      ESP_LOGI(TAG, "Failed to send potentiometer reading");
    }
  } 
}

void read_temp_photo(void *parameters){
  int sent_temp = 0;
  while(1){

    // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    xSemaphoreTake(adcMutex, portMAX_DELAY);
    int reading = read_photo_light();
    xSemaphoreGive(adcMutex);

    ControllerMsg instruction = {
      .pct = reading,
      .sender_id = PHOTORESISTOR,
    };
    if(xQueueSendToBack(controllerQueue ,&instruction, 0) == pdFALSE){
      ESP_LOGI(TAG, "Photoresistor reading dropped");
    }

    xSemaphoreTake(adcMutex, portMAX_DELAY);
    reading = read_temp_pct();
    xSemaphoreGive(adcMutex);

    //send temp sensor as a percentage to controller task
    instruction.pct = reading;
    instruction.sender_id = TEMP_SENSOR;
    if(xQueueSendToBack(controllerQueue ,&instruction, 0) == pdFALSE){
      ESP_LOGI(TAG, "Temperature reading dropped");
    }

    xSemaphoreTake(adcMutex, portMAX_DELAY);
    reading = read_temp_deg();
    xSemaphoreGive(adcMutex);
    if(reading != sent_temp){
      TempReading tempReading = {
        .temp = reading,
      };
      
      if(xQueueSendToFront(tempReadingQueue, &tempReading, 0) == pdFALSE){
        ESP_LOGI(TAG, "Temperature degree reading dropped");
      }else{
        sent_temp = reading;
      }
    }
    

    //send temp sensor as a temperature to UI task if different from sent_temp
    vTaskDelay(1000 / portTICK_PERIOD_MS);

  }
}

void wifi_task(void *parameters){
  WifiData wifiData = {0};

  while(1){
    int temp = wifi_get_temp();
    if(temp != -100){
      wifiData.temp = temp;
      if(xQueueSendToFront(wifiDataQueue, &wifiData, 0)== pdFALSE){
        ESP_LOGI(TAG, "Wifi data dropped");
      }
    }
    
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }

}


// handles all user interface display functionality and interactions

void user_interface(void *parameters){

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
  //stores temp reading from temp sensor
  TempReading tempReading = {0};
  WifiData wifiData = {0};

  time_t current_time;
  struct tm *tm_local;

  char inside_temp[5] = "";
  char outside_temp[5] = "";
  char cur_time_str[6] = "";


  while(1){
    homeScreen(inside_temp, outside_temp, cur_time_str);
    // check the button queue for 1 sec and then refresh the screen
    while(xQueueReceive(buttonQueue, &pressed, pdMS_TO_TICKS(1000)) == pdFALSE){ 
      //scan wifi and temp queue
      if(xQueueReceive(tempReadingQueue, &tempReading, 0) == pdTRUE ||
         xQueueReceive(wifiDataQueue, &wifiData, 0) == pdTRUE){
        //copy temps to strings from most recent data recieved
        snprintf(inside_temp, sizeof(inside_temp),"%d", tempReading.temp);
        snprintf(outside_temp, sizeof(outside_temp), "%d", wifiData.temp);

      }
      //get local time
      current_time = time(NULL);
      tm_local = localtime(&current_time);
      snprintf(cur_time_str, sizeof(cur_time_str), "%d:%d", tm_local->tm_hour, tm_local->tm_min);


      //refresh screen
      homeScreen(inside_temp, outside_temp, cur_time_str);


    }
    // MENU 1: ACTUATORS ///////////////////////
    selected_idx = 0;
    actuator_menu[selected_idx].selected = true;
    displayMenu(actuator_menu, ACTUATOR_MENU_LEN);
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
    //construct a instruction for the controller with input data
    ControllerMsg instruction = {
      .action_id = chosen_action,
      .actuator_id = chosen_actuator,
      .sender_id = UI
    };

    switch(chosen_action){
      case (MODE):
        //query the respective driver for its mode 
        bool is_auto = NULL;
        if(chosen_actuator == FAN){
          is_auto = get_fan_is_auto();
        }else if(chosen_actuator == VENT){
          is_auto = get_vent_is_auto();
        }else if(chosen_actuator == LAMP){
          is_auto = get_lamp_is_auto();
        }else{
          ESP_LOGI(TAG, "Unhandled acutator ID in MODE case");
        }
        //send a message to the controller to update the output of of the given actuator 
        displayMode(actuator_menu[chosen_actuator], is_auto);
        while(pressed != BUTTON_1){ 
          if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){
            if(pressed == BUTTON_2){ // only toggle auto/manual if button 2 (down) is pressed
              is_auto = !is_auto;
              xQueueSendToBack(controllerQueue, &instruction, portMAX_DELAY);
              displayMode(actuator_menu[chosen_actuator], is_auto);
            }
          }
        }
        break;
      case(TOGGLE):
        //get current enable status from chosen driver
        bool enabled = NULL;
        if(chosen_actuator == FAN){
          enabled = get_fan_is_enabled();
        }else if(chosen_actuator == VENT){
          enabled = get_vent_is_enabled();
        }else if(chosen_actuator == LAMP){
          enabled = get_lamp_is_enabled();
        }else{
          ESP_LOGI(TAG, "Unhandled acutator ID in TOGGLE case");
        }
        displayToggle(actuator_menu[chosen_actuator], enabled);
        //query the respective driver for its mode 
        //send a message to the controller to update the output of of the given actuator 
        while(pressed != BUTTON_1){ 
          if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){
            if(pressed == BUTTON_2){ // only toggle on/off if button 2 (down) is pressed
              enabled = !enabled;
              xQueueSendToBack(controllerQueue, &instruction, portMAX_DELAY);
              displayToggle(actuator_menu[chosen_actuator], enabled);
            }
          }
        }
        break;
      case (ADJUST):
        displayAdjust(actuator_menu[chosen_action]);
        //send data to controller and the controller will start sampling
        xQueueSendToBack(controllerQueue, &instruction, portMAX_DELAY);
        while(pressed != BUTTON_1 && pressed != BUTTON_2){ 
          if(xQueueReceive(buttonQueue, &pressed, portMAX_DELAY) == pdTRUE){
            //send data to the controller again and it will stop sampling
            xQueueSendToBack(controllerQueue, &instruction, portMAX_DELAY);
          }
        }
        break;
      case (ACTION_NA):
        //do nothing, program should never trigger this
        break;
    }
  
    
  }
}

//processes data from UI and interfaces with controller task
void controller_task(void *parameters){
  ControllerMsg rec_instruct = {0};
  Actuator_Id cur_adjust = ACTUATOR_NA; // holds ID of whichever actuator potentiometers should be sent to
  while(1){
    if(xQueueReceive(controllerQueue, &rec_instruct, portMAX_DELAY) == pdTRUE){ // make sure queue item is recieved
      if(rec_instruct.sender_id == POTENTIOMETER){
        int percent = rec_instruct.pct;
        //shift extreme values to 0 or 100
        if(percent > 95){
          percent = 100;
        }else if(percent < 5){
          percent = 0;
        }
        set_level_indicator_from_pct(percent);
        switch(cur_adjust){
          case(FAN):
            fan_set_speed(percent);
            break;
          case(VENT):
            uint8_t angle = (percent*180)/100;
            vent_set_angle(angle);
            break;
          case(LAMP):
            lamp_set_brightness(percent);
            break;
          default:
            //do nothing
        }
      }else if(rec_instruct.sender_id == UI){ // detect a message from the UI
        switch(rec_instruct.action_id){ // switch based on which action the user took
          ///////// MODE SWITCH
          case(MODE):
            switch(rec_instruct.actuator_id){
              case(FAN):
                fan_toggle_auto();
                break;
              case(VENT):
                vent_toggle_auto();
                break;
              case(LAMP):
                lamp_toggle_auto();
                break;
              default:
            }            
            break;
          ///////// TOGGLE switch
          case(TOGGLE):
            switch(rec_instruct.actuator_id){
              case(FAN):
                fan_toggle_enabled();
                break;
              case(VENT):
                vent_toggle_enabled();
                break;
              case(LAMP):
                lamp_toggle_enabled();
                break;
              default:
            }            
            break;
          case(ADJUST):
            if(cur_adjust == ACTUATOR_NA){
              cur_adjust = rec_instruct.actuator_id;
              ESP_ERROR_CHECK(gptimer_start(pot_timer));
            }else{
              cur_adjust = ACTUATOR_NA;
              ESP_ERROR_CHECK(gptimer_stop(pot_timer));
              set_level_indicator(0);
            }

            break;
          default:
            ESP_LOGE(TAG, "Controller case unhandled.");
        } 
      }else if(rec_instruct.sender_id == PHOTORESISTOR){
        lamp_send_sensor_pct(rec_instruct.pct);
      }else if(rec_instruct.sender_id == TEMP_SENSOR){
        //do nothing
        vent_send_sensor_pct(rec_instruct.pct);
        fan_send_sensor_pct(rec_instruct.pct);
      }
    }
    
  }
}


void app_main() {
  start_up();

  //setup handles for queues
  buttonQueue = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(ButtonEvent));
  if(buttonQueue == NULL){
    ESP_LOGE(TAG, "Failed to create buttonQueue");
  }
  controllerQueue = xQueueCreate(CONTROLLER_QUEUE_LEN, sizeof(ControllerMsg));
  if(controllerQueue == NULL){
    ESP_LOGE(TAG, "Failed to create controllerQueue");
  }
  tempReadingQueue = xQueueCreate(1, sizeof(TempReading));
  if(tempReadingQueue == NULL){
    ESP_LOGE(TAG, "Failed to create tempReadingQueue");
  }
  wifiDataQueue = xQueueCreate(1, sizeof(WifiData));
  if(wifiDataQueue == NULL){
    ESP_LOGE(TAG, "Failed to create wifiDataQueue");
  }


  //create mutex
  adcMutex = xSemaphoreCreateMutex();
  if(adcMutex == NULL){
    ESP_LOGE(TAG, "Failed to create adcMutex");
  }

  ESP_LOGI(TAG, "Creating Tasks.");

  xTaskCreatePinnedToCore(
    controller_task,
    "Controller Task",
    2048,
    NULL,
    2,
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
    potentiometer_task,
    "Pot Read",
    4096,
    NULL,
    4,
    &potentiometerSampleTask,
    app_cpu
  );

  xTaskCreatePinnedToCore(
    read_temp_photo,
    "Read Temp and Photo",
    2048,
    NULL,
    3,
    &tempPhotoSampleTask,
    app_cpu
  );

  xTaskCreatePinnedToCore(
    wifi_task,
    "Get Temp from Wifi",
    8192,
    NULL,
    1,
    NULL,
    pro_cpu

  );

  setup_isrs();

  vTaskDelete(NULL);
}