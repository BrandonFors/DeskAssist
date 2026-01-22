#include "board.h"
#include "lamp.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include <inttypes.h>



//these values represent the min and max duty cycle when the lamp is in a usable state
#define MAX_LAMP_DUTY 200
#define MIN_LAMP_DUTY 115
#define TIMER_FREQ 250000 
#define SENSOR_THRESH 75

static ledc_timer_config_t timer_config;
static ledc_channel_config_t channel_config;
static const char *TAG = "Lamp";

static uint32_t current_duty;
static bool is_enabled = NULL;
static bool is_auto = NULL;
static bool auto_on = NULL;

void lamp_init(){
  // create a configuration for the timer of the ledc
  timer_config = (ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = TIMER_FREQ,
    .clk_cfg = LEDC_AUTO_CLK,
  };
  
  ESP_LOGI(TAG, "Configuring LEDC Timer");
  ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

  //create a configuration for the channel of the ledc
  channel_config = (ledc_channel_config_t){
    .gpio_num = LAMP_PIN, 
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0,

  };
  
  ESP_LOGI(TAG, "Configuring LEDC Channel");
  ESP_ERROR_CHECK(ledc_channel_config(&channel_config));

  current_duty = 0;
  is_enabled = true;
  is_auto = false;

  // be sure to run ledc_fade_func_install(0); in main
  //this allows the ledc to transition between duty cycle values smoothly

}

void update_lamp_duty(uint32_t duty){
  ESP_LOGI(TAG, "Lamp set to %" PRIu32 " duty.", duty);
  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty, 0));
}

//set brightness using a int from 0-100 (%)
void lamp_set_brightness(uint8_t percent){
  if(percent > 100){
    percent = 100;
  }
  current_duty = MIN_LAMP_DUTY + (MAX_LAMP_DUTY - MIN_LAMP_DUTY)*(percent/100.0);
  if(current_duty < MIN_LAMP_DUTY + 5){
    current_duty = 0; // turn bulb off if the the duty is close to the min
  }
  if(is_enabled && (auto_on || !is_auto)){
    update_lamp_duty(current_duty);
  }

}

bool get_lamp_is_auto(){
  return is_auto;
}

bool get_lamp_is_enabled(){
  return is_enabled;
}

void lamp_toggle_auto(){
  is_auto = !is_auto;
  //if auto was switched off and the lamp was off, turn the lamp back to user defined value
  if(!is_auto && !auto_on){ 
    update_lamp_duty(current_duty);
  }
}

void lamp_toggle_enabled(){
  is_enabled = !is_enabled;
  if(is_enabled){
    ESP_LOGI(TAG, "Lamp has been enabled");
    //either the device has automatically been switched on or automatic mode is off
    if(auto_on || !is_auto){
      update_lamp_duty(current_duty); //update the last duty that was set by the user to the driver
    }
  }else{
    ESP_LOGI(TAG, "Lamp has been disabled");
    update_lamp_duty(0); // push duty directly to avoid overwriting user set duty
  }
}

//turn lamp on from the stored duty cycle
void lamp_on(){
  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, current_duty, 0));
  ESP_LOGI(TAG, "Lamp set to %" PRIu32 " duty.", current_duty);
  
}

//turn lamp off by setting duty to 0
void lamp_off(){
  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 0));
  ESP_LOGI(TAG, "Lamp set to 0 duty.");
}


// if the sent in percentage is greater than the set threshold, turn the fan on, otherwise off
void lamp_send_sensor_pct(uint8_t sensor_pct){
  if((sensor_pct >= SENSOR_THRESH) != auto_on){
    auto_on = !auto_on;
  }
  if(is_auto){ // if auto is enabled for the device
    if(auto_on){ // if the sensor threshold was reached
      update_lamp_duty(current_duty);
    }else{
      update_lamp_duty(0);
    } 
  }
  
}