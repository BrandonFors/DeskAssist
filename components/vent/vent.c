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
#define MIN_PULSE_MS 575 // min pulse in us
#define MAX_PULSE_MS 2600 // max pulse in us
#define MAX_DUTY 4096 // max duty cycle for 12 bit duty resolution

static ledc_timer_config_t timer_config;
static ledc_channel_config_t channel_config;
static const char *TAG = "Vent Servo";

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
  //fade func should only be called once and is also installed by lamp.c
  // ESP_ERROR_CHECK(ledc_fade_func_install(0));

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

void vent_set_angle(uint8_t angle){
  uint32_t new_duty = angle_to_duty(angle);
  ESP_LOGI(TAG, "Servo set to %" PRIu32 " duty.", new_duty);

  ESP_ERROR_CHECK(ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, new_duty, 0));


}