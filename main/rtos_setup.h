#ifndef RTOS_SETUP_H
#define RTOS_SETUP_H

#include "freertos/FreeRTOS.h"

#define BUTTON_QUEUE_LEN 1
#define CONTROLLER_QUEUE_LEN 10

#define DEBOUNCE_TIME_MS 250

// enums correlate with menu items
typedef enum {
  FAN = 0,
  VENT = 1,
  LAMP = 2,
  LEVEL = 3,
  ACTUATOR_NA = 4,
} Actuator_Id;

typedef enum {
  UI = 0,
  CONTROLLER = 1,
  POTENTIOMETER = 2,
  PHOTORESISTOR = 3,
  TEMP_SENSOR = 4,
} Sender_Id;


//struct used to 
typedef enum {
  MODE = 0,
  TOGGLE = 1,
  ADJUST = 2,
  ACTION_NA = 3,
} Action_Id;

typedef struct {
  Actuator_Id actuator_id; 
  Action_Id action_id; 
  Sender_Id sender_id; 
  int pct; //used by ADC tasks
} ControllerMsg;

typedef struct{
  int temp;  
} TempReading;

typedef struct{
  int temp;
  char time[10];
} WifiData;


void gpio_isr_handler(void* arg);

#endif