#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include <stdint.h>
#include "esp_adc/adc_oneshot.h"

void adc_manager_init();
void config_channel(adc_channel_t adc_channel);
int read_vltg_from_channel(adc_channel_t adc_channel);



#endif