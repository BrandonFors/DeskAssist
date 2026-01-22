#include "temp_sensor.h"
#include "adc_manager.h"
#include "esp_adc/adc_oneshot.h"

#include "esp_err.h"
#include "esp_log.h"


//max voltage that the temp sensor will output
#define MAX_VLTG_MV 2000


static char *TAG = "Temp Sensor";
adc_channel_t adc_channel_5;

void temp_sensor_init(){
  adc_channel_5 = ADC_CHANNEL_5;
  adc_manager_init();
  config_channel(adc_channel_5);

}

int read_tmp_vltg(){
  int reading = read_vltg_from_channel(adc_channel_5);
  ESP_LOGI(TAG, "Temp sensor reads value of %dmV", reading);
  return reading;
}

//returns the reading in temperature
int read_temp_deg(){
  int reading = read_tmp_vltg();
  return (reading - 500)/10;
}


int read_temp_pct(){
  int pct = (read_tmp_vltg()*100)/MAX_VLTG_MV;
  return pct;
}