#include "fan_motor.h"
#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <stdint.h>

//tuned to useable values
#define MAX_FAN_DUTY 255
#define MIN_FAN_DUTY 90
#define TIMER_FREQ 20000
static ledc_timer_config_t timer_config;
static ledc_channel_config_t channel_config;
static const char *TAG = "Fan";

static uint32_t current_duty;

bool is_enabled = NULL;




void fan_init(){
  // create a configuration for the timer of the ledc
  timer_config = (ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_2,
    .freq_hz = TIMER_FREQ,
    .clk_cfg = LEDC_AUTO_CLK,
  };
  
  ESP_LOGI(TAG, "Configuring LEDC Timer");
  ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

  //create a configuration for the channel of the ledc
  channel_config = (ledc_channel_config_t){
    .gpio_num = MOTOR_PIN, 
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_2,
    .timer_sel = LEDC_TIMER_2,
    .duty = 0,
    .hpoint = 0,
  };
  
  ESP_LOGI(TAG, "Configuring LEDC Channel");
  ESP_ERROR_CHECK(ledc_channel_config(&channel_config));

  current_duty = 0;

  is_enabled = true;

  // be sure to run ledc_fade_func_install(0); in main 
  //this allows the ledc to transition between duty cycle values smoothly
}

void update_fan_duty(uint32_t duty){
  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty, 0));
  ESP_LOGI(TAG, "Fan set to %" PRIu32 " duty.", duty);
}

//allow for the fan speed to be set with a integer value 0-100
void fan_set_speed(uint8_t percent){
  //limits percent to 100, a uint8_t can never be below 0
  if(percent > 100){
    percent = 100;
  }
  current_duty = MIN_FAN_DUTY + (MAX_FAN_DUTY - MIN_FAN_DUTY)*(percent/100.0);
  if(current_duty < MIN_FAN_DUTY+5){
    current_duty = 0; //set duty to 0 (off) if the dial is close to its lowest position
  }
  if(is_enabled){
    update_fan_duty(current_duty);
  }

}

bool get_fan_is_enabled(){
  return is_enabled;
}

void fan_toggle_enabled(){
  is_enabled = !is_enabled;
  if(is_enabled){
    ESP_LOGI(TAG, "Fan has been enabled");
    update_fan_duty(current_duty); //update the last duty that was set by the user to the driver
  }else{
    ESP_LOGI(TAG, "Fan has been disabled");
    update_duty(0); // push duty directly to avoid overwriting user set duty
  }
}

void fan_on(){
  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, current_duty, 0));
  ESP_LOGI(TAG, "Fan set to %" PRIu32 " duty.", current_duty);
}

void fan_off(){
  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0, 0));
  ESP_LOGI(TAG, "Fan set to 0 duty.");
}