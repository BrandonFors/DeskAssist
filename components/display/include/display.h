#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H
#include <stdbool.h>


typedef struct {
  char *name;
  bool selected;
} MenuItem;


void display_init();

void homeScreen();

void displayMenu(MenuItem menu[], int menu_len);

void displayAdjust(MenuItem item);

#endif