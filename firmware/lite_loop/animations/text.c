#include "text.h"
#include "leds.h"
#include <stdbool.h>

#define blit_pixel uint8_t
#include "fonts/blit16.h"
#include "fonts/blit32.h"

#define OFFSCREEN_PADDING 6 // should be the largest width of one character in any font
#define TEXT_BUFFER_WIDTH (18 + 2 * OFFSCREEN_PADDING) // larger than needed because blit font doesn't draw partial character
#define TEXT_BUFFER_HEIGHT 10 // larger than display to cut off descenders in blit32 rather than ascenders
static uint8_t buffer[TEXT_BUFFER_WIDTH * TEXT_BUFFER_HEIGHT];
static bool init = false;

// Activate text mode by writing 0x03 to animation type register (0x37)

// this reinterprets the 'direct control' registers as ascii characters (zero terminated)
// 0x50 direct control options (must be zero)
// 0x51-0xEC ASCII text

// Additional configuration:
// 0x38 (2 bytes) 16-bit offset in pixels
// 0x3A (1 byte) flags
// - 1:0 font (00 = blit16, 01 = blit32, 10-11 = reserved)
// - [not implemented] 2   flip (0 = baseline outer, 1 = baseline inner)
// - [not implemented] 6:3 reserved
// - [not implemented] 7   ended (= text is no longer visible in this segment due to offset)

void animation_text(core_registers_t *core_registers, matrix_pwm_t *frame_buffer) {
  uint16_t offset = (core_registers->animation_options_1[0]) << 8 | (core_registers->animation_options_1[1]);
  uint8_t flags = core_registers->animation_options_1[2];
  char *text = core_registers->direct_control_data;
  uint8_t len = 0;
  for (len = 0; len < sizeof(core_registers->direct_control_data); len++) {
    if (text[len] == '\0') {
      break;
    }
  }
  
  // TODO interpret flip flag and set ended? flag
  
  // Render text (this applies the offset too)
  for (int i = 0; i < sizeof(buffer); i++) {
    buffer[i] = 0;
  }
  
  // offset = 0 means the text starts in the first off-screen column on the right edge
  int startX = LEDS_GRID_COLS + OFFSCREEN_PADDING - offset;
  switch (flags & 0x03) {
    case 0x00:
      blit16_TextNExplicit(buffer, 255, 1, TEXT_BUFFER_WIDTH, TEXT_BUFFER_HEIGHT, 0, startX, 2, len, text);
      break;
    case 0x01:
    default:
      blit32_TextNExplicit(buffer, 255, 1, TEXT_BUFFER_WIDTH, TEXT_BUFFER_HEIGHT, 0, startX, 1, len, text);
  }

  // Copy rendered pixels into frame buffer
  // TODO this step and the `buffer` may be unneccessary but then text rendering would have to be taken out of blit*.h
  for (int i = 0; i < LEDS_GRID_COLS; i++) {
    for (int j = 0; j < LEDS_GRID_ROWS; j++) {
      uint8_t index = leds_grid_indices[i + j * LEDS_GRID_COLS];
      if (index > LEDS_COUNT) {
        continue;
      }

      frame_buffer[index] = buffer[(i + OFFSCREEN_PADDING) + j * TEXT_BUFFER_WIDTH];
    }
  }

  Delay_Ms(10);
}