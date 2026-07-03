#include "ch32fun.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define PRINTF printf
// #define BOOTLOADER_FORCE

#define BOOTLOADER_PIN PD7
#define BOOTLOADER_PIN_TIMEOUT 50
#define BOOTLOADER_INTERFACE_TIMEOUT 10000
#define BOOTLOADER_ADDRESS 0x2A
#define BOOTLOADER_BLINK_PIN PA1

#define COMMAND_STATUS 0x00
#define COMMAND_FAST_WRITE 0x01

#define SDA_PIN PC1
#define SCL_PIN PC2

volatile uint8_t buffer[256];
volatile uint32_t address;
volatile bool writing;

void boot_usercode();
void unlock_flash();
void i2c_init(uint8_t address);

int main() {
  SystemInit();

  // 1. Initialise GPIO pins and check if boot mode pin is pulled low externally within the timeout
  funGpioInitAll();

  // Input with pull up for boot mode detect pin
  funPinMode(BOOTLOADER_PIN, GPIO_CFGLR_IN_PUPD);
  funDigitalWrite(BOOTLOADER_PIN, FUN_HIGH);
  
  // Output for blink pin
  funPinMode(BOOTLOADER_BLINK_PIN, FUN_OUTPUT);

  // open drain output for I2C (needs external pull ups)
  funPinMode(SDA_PIN, GPIO_CFGLR_OUT_10Mhz_AF_OD);
  funPinMode(SCL_PIN, GPIO_CFGLR_OUT_10Mhz_AF_OD);

  funDigitalWrite(BOOTLOADER_BLINK_PIN, FUN_HIGH);
  Delay_Ms(BOOTLOADER_PIN_TIMEOUT);
  funDigitalWrite(BOOTLOADER_BLINK_PIN, FUN_LOW);

  // Boot to user code if bootloader pin is not pulled low
  // TODO ignoring the condition for testing, always stay in bootloader
  #ifndef BOOTLOADER_FORCE
  if (funDigitalRead(BOOTLOADER_PIN) != FUN_LOW) {
    boot_usercode();
  }
  #endif

  // 2. unlock the user flash area and fast write mode
  unlock_flash();
  
  // 3. create an I2C slave to allow bulk writes (4 address bytes + 256 data bytes at a time)
  i2c_init(BOOTLOADER_ADDRESS);

  Delay_Ms(BOOTLOADER_INTERFACE_TIMEOUT);

  boot_usercode();
}


void boot_usercode() {
  PRINTF("REBOOT\n");

  FLASH->BOOT_MODEKEYR = FLASH_KEY1;
  FLASH->BOOT_MODEKEYR = FLASH_KEY2;
  FLASH->STATR = 0;
  PFIC->SCTLR = 1<<31;
}

void unlock_flash() {
  FLASH->KEYR = FLASH_KEY1;
  FLASH->KEYR = FLASH_KEY2;
  FLASH->MODEKEYR = FLASH_KEY1;
  FLASH->MODEKEYR = FLASH_KEY2;
}

// I2C Slave adapted from ch32fun/examples/i2c_slave
struct _i2c_slave_state {
  bool command_write;
  bool address_write;
  uint8_t command;
  uint8_t offset;
  uint8_t position;
  bool writing;
} i2c_slave_state;


enum _programming_state {
  // assume flash is already unlocked (steps 1-2)
  PS_CLEAR_BUFFER, // wait for !BSY, set FTPG, set BUFRST, wait for !BSY, clear EOP (steps 3-6)
  PS_BYTE0, // collect the first byte
  PS_BYTE1, // collect the second byte
  PS_BYTE2, // collect the third byte
  PS_BYTE3_WRITE, // collect the fourth byte, write the full 32-bit word, set BUFLOAD, wait for !BSY or EOP (steps 7-10)
  // if it was the last word, write FLASH_ADDR, set STRT, wait for !BSY or EOP, clear EOP, read STATR (steps 11-14)
  // and if we can finish programming (do after every page for now?) clear FTPG (step 15)
  PS_FINISHED, // dummy state to discard any further writes until next start condition
} programming_state;

void i2c_init(uint8_t address) {
  i2c_slave_state.command_write = 1;
  i2c_slave_state.address_write = 0;
  i2c_slave_state.offset = 0; // in register reading, the first address to read in the transfer
  i2c_slave_state.position = 0; // in register reading, the current address to read

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

const uint32_t flash_start = 0x08000000;
uint32_t next_word;
uint8_t last_status;
uint8_t last_page_number;

void I2C1_EV_IRQHandler(void) __attribute__((interrupt));
void I2C1_EV_IRQHandler(void) {
  uint16_t STAR1, STAR2 __attribute__((unused));
  STAR1 = I2C1->STAR1;
  STAR2 = I2C1->STAR2;

  if (STAR1 & I2C_STAR1_ADDR) { // Start event
    i2c_slave_state.command_write = 1; // Next write will be the command
    i2c_slave_state.position = i2c_slave_state.offset; // Reset position
  }

  if (STAR1 & I2C_STAR1_RXNE) { // Write event
    if (i2c_slave_state.command_write) { // First byte written, set the command
      i2c_slave_state.command = I2C1->DATAR;
      i2c_slave_state.command_write = 0;
      i2c_slave_state.address_write = 1; // next write will be the address byte (page number)
      i2c_slave_state.writing = false;
    } else if (i2c_slave_state.address_write) { // Second byte written, set the address (page number)
      i2c_slave_state.offset = I2C1->DATAR;
      i2c_slave_state.position = i2c_slave_state.offset;
      i2c_slave_state.address_write = 0;
      i2c_slave_state.writing = false;

      if (i2c_slave_state.command == COMMAND_FAST_WRITE) {
        programming_state = PS_CLEAR_BUFFER; // reset state machine for programming before first data byte
        i2c_slave_state.position = 0; // always start from the first address in the page; offset is used as the page number
      }

      PRINTF("START %02x %d %d\n", i2c_slave_state.command, i2c_slave_state.offset, i2c_slave_state.position);
    } else { // data byte
      i2c_slave_state.writing = true;

      if (i2c_slave_state.command == COMMAND_FAST_WRITE) {
        switch (programming_state) {
          case PS_CLEAR_BUFFER: // collect first byte and wait for !BSY, set FTPG, set BUFRST, wait for !BSY or EOP, clear EOP (steps 3-6)
            PRINTF("CLEAR %d\n", i2c_slave_state.offset);
            while (FLASH->STATR & FLASH_STATR_BSY)
              ;
            FLASH->CTLR |= FLASH_CTLR_PAGE_FTPG;
            FLASH->CTLR |= FLASH_CTLR_BUF_RST;
            while (FLASH->STATR & FLASH_STATR_BSY && !(FLASH->STATR & FLASH_STATR_EOP))
              ;
            FLASH->STATR &= ~(FLASH_STATR_EOP);
            
            PRINTF("STATR: 0x%08lx CTLR: 0x%08lx\n", FLASH->STATR, FLASH->CTLR);
            
            // Also collect first byte here and then skip to byte1. later words don't need the clear buffer part and start with byte0 instead.
            next_word = (I2C1->DATAR << 0);
            programming_state = PS_BYTE1;
            break;
          case PS_BYTE0: // collect the first byte
            // PRINTF("BYTE0\n");
            next_word = (I2C1->DATAR << 0);
            programming_state = PS_BYTE1;
            break;
          case PS_BYTE1: // collect the second byte
            // PRINTF("BYTE1\n");
            next_word |= (I2C1->DATAR << 8);
            programming_state = PS_BYTE2;
            break;
          case PS_BYTE2: // collect the third byte
            // PRINTF("BYTE2\n");
            next_word |= (I2C1->DATAR << 16);
            programming_state = PS_BYTE3_WRITE;
            break;
          case PS_BYTE3_WRITE: // collect the fourth byte, write the full 32-bit word, set BUFLOAD, wait for !BSY or EOP (steps 7-10)
                               // if it was the last word, write FLASH_ADDR, set STRT, wait for !BSY or EOP, clear EOP, read STATR (steps 11-14)
                               // and if we can finish programming (do after every page for now?) clear FTPG (step 15)
            // PRINTF("BYTE3_WRITE\n");
            // Capture the last bbyte of the current word
            next_word |= (I2C1->DATAR << 24);

            // target address:
            // start from flash start address, and add
            // offset (page number) * 256 to get page start address, and add
            // position * 4 to get current 32-bit word address
            uint32_t page_address = flash_start + i2c_slave_state.offset * 256;
            uint32_t word_address = page_address + 4 * i2c_slave_state.position;

            *((uint32_t *) word_address) = next_word;
            // if (i2c_slave_state.position == 0)
            //   PRINTF("%08lx\n", next_word);
            
            // load buffer and wait for operation to finish
            FLASH->CTLR |= FLASH_CTLR_BUF_LOAD;
            while (FLASH->STATR & FLASH_STATR_BSY && !(FLASH->STATR & FLASH_STATR_EOP))
              ;

            // if this was the last word in the page, to the programming operation and wait for it to finish
            // otherwise start collecting bytes for the next word
            if (i2c_slave_state.position == 63) {
              PRINTF("WRITE PAGE\n");
              funDigitalWrite(BOOTLOADER_BLINK_PIN, FUN_HIGH);

              // Write the page address and start the programming operation
              FLASH->ADDR = page_address;
              FLASH->CTLR |= FLASH_CTLR_STRT;

              // Wait for operation to complete
              while (FLASH->STATR & FLASH_STATR_BSY && !(FLASH->STATR & FLASH_STATR_EOP))
                ;

              // Clear the EOP bit and read the status register
              FLASH->STATR &= ~(FLASH_STATR_EOP);
              last_page_number = i2c_slave_state.offset;
              last_status = FLASH->STATR;

              // Clear FTPG
              FLASH->CTLR &= ~(FLASH_CTLR_PAGE_FTPG);

              funDigitalWrite(BOOTLOADER_BLINK_PIN, FUN_LOW);
              programming_state = PS_FINISHED;

              PRINTF("DONE\n");
            } else {
              // Advance position for next word and wait for next byte
              i2c_slave_state.position++;

              programming_state = PS_BYTE0;
            }
            break;
          case PS_FINISHED:
            I2C1->DATAR;
            i2c_slave_state.position++;
            break;
        }
      } else {
        I2C1->DATAR; // TODO still read the data register to avoid indefinite clock extension?
      }
    }
  }

  if (STAR1 & I2C_STAR1_TXE) { // Read event
    i2c_slave_state.writing = false;
    if (i2c_slave_state.command == COMMAND_STATUS) {
      switch (i2c_slave_state.position) {
        case 0x00:
          I2C1->DATAR = last_status;
          break;
        case 0x01:
          I2C1->DATAR = last_page_number;
          break;
        case 0x02:
          I2C1->DATAR = INFO->CHIPID;
          break;
        case 0x04:
          I2C1->DATAR = ESIG->FLACAP;
          break;
        default:
          I2C1->DATAR = 0x00;
      }
      i2c_slave_state.position++;
    } else {
      I2C1->DATAR = 0x00;
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





