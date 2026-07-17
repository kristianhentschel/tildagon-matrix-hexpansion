#ifndef CORE_REGISTERS_H
#define CORE_REGISTERS_H
#include <stdint.h>

// TODO define this from commit hash in makefile and just check for equality?
#ifndef CORE_REGISTERS_VERSION
#define CORE_REGISTERS_VERSION 20260716
#endif

#define CORE_REGISTERS_DIRECT_CONTROL_TYPE 0x03
#define CORE_REGISTERS_DIRECT_CONTROL_TYPE_NONE 0x00
#define CORE_REGISTERS_DIRECT_CONTROL_TYPE_CONST 0x01
#define CORE_REGISTERS_DIRECT_CONTROL_TYPE_BIT 0x02
#define CORE_REGISTERS_DIRECT_CONTROL_TYPE_BYTE 0x03

typedef struct core_registers {
  // 0x00 - only version is used so far
  volatile uint32_t core_registers_version;
  volatile uint32_t time_seconds;
  volatile uint16_t time_milliseconds;
  volatile uint8_t current_slot;
  volatile uint8_t matching_slots;
  volatile uint8_t _reserved_0[4]; // break glass in case of hedgehog

  // 0x10 - IMU, not used yet
  volatile int16_t acc_x;
  volatile int16_t acc_y;
  volatile int16_t acc_z;
  volatile int16_t acc_scale;
  volatile int16_t gyro_x;
  volatile int16_t gyro_y;
  volatile int16_t gyro_z;
  volatile int16_t gyro_scale;

  // 0x20 - IMU, not used yet
  volatile int16_t mag_x;
  volatile int16_t mag_y;
  volatile int16_t mag_z;
  volatile int16_t mag_scale;
  volatile uint8_t _reserved_2[8];

  // 0x30 - only 0x37 onward are used so far
  volatile uint8_t animation_brightness;
  volatile uint8_t animation_period;
  volatile uint8_t _reserved_3[5];
  volatile uint8_t animation_type;         // 0x37 - pattern selection
  volatile uint8_t animation_options_1[8]; // 0x38 - pattern-specific meaning

  // 0x40 reserved for further animation options
  volatile uint8_t animation_options_2[16];

  // 0x50 Direct control options
  // bits [7:2] reserved, should be written as 0
  // bits [0:1]
  // 0 = Direct control is not used, animation pattern (see 0x30) is used instead.
  // 1 = 1 byte at 0x51 applied to all LEDs
  // 2 = 20 bytes from 0x51, 1 bit on/off per LED (high bit of first byte = LED 0)
  // 3 = 156 bytes from 0x51, 1 byte PWM value per LED
  volatile uint8_t direct_control_options;

  // 0x51 - 0xEC
  // depending on setting of direct control options only a subset of these bytes will be used
  // NB text display uses this as ASCII characters
  volatile uint8_t direct_control_data[156];

  // NB 0xFD is reserved as the command register to select the active page
} core_registers_t;
#endif