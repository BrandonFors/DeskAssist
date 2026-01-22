#include "board.h"
#include "vent.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include <inttypes.h>

//the values below are assigned based on SG90 datasheet
#define PWM_PERIOD_HZ 50 // pwm period in HZ
#define PWM_PERIOD_MS 20000 //pwm period in ms
//the following two values were tuned to my servo specifically
#define MIN_PULSE_MS 600 // min pulse in us
#define MAX_PULSE_MS 2700 // max pulse in us
#define MAX_DUTY 4096 // max duty cycle for 12 bit duty resolution

#define SENSOR_THRESH 39


static ledc_timer_config_t timer_config;
static ledc_channel_config_t channel_config;
static const char *TAG = "Vent Servo";
static uint32_t current_duty;

static bool is_enabled = NULL;
static bool is_auto = NULL;
static bool auto_on = NULL;

void vent_init(){
  timer_config = (ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_12_BIT,
    .timer_num = LEDC_TIMER_1,
    .freq_hz = PWM_PERIOD_HZ, // 50hz
    .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_LOGI(TAG, "Configuring LEDC Timer");
  ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

  //create a configuration for the channel of the ledc
  channel_config = (ledc_channel_config_t){
    .gpio_num = VENT_PIN, 
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_1,
    .timer_sel = LEDC_TIMER_1,
    .duty = angle_to_duty(0),
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

uint32_t angle_to_duty(uint8_t angle){
  if (angle > 180){
    angle = 180;
  }
  //convert the angle into the pulse value for that angle
  uint32_t pulse = MIN_PULSE_MS + (uint32_t)angle*(MAX_PULSE_MS-MIN_PULSE_MS)/180; 

  //return the duty cycle for the angle
  return pulse*MAX_DUTY/PWM_PERIOD_MS;
}

//sends duty to the ledc driver and updates the value output
void update_vent_duty(uint32_t duty){
  ESP_LOGI(TAG, "Servo set to %" PRIu32 " duty.", duty);
  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty, 0));
}

// fxn for manual control by user
void vent_set_angle(uint8_t angle){ 
  current_duty = angle_to_duty(angle);
  //only updates input if conditions for an active vent are met
  //ie vent is enabled and either automatically turned on or auto mode is turned off
  if(is_enabled && (auto_on || !is_auto)){
    update_vent_duty(current_duty);
  }
}

//auto functionality tbd 



bool get_vent_is_auto(){
  return is_auto;
}

bool get_vent_is_enabled(){
  return is_enabled;
}

void vent_toggle_auto(){
  is_auto = !is_auto;
  //if auto was switched off and the vent was off, turn the vent back to user defined value
  if(!is_auto && !auto_on){ 
    update_vent_duty(current_duty);
  }
}

void vent_toggle_enabled(){
  is_enabled = !is_enabled;
  if(is_enabled){
    ESP_LOGI(TAG, "Vent has been enabled");
    update_vent_duty(current_duty); //update the last duty that was set by the user to the driver
  }else{
    ESP_LOGI(TAG, "Vent has been disabled");
    uint8_t duty = angle_to_duty(0); //get duty for angle 0 (closed position)
    update_vent_duty(duty); // push duty directly to avoid overwriting user set duty
  }
}

// if the sent in percentage is greater than the set threshold, turn the fan on, otherwise off
void vent_send_sensor_pct(uint8_t sensor_pct){
  ESP_LOGI(TAG, "Given %d", sensor_pct);
  if((sensor_pct >= SENSOR_THRESH) != auto_on){
    auto_on = !auto_on;
  }
  if(is_auto){ // if auto is enabled for the device
    if(auto_on){ // if the sensor threshold was reached
      update_vent_duty(current_duty);
    }else{
      update_vent_duty(angle_to_duty(0));
    } 
  }
  
}