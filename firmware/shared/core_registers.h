#ifndef CORE_REGISTERS_H
#define CORE_REGISTERS_H
#include <stdint.h>

#define CORE_REGISTERS_VERSION 0x00000001

typedef struct core_registers {
  // 0x00
  uint32_t core_registers_version;
  uint32_t time_seconds;
  uint16_t time_milliseconds;
  uint8_t current_slot;
  uint8_t used_slots;
  uint8_t _reserved_0[4];

  // 0x10
  int16_t acc_x;
  int16_t acc_y;
  int16_t acc_z;
  int16_t acc_scale;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  int16_t gyro_scale;

  // 0x20
  int16_t mag_x;
  int16_t mag_y;
  int16_t mag_z;
  int16_t mag_scale;
  uint8_t _reserved_2[8];

  // 0x30
  uint8_t animation_brightness;
  uint8_t animation_period;
  uint8_t reserved_3[5];
  uint8_t animation_type;         // 0x37
  uint8_t animation_options_1[8]; // 0x38

  // 0x40
  uint8_t animation_options_2[16];

  // 0x50
  uint8_t direct_control_options;
  
  // 0x51 - 0xEC
  // depending on setting of direct control options only a subset of these bytes will be used
  uint8_t direct_control_data[156];

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