#include "ch32fun.h"
#include "level.h"
#include "leds.h"

void animation_level(core_registers_t *core_registers, uint8_t *frame_buffer) {
  static int count = 0;

  float angle = core_registers->animation_options_1[0] / 255.0f * 360.0f;
  float port_angle = core_registers->animation_options_1[1] / 255.0f * 360.0f;

  for (int i = 0; i < LEDS_COUNT; i++) {
    uint8_t value = 255;

    float delta = leds_polar_positions[i].angle - angle;
    if (delta < 0) {
      delta *= -1.0;
    }
    
    if (delta < 5) {
      value = 0;
    } else if (delta < 6) {
      value = 16;
    } else {
      value = 255;
    }
    frame_buffer[i] = value;
  }

  count++;

  Delay_Ms(8);
}