#include "level_indicator.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "board.h"


// for spi interaction with the ic
static spi_device_handle_t spi_handle;
static spi_bus_config_t spi_config;
static spi_transaction_t t;
static spi_device_interface_config_t spi_device_config;
static spi_transaction_t t;

static const char *TAG = "Level Indicator";

static uint8_t sendbuf;


void indicator_init(){
  gpio_set_direction(LATCH, GPIO_MODE_OUTPUT);

  gpio_set_direction(LATCH, GPIO_MODE_OUTPUT);


  spi_config = (spi_bus_config_t){ //configure for spi communcation for led row
    .mosi_io_num = MOSI,
    .miso_io_num = -1,
    .sclk_io_num = SCLK,
  };

  ESP_LOGI(TAG, "Initializing SPI Bus");
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_config, SPI_DMA_CH_AUTO));


  spi_device_config = (spi_device_interface_config_t){
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = -1, // handling CS manually (just connected to 5V on board)
    .queue_size = 1,
  };
  
  ESP_LOGI(TAG, "Adding SPI Device");
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &spi_device_config, &spi_handle));

  t = (spi_transaction_t){
    .length = 8,
    .tx_buffer = &sendbuf,
    .rx_buffer = NULL,
  };
}

void set_level(int lvl){
  sendbuf = 0b00000000;

  for(int i = 0; i < lvl; i++){
    unsigned int mask = 1 << i;
    sendbuf ^= mask;
  }
  
  gpio_set_level(LATCH, 0);
  spi_device_transmit(spi_handle, &t);
  gpio_set_level(LATCH, 1);

}