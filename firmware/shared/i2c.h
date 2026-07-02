#ifndef I2C_H
#define I2C_H
#include <stdint.h>
#include <stdbool.h>
#include "hexpansion_header.h"

typedef uint8_t i2c_address_t;
typedef uint8_t i2c_page_address_t;
typedef uint8_t i2c_byte_address_t;

typedef struct page_definition {
  volatile uint8_t page_address;
  volatile uint8_t data_size;
  volatile void* volatile data;
} i2c_page_definition_t;

typedef void i2c_write_callback_t(i2c_page_address_t page_address, i2c_byte_address_t start_address, i2c_byte_address_t length); 
typedef void i2c_general_call_callback_t(uint8_t command_data); 

typedef struct i2c_config {
  i2c_address_t primary_address;
  i2c_page_address_t primary_num_pages;
  i2c_page_definition_t *primary_page_definitions;

  i2c_address_t secondary_address;
  volatile hexpansion_header_t* volatile secondary_header;
  volatile uint8_t* volatile secondary_fs;
  volatile uint16_t secondary_fs_size;

  i2c_write_callback_t *on_primary_write;
  i2c_write_callback_t *on_secondary_write;
  i2c_general_call_callback_t *on_general_call;
} i2c_config_t;

void i2c_setup(const i2c_config_t *config);
#endif