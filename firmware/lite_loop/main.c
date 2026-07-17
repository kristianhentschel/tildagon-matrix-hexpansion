#include "ch32fun.h"
#include <stdio.h>
#include "../shared/matrix.h"
#include "../shared/debug_printf.h"
#include "../shared/hexpansion_header.h"
#include "../shared/i2c_again.h"
#include "../shared/core_registers.h"
#include "leds.h"
#include "animations/level.h"
#include "animations/starfield.h"
#include "animations/text.h"

// #define DEBUG_BLINK

void loop() __attribute__((section(".srodata")));

uint32_t count;
matrix_pwm_t frame_buffer[MATRIX_NUM_LEDS];

static hexpansion_header_t g_hexpansion_header = {
  .magic = {'T', 'H', 'E', 'X'},
  .manifest_version = {'2', '0', '2', '4'},
  .filesystem_info = {
    .offset = 32, // offset from start of eeprom, must be multiple of page size
    .page_size = 32, // emulated eeprom page size (not littlefs block size I think)
    .total_size = 0, // emulated space, reads past fs will return zero
  },
  .vendor_id = 0xCAFE, // "misc hexpansions" from https://badge.emfcamp.org/Tildagon/UHB-IF/Uncontrolled_IDs
  .product_id = 0x54E1,
  .unique_id = 0x0000,
  .friendly_name = {'L', 'i', 't', 'e', 'l', 'o', 'o', 'p', 0},
};

volatile core_registers_t g_core_registers = {
  .core_registers_version = CORE_REGISTERS_VERSION,
};
i2c_page_definition_t g_page_definitions[] = {
  {.page_address = 0, .data = (uint8_t *)&g_core_registers, .data_size = sizeof(g_core_registers)},
};

static const i2c_config_t g_i2c_config = {
  .primary_address = 0x20,
  .primary_num_pages = 1,
  .primary_page_definitions = g_page_definitions,

  .secondary_address = 0x50, // EEPROM on 0x50 (expected fixed address by companion app)
  .secondary_header =  &g_hexpansion_header,
  .secondary_fs = NULL,
  .secondary_fs_size = 0,
};

int main()
{
  SystemInit();
  Delay_Ms(50);

  funGpioInitAll();
  funPinMode(PA1, FUN_OUTPUT);
  funPinMode(PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SDA
  funPinMode(PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SCL

  funDigitalWrite(PA1, FUN_LOW);

  g_hexpansion_header.unique_id = ESIG->UNIID1 ^ ESIG->UNIID2 ^ ESIG->UNIID3 & 0xFFFF;
  hexpansion_header_fill_checksum(&g_hexpansion_header);

  i2c_setup(&g_i2c_config);

  matrix_setup(frame_buffer);

  g_core_registers.animation_type = 2;
  loop();
}

void loop() {
  int i = 0;
  while(1) {
    if (g_core_registers.direct_control_options != 0) {
      switch (g_core_registers.direct_control_options & CORE_REGISTERS_DIRECT_CONTROL_TYPE) {
        case CORE_REGISTERS_DIRECT_CONTROL_TYPE_CONST:
          // Constant mode: all LEDs set to the same PWM value (that of the first byte)
          for (uint8_t j = 0; j < MATRIX_NUM_LEDS; j++) {
            frame_buffer[j] = g_core_registers.direct_control_data[0];
          }
          break;
        case CORE_REGISTERS_DIRECT_CONTROL_TYPE_BIT:
          // Bit mode: unpack each bit in a 20 byte array to turn one LED fully on or off
          for (uint8_t j = 0; j < 20; j++) {
            for (uint8_t k = 0; k < 8; k++) {
              if (j * 8 + k < MATRIX_NUM_LEDS) {
                frame_buffer[j * 8 + k] = (g_core_registers.direct_control_data[j] & (0x01 << (7 - k))) ? 255 : 0;
              }
            }
          }
          break;
        case CORE_REGISTERS_DIRECT_CONTROL_TYPE_BYTE:
          // Byte morde: each settings byte controls the PWM level of one LED (1:1 mapping)
          for (uint8_t j = 0; j < MATRIX_NUM_LEDS; j++) {
            frame_buffer[j] = g_core_registers.direct_control_data[j];
          }
          break;
      }
    } else if (g_core_registers.animation_type == 1) {
      animation_level(&g_core_registers, frame_buffer);
    } else if (g_core_registers.animation_type == 2) {
      animation_starfield(&g_core_registers, frame_buffer);
    } else if (g_core_registers.animation_type == 3) {
      animation_text(&g_core_registers, frame_buffer);
    } else {
      // default test pattern (fill in wiring order)
      for (uint8_t j = 0; j < MATRIX_NUM_LEDS; j++) {
        frame_buffer[j] = j < i ? 255 : 4;
      }

      i = (i + 1) % (MATRIX_NUM_LEDS + 1);
      Delay_Ms(16);
      if (i == 0) {
        Delay_Ms(500);
      }
    }

    // Liveness indicator
    #ifdef DEBUG_BLINK
    static uint32_t _last_blink_tick = 0;
    if (TimeElapsed32u(SysTick->CNT, _last_blink_tick) > Ticks_from_Ms(100)) {
      funDigitalWrite(PA1, !funDigitalRead(PA1));
      _last_blink_tick = SysTick->CNT;
    }
    #endif
  }
}