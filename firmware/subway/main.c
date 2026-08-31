#include "ch32fun.h"
#include <stdio.h>
#include "../shared/debug_printf.h"
#include "../shared/hexpansion_header.h"
#include "../shared/i2c_again.h"
#include "../shared/matrix.h"
#include "lib_rand.h"

#define BTN_LEFT_PIN PA2
#define BTN_RIGHT_PIN PA1

void loop() __attribute__((section(".srodata")));

matrix_pwm_t frame_buffer[MATRIX_NUM_LEDS];

static hexpansion_header_t g_hexpansion_header = {
  .magic = {'T', 'H', 'E', 'X'},
  .manifest_version = {'2', '0', '2', '4'},
  .filesystem_info = {
    .offset = 32, // offset from start of eeprom, must be multiple of page size
    .page_size = 32, // emulated eeprom page size (not littlefs block size I think)
    .total_size = 0, // emulated space, reads past fs will return zero
  },
  .vendor_id = 0xCAFE,
  .product_id = 0x54E1,
  .unique_id = 0x0000,
  .friendly_name = {'S', 'h', 'o', 'o', 'g', 'l', 'e', 0, 0},
};

static const i2c_config_t g_i2c_config = {
  .primary_address = 0x20,
  .primary_num_pages = 0, // primary I2C interface not used yet
  .primary_page_definitions = NULL,

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

  // Configure button pins as inputs with pull up
  funPinMode(BTN_LEFT_PIN, GPIO_CNF_IN_PUPD);
  funPinMode(BTN_RIGHT_PIN, GPIO_CNF_IN_PUPD);
  funDigitalWrite(BTN_LEFT_PIN, FUN_HIGH);
  funDigitalWrite(BTN_RIGHT_PIN, FUN_HIGH);

  // Configure I2C pins (TODO why isn't this in i2c_setup)
  funPinMode(PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SDA
  funPinMode(PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SCL

  hexpansion_header_fill_checksum(&g_hexpansion_header);

  matrix_setup(frame_buffer);

  i2c_setup(&g_i2c_config);

  loop();
}

#define NUM_TRAINS 16
#define LEDS_PER_CIRCLE 30
#define RESOLUTION 30
#define NUM_POSITIONS (RESOLUTION*LEDS_PER_CIRCLE)
#define NUM_STATIONS 15
#define STATION_SPACING (RESOLUTION * LEDS_PER_CIRCLE / NUM_STATIONS)
#define STATION_OFFSET (RESOLUTION / 2)
#define FRAME_PERIOD 20
#define STOPPING_TIME 100
// TODO define a rand() wrapper to scale with resolution

// Gamma brightness lookup table <https://victornpb.github.io/gamma-table-generator>
// gamma = 2.70 steps = 256 range = 0-255
const uint8_t gamma_lut[256] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   3,   3,
  3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,   5,   6,   6,
  6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  10,  11,
  11,  12,  12,  12,  13,  13,  14,  14,  14,  15,  15,  16,  16,  17,  17,  18,
  18,  19,  19,  20,  20,  21,  21,  22,  23,  23,  24,  24,  25,  26,  26,  27,
  28,  28,  29,  30,  30,  31,  32,  33,  33,  34,  35,  36,  36,  37,  38,  39,
  40,  41,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  51,  52,  53,
  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  68,  69,  70,  71,
  72,  74,  75,  76,  77,  79,  80,  81,  83,  84,  85,  87,  88,  89,  91,  92,
  94,  95,  97,  98, 100, 101, 103, 104, 106, 107, 109, 110, 112, 114, 115, 117,
  119, 120, 122, 124, 125, 127, 129, 131, 132, 134, 136, 138, 140, 141, 143, 145,
  147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 178,
  180, 182, 184, 186, 188, 191, 193, 195, 198, 200, 202, 205, 207, 209, 212, 214,
  216, 219, 221, 224, 226, 229, 231, 234, 237, 239, 242, 244, 247, 250, 252, 255,
};

typedef struct train {
  uint16_t pos;
  uint8_t speed;
  uint8_t dwell;
  uint16_t next_station_pos;
  int8_t direction;
} train_t;

void loop() {
  // TODO random starting positions
  train_t trains[NUM_TRAINS];
  uint8_t scratch[NUM_POSITIONS];

  for (int i = 0; i < NUM_TRAINS; i++) {
    uint16_t pos = (STATION_OFFSET + i * STATION_SPACING) % NUM_POSITIONS;
    train_t train = {
      .pos = pos,
      .speed = 1,
      .dwell = (rand() & 0xff),
      .next_station_pos = pos,
      .direction = i % 2 ? -1 : 1,
    };

    trains[i] = train;
  }

  while(1)
  {
    uint32_t frame_start = SysTick->CNT;

    // TODO more interesting movement logic (eg. stop at stations, no overtaking, acceleration and braking)
    // Advance all train positions
    for (int i = 0; i < NUM_TRAINS; i++) {
      train_t *train = &trains[i];

      // pause if reached the next station
      if (train->pos == train->next_station_pos) {
        train->dwell = STOPPING_TIME + (rand() & 0x3F);
        train->next_station_pos = (NUM_POSITIONS + train->pos + train->direction * STATION_SPACING) % NUM_POSITIONS;

        train->speed = (rand() & 0xF) > 10 ? 2 : 1; // Some trains are just faster, sometimes. NB speed must evenly divide into station spacing
      }

      // pause if there is a train in front
      if (train->dwell == 0) {
        for (int j = 0; j < NUM_TRAINS; j++) {
          train_t *other = &trains[j];
          if (i != j && train->direction == other->direction) {
            if ((NUM_POSITIONS + train->direction * other->pos - train->direction * train->pos) % NUM_POSITIONS <= STATION_SPACING / 2) {
              train->dwell = (rand() & 0xF);
              break;
            }
          }
        }
      }

      // decrement dwell time or move at max speed
      if (train->dwell > 0) {
        train->dwell--;
      } else {
        train->pos = (NUM_POSITIONS + train->pos + train->speed * train->direction) % NUM_POSITIONS;
      }
    }

    // Update LEDs
    for (int i = 0; i < 2; i++) {
      uint8_t offset = i * LEDS_PER_CIRCLE;

      // clear
      for (int j = 0; j < NUM_POSITIONS; j++) {
        scratch[j] = 0;
      }

      // fill in high resolution positions occupied by the train
      for (int j = 0; j < NUM_TRAINS; j++) {
        // skip trains on the other circle
        if (trains[j].direction != (i == 0 ? 1 : -1)) {
          continue;
        }

        uint16_t pos = trains[j].pos;
        for (int k = 0; k < RESOLUTION; k++) {
          scratch[(pos + k) % NUM_POSITIONS] = 1;
        }
      }

      // reduce by counting the number of occupied high resolution positions mapped to each LED
      for (int j = 0; j < LEDS_PER_CIRCLE; j++) {
        uint8_t sum = 0;
        for (int k = 0; k < RESOLUTION; k++) {
          sum = sum + scratch[(NUM_POSITIONS + j * RESOLUTION - RESOLUTION / 2 + k) % NUM_POSITIONS];
        }
        frame_buffer[j + offset] = gamma_lut[sum * (255 / RESOLUTION)];
        if (frame_buffer[j + offset] == 0) {
          frame_buffer[j + offset] = 1;
        }
      }
    }

    while (TimeElapsed32u(SysTick->CNT, frame_start) < Ticks_from_Ms(FRAME_PERIOD)) {
      ;
    }
  }
}

