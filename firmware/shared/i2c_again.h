#ifndef __I2C_AGAIN_H
#define __I2C_AGAIN_H

#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "hexpansion_header.h"

typedef struct page_definition {
  uint8_t page_address;
  uint8_t data_size;
  volatile uint8_t* volatile data;
} i2c_page_definition_t;

typedef struct i2c_config {
  uint8_t primary_address;
  uint8_t primary_num_pages;
  i2c_page_definition_t *primary_page_definitions;

  uint8_t secondary_address;
  volatile hexpansion_header_t* volatile secondary_header;
  volatile uint8_t* volatile secondary_fs;
  volatile uint16_t secondary_fs_size;
} i2c_config_t;

void i2c_setup(const i2c_config_t* config);
#endif
