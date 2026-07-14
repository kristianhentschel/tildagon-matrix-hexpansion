/*
 * I2C slave device for matrix hexpansion
 * - primary slave: multiple pages, selected by writing a page address to register 0xFD
 * - secondary slave: eeprom emulation with 16-bit register address
 * 
 * Based on Single-File-Header for using the I2C peripheral in slave mode:
 *
 * MIT License
 *
 * Copyright (c) 2024 Renze Nicolai
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "i2c_again.h"

struct _i2c_slave_state {
  volatile uint16_t offset;
  volatile uint16_t position;
  volatile uint8_t* volatile registers1;
  volatile uint8_t size1;
  volatile uint8_t* volatile header2;
  volatile uint8_t header_size2;
  volatile uint16_t registers_offset2;
  volatile uint8_t* volatile registers2;
  volatile uint16_t size2;
  bool first_write;
  bool second_write;
  bool read_only1;
  bool read_only2;
  bool writing;
  bool address2matched;
} i2c_slave_state;

void SetupI2CSlave(uint8_t address, volatile uint8_t* registers, uint8_t size, bool read_only) {
  i2c_slave_state.first_write = 1;
  i2c_slave_state.second_write = 0;
  i2c_slave_state.offset = 0;
  i2c_slave_state.position = 0;
  i2c_slave_state.registers1 = registers;
  i2c_slave_state.size1 = size;
  i2c_slave_state.registers2 = NULL;
  i2c_slave_state.size2 = 0;
  i2c_slave_state.read_only1 = read_only;
  i2c_slave_state.read_only2 = false;

  // Enable I2C1
  RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

  // Reset I2C1 to init all regs
  #ifdef CH32V003
    #warning "CH32V003"
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;
  #else // CH32V00x
    RCC->PB1PCENR |= RCC_I2C1EN;
    RCC->PB1PRSTR |= RCC_I2C1RST;
    RCC->PB1PRSTR &= ~RCC_I2C1RST;
  #endif

  I2C1->CTLR1 |= I2C_CTLR1_SWRST;
  I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;

  // Set module clock frequency
  uint32_t prerate = 2000000; // I2C Logic clock rate, must be higher than the bus clock rate
  I2C1->CTLR2 |= (FUNCONF_SYSTEM_CORE_CLOCK/prerate) & I2C_CTLR2_FREQ;

  // Enable interrupts
  I2C1->CTLR2 |= I2C_CTLR2_ITBUFEN | I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITERREN;

  NVIC_EnableIRQ(I2C1_EV_IRQn); // Event interrupt
  NVIC_SetPriority(I2C1_EV_IRQn, 2 << 4);
  NVIC_EnableIRQ(I2C1_ER_IRQn); // Error interrupt
  NVIC_SetPriority(I2C1_ER_IRQn, 2 << 4);

  // Set clock configuration
  uint32_t clockrate = 1000000; // I2C Bus clock rate, must be lower than the logic clock rate
  I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK/(3*clockrate))&I2C_CKCFGR_CCR) | I2C_CKCFGR_FS; // Fast mode 33% duty cycle
  //I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK/(25*clockrate))&I2C_CKCFGR_CCR) | I2C_CKCFGR_DUTY | I2C_CKCFGR_FS; // Fast mode 36% duty cycle
  //I2C1->CKCFGR = (FUNCONF_SYSTEM_CORE_CLOCK/(2*clockrate))&I2C_CKCFGR_CCR; // Standard mode good to 100kHz

  // Set I2C address
  I2C1->OADDR1 = address << 1;
  I2C1->OADDR2 = 0;

  // Enable I2C
  I2C1->CTLR1 |= I2C_CTLR1_PE;

  // Acknowledge bytes when they are received
  I2C1->CTLR1 |= I2C_CTLR1_ACK;
}

void SetupSecondaryI2CSlave(uint8_t address, volatile hexpansion_header_t* header, uint8_t header_size, uint16_t registers_offset, volatile uint8_t* registers, uint16_t size,  bool read_only) {
  if (address > 0) {
    I2C1->OADDR2 = (address << 1) | 1;
    i2c_slave_state.registers_offset2 = registers_offset;
    i2c_slave_state.registers2 = registers;
    i2c_slave_state.size2 = size;
    i2c_slave_state.read_only2 = read_only;
    i2c_slave_state.header2 = (uint8_t*)header;
    i2c_slave_state.header_size2 = header_size;
  } else {
    I2C1->OADDR2 = 0;
  }
}

void I2C1_EV_IRQHandler(void) __attribute__((interrupt));
void I2C1_EV_IRQHandler(void) {
  uint16_t STAR1, STAR2 __attribute__((unused));
  uint8_t DATAR __attribute__((unused));
  STAR1 = I2C1->STAR1;
  STAR2 = I2C1->STAR2;

  // printf("I2C status %08x %08x\n", STAR1);
  DATAR = I2C1->DATAR;

  if (STAR1 & I2C_STAR1_ADDR) { // Start event
    i2c_slave_state.first_write = 1; // Next write will be the offset
    i2c_slave_state.position = i2c_slave_state.offset; // Reset position
    i2c_slave_state.address2matched = !!(STAR2 & I2C_STAR2_DUALF);
  }

  if (STAR1 & I2C_STAR1_RXNE) { // Write event
    if (i2c_slave_state.first_write) { // First byte written, set the offset
      i2c_slave_state.offset = DATAR;
      i2c_slave_state.position = i2c_slave_state.offset;
      i2c_slave_state.first_write = 0;

      if (i2c_slave_state.address2matched) {
        i2c_slave_state.second_write = 1; // need to wait for the second byte of offset
      } else {
        i2c_slave_state.writing = false;
      }
    } else if (i2c_slave_state.second_write) { // Second byte written, set the lower byte of offset (for secondary device only)
      i2c_slave_state.offset <<= 8;
      i2c_slave_state.offset |= DATAR;
      i2c_slave_state.position = i2c_slave_state.offset;
      // printf("second write, offset = %d\n", i2c_slave_state.offset);

      i2c_slave_state.second_write = 0;
      i2c_slave_state.writing = false;
    } else { // Normal register write
      i2c_slave_state.writing = true;
      if (i2c_slave_state.address2matched) {
        if (i2c_slave_state.position < i2c_slave_state.size2 && !i2c_slave_state.read_only2) {
          i2c_slave_state.registers2[i2c_slave_state.position] = DATAR;
          i2c_slave_state.position++;
        }
      } else {
        // TODO primary device needs to change page after writing the command register at 0xfd (for applications needing multiple pages)
        if (i2c_slave_state.position < i2c_slave_state.size1 && !i2c_slave_state.read_only1) {
          i2c_slave_state.registers1[i2c_slave_state.position] = DATAR;
          i2c_slave_state.position++;
        }
      }
    }
  }

  if (STAR1 & I2C_STAR1_TXE) { // Read event
    i2c_slave_state.writing = false;
    if (i2c_slave_state.address2matched) {
      if (i2c_slave_state.position < i2c_slave_state.header_size2) {
        I2C1->DATAR = i2c_slave_state.header2[i2c_slave_state.position];
        i2c_slave_state.position++;
      } else if (i2c_slave_state.position < i2c_slave_state.registers_offset2 + i2c_slave_state.size2) { // file system does not include header offset blank space
        I2C1->DATAR = i2c_slave_state.registers2[i2c_slave_state.position - i2c_slave_state.registers_offset2];
        i2c_slave_state.position++;
      } else {
        I2C1->DATAR = 0x00;
      }
    } else {
      if (i2c_slave_state.position < i2c_slave_state.size1) {
        I2C1->DATAR = i2c_slave_state.registers1[i2c_slave_state.position];
        i2c_slave_state.position++;
      } else {
        I2C1->DATAR = 0x00;
      }
    }
  }

  if (STAR1 & I2C_STAR1_STOPF) { // Stop event
    I2C1->CTLR1 &= ~(I2C_CTLR1_STOP); // Clear stop
  }
}

void I2C1_ER_IRQHandler(void) __attribute__((interrupt));
void I2C1_ER_IRQHandler(void) {
  uint16_t STAR1 = I2C1->STAR1;

  if (STAR1 & I2C_STAR1_BERR) { // Bus error
    I2C1->STAR1 &= ~(I2C_STAR1_BERR); // Clear error
  }

  if (STAR1 & I2C_STAR1_ARLO) { // Arbitration lost error
    I2C1->STAR1 &= ~(I2C_STAR1_ARLO); // Clear error
  }

  if (STAR1 & I2C_STAR1_AF) { // Acknowledge failure
    I2C1->STAR1 &= ~(I2C_STAR1_AF); // Clear error
  }
}

void i2c_setup(const i2c_config_t* config) {
  // TODO: primary multiple pages and command register page selection
  if (config->primary_num_pages > 0) {
    i2c_page_definition_t page = config->primary_page_definitions[0];
    SetupI2CSlave(config->primary_address, page.data, page.data_size, false);
  }

  SetupSecondaryI2CSlave(
    config->secondary_address,
    config->secondary_header, sizeof(hexpansion_header_t),
    config->secondary_header->filesystem_info.offset, config->secondary_fs, config->secondary_fs_size,
    true);
}