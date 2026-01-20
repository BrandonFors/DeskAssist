#include "potentiometer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <inttypes.h>

//max adc reading
#define MAX_READ 4095
//max voltage that the potentiometer can read on wipper in mv
#define MAX_VLTG_MV 877
//number of samples adc uses to calculate an average adc reading
#define NUM_SAMPLES 10

static adc_channel_t adc_channel;
static adc_oneshot_unit_handle_t adc_handle;
static adc_oneshot_unit_init_cfg_t init_config;
static adc_oneshot_chan_cfg_t channel_config;
static adc_cali_line_fitting_config_t cali_config;
static adc_cali_handle_t adc_cali_handle;



static char *TAG = "Potentiometer";

void potentiometer_init(){
  adc_channel = ADC_CHANNEL_0;

  init_config = (adc_oneshot_unit_init_cfg_t){
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

  ESP_LOGI(TAG, "Creating ADC Oneshot Unit");
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
  
  channel_config = (adc_oneshot_chan_cfg_t){
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_0,
  };
  ESP_LOGI(TAG, "Creating ADC Oneshot Channel");
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &channel_config));

  cali_config = (adc_cali_line_fitting_config_t){
    .unit_id = ADC_UNIT_1,
    .atten = ADC_ATTEN_DB_0,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  ESP_LOGI(TAG, "Creating Calibration Scheme.");
  ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));


}

// In my setup, the potentiomter reads in a way that is counter intuitive
//I want the potentiometer to output higher values when turrned to the right so i use this fxn
int invert_reading(int vltg){
  return MAX_VLTG_MV-vltg;
}

void insertion_sort(int samples[], int arr_len){
  for(int i = 1; i < arr_len; i++ ){
    int j = i-1;
    int temp = samples[i];
    while((j >= 0) && (samples[j] > temp)){
      samples[j+1] = samples[j];
      j--;
    }
    samples[j+1] = temp;
  }
}

//returns the raw reading of the potentiometer
int read_pot_vltg(){
  int samples[NUM_SAMPLES];
  int reading = 0;
  for(int i = 0; i<NUM_SAMPLES; i++){
    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, adc_channel, &raw)); // sample the adc
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw, &samples[i])); //converts the raw value to a calibrated voltage
  }
  //sort the samples and drop the outer four
  insertion_sort(samples, NUM_SAMPLES);
  for(int i = 2; i < NUM_SAMPLES - 2; i++){
    reading += samples[i];
  }
  reading /= (NUM_SAMPLES - 4);
  //invert the voltage reading 
  reading = invert_reading(reading);

  return reading;
}

//returns the adc reading as a percentage of the max
int read_pot_pct(){
  int pct = (read_pot_vltg()*100)/MAX_VLTG_MV;
  return pct;
}


