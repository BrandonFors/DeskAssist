#include "potentiometer.h"
#include "adc_manager.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gptimer.h"

#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <inttypes.h>

//max adc reading
#define MAX_READ 4095
//max voltage that the potentiometer can read on wipper in mv
#define MAX_VLTG_MV 3300

//adc variables
static adc_channel_t adc_channel_4;


gptimer_handle_t pot_timer;

static char *TAG = "Potentiometer";

void potentiometer_init(){
  adc_channel_4 = ADC_CHANNEL_4;
  adc_manager_init();
  config_channel(adc_channel_4);

  //set up gptimer for interrupts
  gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1000000,
    .intr_priority = 0,
  };
  //create timer instance
  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &pot_timer));

  gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,
    .alarm_count = 1000000/4, // the first digit is the resolution: divide by desired sampling Hz (make sure result is an int)
    .flags.auto_reload_on_alarm = true,
  };
  //set the timer's alarm action
  ESP_ERROR_CHECK(gptimer_set_alarm_action(pot_timer, &alarm_config));

  // register event callbacks in main.c
}

// In my setup, the potentiomter reads in a way that is counter intuitive
//I want the potentiometer to output higher values when turrned to the right so i use this fxn
int invert_reading(int vltg){
  return MAX_VLTG_MV-vltg;
}



//returns the raw reading of the potentiometer
int read_pot_vltg(){
  int reading = read_vltg_from_channel(adc_channel_4);
  ESP_LOGI(TAG, "Photentiometer reads value of %dmV", reading);

  //invert the voltage reading 
  reading = invert_reading(reading);

  return reading;
}

//returns the adc reading as a percentage of the max
int read_pot_pct(){
  int pct = (read_pot_vltg()*100)/MAX_VLTG_MV;
  return pct;
}


