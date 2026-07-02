#include "text.h"
#include "leds.h"
#include <stdbool.h>

#define TEXT_COLS 1024
static uint8_t image[TEXT_COLS];
static bool init = false;

void animation_text(core_registers_t *core_registers, matrix_pwm_t *frame_buffer) {
  // offset = data column currently shown in the right most display column
  static uint32_t offset = 0;
  const uint32_t length = TEXT_COLS + LEDS_GRID_COLS;

  // Initialise image with garbage data
  if (!init) {
    for (uint32_t i = 0; i < TEXT_COLS; i++) {
      image[i] = i % 255;
    }
    init = true;

    for (int i = 0; i < LEDS_GRID_ROWS; i++) {
      for (int j = 0; j < LEDS_GRID_COLS; j++) {
        int pos = leds_grid_indices[i * LEDS_GRID_COLS + j];
        frame_buffer[pos] = 255;
        Delay_Ms(5);
      }
    }
  }

  for (int i = 0; i < LEDS_GRID_COLS; i++) {
    int32_t col = i + offset - LEDS_GRID_COLS;
    uint8_t col_data = 0x00;
    if (col >= 0 && col < TEXT_COLS) {
      col_data = image[col];
    }

    for (int j = 0; j < LEDS_GRID_ROWS; j++) {
      int pos = leds_grid_indices[i + j * LEDS_GRID_COLS];
      if (pos > LEDS_COUNT) continue;

      if (j == 0) {
        // Ignore top row because we only have an 8-bit wide buffer
        frame_buffer[pos] = 0;
      } else {
        frame_buffer[pos] = (col_data & (1 << (8 - j))) ? 255 : 0;
      }
    }
  }

  offset = (offset + 1) % length;
  Delay_Ms(50);
}