#ifndef VENT_H
#define VENT_H

#include <stdint.h>

void vent_init();
uint32_t angle_to_duty(uint8_t angle);
void vent_set_angle(uint8_t angle);


#endif