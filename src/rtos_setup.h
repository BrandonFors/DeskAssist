#ifndef RTOS_SETUP_H
#define RTOS_SETUP_H

#include "freertos/FreeRTOS.h"

#define BUTTON_QUEUE_LEN 10


enum ACTUATOR{
  FAN,
  VENT,
  LAMP
};

enum COMMAND{
  MODE,
  TOGGLE,
  SAMPLE_POT, // sampling from potentiometer
  SAMPLE_BUT,
  STOP_SAMPLE,
};

//queue handles

QueueHandle_t buttonQueue;

#endif