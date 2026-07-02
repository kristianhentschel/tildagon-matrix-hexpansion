#include "i2c.h"
#include "ch32fun.h"
#include <stdbool.h>
#include <stdio.h>

#define DEBUG_I2C

// The configuration is mostly based on ch32fun/examples/i2c_slave
// MIT Licence (c) 2024 Renze Nicolai

// Primary device:
// approximately follows IS31FL3731 interface, selecting a 'page' by writing a byte to the command register 0xFD
// then addressing within the current page with a one-byte address at the start of a write.

// Secondary device
// emulates a ZD24C64A eeprom interface, addressing a 13-bit byte offset with the first two bytes in a write transfer

static const i2c_config_t *g_config;

void i2c_setup(const i2c_config_t *config) {
  if (g_config != NULL) {
    return;
  }
  g_config = config;

  // Configure GPIO pins in main code:
  // funPinMode(PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SDA
  // funPinMode(PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SCL

  // Enable and reset I2C1 peripheral through RCC module
  #ifdef CH32V003
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

  // Set logic clock frequency; must be higher than bus clock
  uint32_t prerate = 2000000;
  I2C1->CTLR2 |= (FUNCONF_SYSTEM_CORE_CLOCK / prerate) & I2C_CTLR2_FREQ;

  // Enable interrupts
  I2C1->CTLR2 |= I2C_CTLR2_ITBUFEN | I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITERREN;

  NVIC_EnableIRQ(I2C1_EV_IRQn);
  NVIC_SetPriority(I2C1_EV_IRQn, 2<<4);
  NVIC_EnableIRQ(I2C1_ER_IRQn);
  NVIC_SetPriority(I2C1_ER_IRQn, 2<<4);

  // Set bus clock configuration
  uint32_t clockrate = 1000000; // I2C Bus clock rate, must be lower than the logic clock rate
  I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK/(3*clockrate))&I2C_CKCFGR_CCR) | I2C_CKCFGR_FS; // Fast mode 33% duty cycle
  //I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK/(25*clockrate))&I2C_CKCFGR_CCR) | I2C_CKCFGR_DUTY | I2C_CKCFGR_FS; // Fast mode 36% duty cycle
  //I2C1->CKCFGR = (FUNCONF_SYSTEM_CORE_CLOCK/(2*clockrate))&I2C_CKCFGR_CCR; // Standard mode good to 100kHz

  // Address registers; assumes both addresses are 7 bit
  // (otherwise need to also set address length bit in oaddr1)
  I2C1->OADDR1 = config->primary_address << 1;
  I2C1->OADDR2 = 0;
  if (config->secondary_address != 0) {
    I2C1->OADDR2 = (config->secondary_address << 1) | I2C_OADDR2_ENDUAL;
  }

  // TODO enable general call address?

  // Enable peripheral
  I2C1->CTLR1 |= I2C_CTLR1_PE;

  // Acknowledge bytes when they are received
  I2C1->CTLR1 |= I2C_CTLR1_ACK;

  // TODO without at least this print the I2C device is not recognised by hex launcher on I2C scan.
  printf("I2C enabled.\n");
}

static struct i2c_state {
  bool secondary;

  // state for primary device
  // this works on pages and registers, with register write to 0xFD changing the page index
  i2c_page_definition_t *primary_page;
  uint8_t primary_register_address;
  bool primary_register_address_received;

  // state for secondary device
  uint16_t secondary_byte_address;
  bool secondary_byte_address_high_received;
  bool secondary_byte_address_low_received;
} g_state; 

void I2C1_EV_IRQHandler(void) __attribute__((interrupt)) __attribute__((section(".srodata")));
void I2C1_EV_IRQHandler(void) {
  uint16_t STAR1, STAR2 __attribute__((unused));
  uint8_t DATAR __attribute__((unused));

  STAR1 = I2C1->STAR1;
  STAR2 = I2C1->STAR2;

  #ifdef DEBUG_I2C
  printf("                                         I2C event STAR1 %04x STAR2 %04x --\n", STAR1, STAR2);
  #endif

  if (STAR1 & I2C_STAR1_ADDR) {
    // Start event
    
    // Determine which device address matched
    // TODO handle general call address?
    if (STAR2 & I2C_STAR2_DUALF) {
      // Secondary device address matched
      g_state.secondary = 1;

      // try reading the second address byte from data register if in transfer more
      // hopefully it's still in there
     if (g_state.secondary_byte_address_high_received && !g_state.secondary_byte_address_low_received) {
       g_state.secondary_byte_address |= I2C1->DATAR;
       g_state.secondary_byte_address_low_received = 1;
       #ifdef DEBUG_I2C
       printf("secondary byte address captured in ADDR: %d\n", g_state.secondary_byte_address);
       #endif
     } else {
       // Secondary device always has two address bytes in a write transfer.
       // A read starts from the last written or incremented address.
       g_state.secondary_byte_address_high_received = 0;
       g_state.secondary_byte_address_low_received = 0;
     }

      #ifdef DEBUG_I2C
      printf("I2C start secondary.\n");
      #endif
    } else {
      // Primary address matched
      g_state.secondary = 0;

      // Primary device uses a one-byte address at the start of a write transfer.
      g_state.primary_register_address_received = 0;

      #ifdef DEBUG_I2C
      printf("I2C start primary.\n");
      #endif
    }
  }

  if (STAR1 & I2C_STAR1_RXNE) {
    DATAR = I2C1->DATAR; // always read the data register to clear RXNE, even if the write is later ignored

    if (g_state.secondary) {
      #ifdef DEBUG_I2C
      printf(" RXNE secondary (current byte address %d) <- 0x%02x\n", g_state.secondary_byte_address, DATAR);
      #endif
      
      // Secondary write event
      // Only capture addresses and ignore any other write attempts as header and fs is burnt into the image
      if (!g_state.secondary_byte_address_high_received) {
        // Capture the high five bits of the byte address
        g_state.secondary_byte_address = (DATAR & 0b00011111) << 8;
        g_state.secondary_byte_address_high_received = 1;

        #ifdef DEBUG_I2C
        printf("    received high address byte, 0x%02x, full address %d\n", DATAR, g_state.secondary_byte_address);
        #endif
      } else if (!g_state.secondary_byte_address_low_received) {
        // Capture the low eight bits of the byte address
        g_state.secondary_byte_address |= DATAR;
        g_state.secondary_byte_address_low_received = 1;
        #ifdef DEBUG_I2C
        printf("secondary byte address captured in RXNE: %d\n", g_state.secondary_byte_address);
        printf("    received low address byte, 0x%02x, full address %d\n", DATAR, g_state.secondary_byte_address);
        #endif
      } else {
        // TODO revert to read only and making fs data const
        #ifdef DEBUG_I2C
        printf("    write to secondary fs %d\n", g_state.secondary_byte_address);
        #endif

        if (g_state.secondary_byte_address < g_config->secondary_fs_size) {
          g_config->secondary_fs[g_state.secondary_byte_address] = DATAR;
        }
        g_state.secondary_byte_address++;

        #ifdef DEBUG_I2C
        printf("  write data ignored\n");
        #endif
      }
    } else {
      // Primary write event
      if (!g_state.primary_register_address_received) {
        // No register address received as part of write? First byte is the register addres
        g_state.primary_register_address = DATAR;
        g_state.primary_register_address_received = 1;

        #ifdef DEBUG_I2C
        printf("Primary write got register address 0x%02x\n", g_state.primary_register_address);
        #endif
      } else {
        if (g_state.primary_register_address == 0xFD) {
          // Command register special case: selects page
          #ifdef DEBUG_I2C
          printf("Primary write to command register 0xFD: 0x%02x\n", DATAR);
          #endif
          bool found = false;
          for (int i = 0; i < g_config->primary_num_pages; i++) {
            if (g_config->primary_page_definitions[i].page_address == DATAR) {
              g_state.primary_page = &g_config->primary_page_definitions[i];
              g_state.primary_register_address = 0;
              found = true;
              #ifdef DEBUG_I2C
              printf("Command register: primary page changed to 0x%02x and register address set to 0\n", g_state.primary_page->page_address);
              #endif
            }
          }
          if (!found) {
            g_state.primary_page = NULL;
          }
          // TODO end transfer after command?
        } else {
          // Regular write and auto increment address
          if (g_state.primary_page != NULL && g_state.primary_register_address < g_state.primary_page->data_size) {
            #ifdef DEBUG_I2C
            printf("Primary write to regular register 0x%02x in page %02x = 0x%02x\n", g_state.primary_register_address, g_state.primary_page->page_address, DATAR);
            #endif
            ((uint8_t*) g_state.primary_page->data)[g_state.primary_register_address] = DATAR;
            g_state.primary_register_address++;
            // TODO on_write callback?
          } else {
            #ifdef DEBUG_I2C
            printf("Primary write failed to bad page or register address, trying to set register 0x%02x = 0x%02x\n", g_state.primary_register_address, DATAR);
            #endif
          }
          // TODO error?
        }
      }
    }
  }

  // TODO when is the last address byte valid in the data register and is it already saying TXE because at that point the start condition for the switch to reading has been received?

  if (STAR1 & I2C_STAR1_TXE) {
    // Read event
    if (g_state.secondary) {
      #ifdef DEBUG_I2C
      printf("TXE secondary reading from %d\n", g_state.secondary_byte_address);
      #endif

      // Secondary read event
      if (g_state.secondary_byte_address < g_config->secondary_fs_size) {
        if (g_state.secondary_byte_address < sizeof(hexpansion_header_t)) {
          I2C1->DATAR = ((uint8_t *)g_config->secondary_header)[g_state.secondary_byte_address];
          #ifdef DEBUG_I2C
          printf("    TXE read from secondary header at %d\n", g_state.secondary_byte_address);
          #endif
        } else {
          I2C1->DATAR = g_config->secondary_fs[g_state.secondary_byte_address];
          #ifdef DEBUG_I2C
          printf("    TXE read from secondary fs at %d\n", g_state.secondary_byte_address);
          #endif
        }

        g_state.secondary_byte_address++;
      } else {
        I2C1->DATAR = 0x00;

        #ifdef DEBUG_I2C
        printf(" TXE read from secondary %d = 0x00 (out of range)\n", g_state.secondary_byte_address);
        #endif

        g_state.secondary_byte_address++;
      }
    } else {
      // Primary read event
      if (g_state.primary_register_address == 0xFD) {
        #ifdef DEBUG_I2C
        printf("Primary read command register\n");
        #endif
        // Command register special case: get address of current page
        if (g_state.primary_page != NULL) {
          I2C1->DATAR = g_state.primary_page->page_address;
        } else {
          I2C1->DATAR = 0xFF; // page not set
        }
      } else {
        // Regular read and auto increment register address
        if (g_state.primary_page != NULL && g_state.primary_register_address < g_state.primary_page->data_size) {
          I2C1->DATAR = ((uint8_t*) g_state.primary_page->data)[g_state.primary_register_address];
          #ifdef DEBUG_I2C
          printf("Primary read from register 0x%02x (0x%02x)\n", g_state.primary_register_address, I2C1->DATAR);
          #endif
          g_state.primary_register_address++;
        } else {
          #ifdef DEBUG_I2C
          printf("primary read but page %d is NULL or address is bigger than size\n", g_state.primary_page);
          #endif
          I2C1->DATAR = 0x00;
        }
      }
    }
  }

  if (STAR1 & I2C_STAR1_STOPF) {
    // Stop event
    I2C1->CTLR1 &= ~(I2C_CTLR1_STOP); // Clear stop
    
    // TODO on_write callbacks

    #ifdef DEBUG_I2C
    printf("I2C Stop\n\n");
    #endif
  }
}

void I2C1_ER_IRQHandler(void) __attribute__((interrupt)) __attribute__((section(".srodata")));
void I2C1_ER_IRQHandler(void) {
  uint16_t STAR1 __attribute__((unused));
  STAR1 = I2C1->STAR1;

  #ifdef DEBUG_I2C
  printf("I2C error %04x.\n", STAR1);
  #endif

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