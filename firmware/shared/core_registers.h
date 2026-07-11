#ifndef CORE_REGISTERS_H
#define CORE_REGISTERS_H
#include <stdint.h>

#define CORE_REGISTERS_VERSION 0x20260711

typedef struct core_registers {
  // 0x00
  volatile uint32_t core_registers_version;
  volatile uint32_t time_seconds;
  volatile uint16_t time_milliseconds;
  volatile uint8_t current_slot;
  volatile uint8_t used_slots;
  volatile uint8_t _reserved_0[4];

  // 0x10
  volatile int16_t acc_x;
  volatile int16_t acc_y;
  volatile int16_t acc_z;
  volatile int16_t acc_scale;
  volatile int16_t gyro_x;
  volatile int16_t gyro_y;
  volatile int16_t gyro_z;
  volatile int16_t gyro_scale;

  // 0x20
  volatile int16_t mag_x;
  volatile int16_t mag_y;
  volatile int16_t mag_z;
  volatile int16_t mag_scale;
  volatile uint8_t _reserved_2[8];

  // 0x30
  volatile uint8_t animation_brightness;
  volatile uint8_t animation_period;
  volatile uint8_t reserved_3[5];
  volatile uint8_t animation_type;         // 0x37
  volatile uint8_t animation_options_1[8]; // 0x38

  // 0x40
  volatile uint8_t animation_options_2[16];

  // 0x50
  volatile uint8_t direct_control_options;
  
  // 0x51 - 0xEC
  // depending on setting of direct control options only a subset of these bytes will be used
  volatile uint8_t direct_control_data[156];

  // NB 0xFD is reserved as the command register to select the active page
} core_registers_t;


typedef struct text_registers {
  uint8_t len;
  char data[128];
} text_registers_t;

typedef struct frame_registers {
  uint8_t pwm[156];
} frame_registers_t;

#endif