
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <inttypes.h>

//number of samples adc uses to calculate an average adc reading
#define NUM_SAMPLES 10

static adc_oneshot_unit_init_cfg_t init_config;
static adc_oneshot_chan_cfg_t channel_config;
static adc_cali_line_fitting_config_t cali_config;
static adc_cali_handle_t adc_cali_handle;
static adc_oneshot_unit_handle_t adc_handle;

bool initialized = false;

static char *TAG = "ADC MANAGER";

void adc_manager_init(){
  if(!initialized){
    //configuring all adc channels to unit 1 for simplicity
    init_config = (adc_oneshot_unit_init_cfg_t){
      .unit_id = ADC_UNIT_1,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_LOGI(TAG, "Creating ADC Oneshot Unit");
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    
    //config used for all channels
    //atten db 12 used for all channels for simplicity
    channel_config = (adc_oneshot_chan_cfg_t){
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = ADC_ATTEN_DB_12,
    };

    cali_config = (adc_cali_line_fitting_config_t){
      .unit_id = ADC_UNIT_1,
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_LOGI(TAG, "Creating Calibration Scheme.");
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));
    
    initialized = true;
  }

}

void config_channel(adc_channel_t adc_channel){
  ESP_LOGI(TAG, "Creating ADC Oneshot Channel");
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &channel_config));
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


int read_vltg_from_channel(adc_channel_t adc_channel){
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
  return reading;
}