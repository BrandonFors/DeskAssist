#ifndef RTOS_SETUP_H
#define RTOS_SETUP_H

#include "freertos/FreeRTOS.h"

#define BUTTON_QUEUE_LEN 1

#define DEBOUNCE_TIME_MS 250

// enums correlate with menu items
typedef enum {
  FAN = 0,
  VENT = 1,
  LAMP = 2,
  ACTUATOR_NA = 3,
} Actuator_Id;

typedef enum {
  MODE = 0,
  TOGGLE = 1,
  ADJUST = 2,
  ACTION_NA = 3,
} Action_Id;

typedef struct {
  Action_Id action_id;
  Actuator_Id actuator_id;
} Controller_Msg;

typedef struct {
  
} ActuatorState;



//queue handles

QueueHandle_t buttonQueue;


//Task Handles 

TaskHandle_t userInterfaceTask;


void gpio_isr_handler(void* arg);

#endif