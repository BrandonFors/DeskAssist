#include "photoresistor.h"
#include "adc_manager.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"


#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <inttypes.h>

//max adc reading
#define MAX_READ 4095
//voltage that we consider to represent the midpoint between light and dark
//will have to be hand tuned
#define MID_VLTG_MV 2000 //in mv


//adc variables
static adc_channel_t adc_channel_6;

static char *TAG = "Photoresistor";

void photoresistor_init(){
  adc_channel_6 = ADC_CHANNEL_6;
  adc_manager_init();
  config_channel(adc_channel_6);
  
}

int read_photo_vltg(){
  int reading = read_vltg_from_channel(adc_channel_6);
  ESP_LOGI(TAG, "Photoresistor reads value of %dmV", reading);
  return reading;
}


//note that the actual max of the photoresistor in a voltage divider is unknown and hard to determine
//for this reason we will return 0 for light or 100 for dark
//the photoresistor is placed in series with a 10k resistor which means light => vltg reading up
int read_photo_light(){
  int vltg = read_photo_vltg();
  if(vltg >= MID_VLTG_MV){
    return 0; // 
  }else{
    return 100;
  }
}