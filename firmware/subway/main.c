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

  hexpansion_header_fill_checksum(&g_hexpansion_header);

	matrix_setup(frame_buffer);

  i2c_setup(&g_i2c_config);

  loop();
}

#define NUM_TRAINS 10
#define LEDS_PER_CIRCLE 30
#define RESOLUTION 20
#define NUM_POSITIONS (RESOLUTION*LEDS_PER_CIRCLE)
#define FRAME_PERIOD 5

#define ABS(a) ((a) > 0 ? (a) : -(a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

void loop() {
	// TODO random starting positions
	uint16_t inner_trains[NUM_TRAINS] = { 0, 20, 40, 80, 120 };
	uint16_t outer_trains[NUM_TRAINS] = { 0, 100, 175, 250 };
	uint8_t scratch[NUM_POSITIONS];

	while(1)
	{
		uint32_t frame_start = SysTick->CNT;

		// TODO more interesting movement logic (eg. stop at stations, no overtaking, acceleration and braking)
		// Advance all train positions
		for (int i = 0; i < NUM_TRAINS; i++) {
			if (rand() > 0x80000000) {
				inner_trains[i] = (inner_trains[i] + 1) % NUM_POSITIONS;
			}
			
			if (rand() > 0x80000000) {
				outer_trains[i] = (NUM_POSITIONS + outer_trains[i] - 1) % NUM_POSITIONS;
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
				uint16_t pos = i == 0 ? inner_trains[j] : outer_trains[j];
				for (int k = 0; k < RESOLUTION; k++) {
					scratch[(pos + k) % NUM_POSITIONS] = 1;
				}
			}

			// reduce by counting the number of occupied high resolution positions mapped to each LED
			for (int j = 0; j < LEDS_PER_CIRCLE; j++) {
				uint8_t sum = 0;
				for (int k = 0; k < RESOLUTION; k++) {
					sum = sum + scratch[(j * RESOLUTION + k) % NUM_POSITIONS];
				}
				frame_buffer[j + offset] = sum * (255 / RESOLUTION);
			}
		}

		while (TimeElapsed32u(SysTick->CNT, frame_start) < Ticks_from_Ms(FRAME_PERIOD)) {
			;
		}
	}
}

