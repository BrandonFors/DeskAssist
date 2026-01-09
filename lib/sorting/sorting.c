#include "sorting.h"

//sorting algorithm to be used on samples
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