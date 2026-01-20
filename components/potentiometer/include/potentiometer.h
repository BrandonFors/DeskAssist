#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <stdint.h>


void potentiometer_init();
int invert_reading(int raw);
int read_pot_vltg();
int read_pot_pct();

#endif