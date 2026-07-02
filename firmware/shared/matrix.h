#ifndef MATRIX_H
#define MATRIX_H
// === Framework and standard includes ===
#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

// === Options ===

// The PWM count and number type that can hold numbers up to MATRIX_PWM_COUNT - 1.
// Must be a power of two between 256 and 4096, and either uint8_t or uint16_t.

// Currently working values:
// - bits = 7, prescaler = 0x10, ~50fps (preferred)
// - bits = 8, prescaler = 0x11, ~25fps

#define MATRIX_PWM_BITS 7
#define MATRIX_PWM_COUNT 1 << MATRIX_PWM_BITS
#define MATRIX_BUF_SIZE MATRIX_PWM_COUNT // TODO allow reducing this to save on SRAM
#define MATRIX_TIM1_PRESCALER 0x0010 // TODO attempt to reduce to increase column scan rate and reduce flicker

typedef uint8_t matrix_pwm_t;

// Choose the layout by uncommenting _one_ of these options:
// #define BOTTLESHIP_CONFIG_6x15 // 6x15 matrix for 90 LEDs using 10 pins
#define BOTTLESHIP_CONFIG_HEX_61 // two matrices of 6x5 = 60 pins (TODO plus one special one not considered)
// #define BOTTLESHIP_CONFIG_72R5 // 5 rings for 72 LEDs using 9 pins

// === Layout-specific options ===

// Number of LEDs in this layout
// Combining the ports and addressing them as one matrix with 13 pins allows addressing up to 156 LEDs.
#ifndef MATRIX_NUM_LEDS
#define MATRIX_NUM_LEDS 156
#endif

// Independent mode provides brighter LEDs and faster refresh cycles but can only address up to 72.
// #define MATRIX_INDEPENDENT_PORTS

// Number of Port C and Port D pins used
// Reduce this for smaller matrices
#ifndef MATRIX_NUM_GPIOC_PINS
#define MATRIX_NUM_GPIOC_PINS 6
#endif
#ifndef MATRIX_NUM_GPIOD_PINS
#define MATRIX_NUM_GPIOD_PINS 7
#endif

// Pin numbers
// On port C we can use 6 pins, skipping the I2C pins PC1 and PC2
#define MATRIX_GPIOC_0 0
#define MATRIX_GPIOC_1 3
#define MATRIX_GPIOC_2 4
#define MATRIX_GPIOC_3 5
#define MATRIX_GPIOC_4 6
#define MATRIX_GPIOC_5 7

// On port D, we can use 7 pins (including PD7), just skipping PD1 as that is used for SWIO
// Note: if PD7 is used, the NRST function must be disabled once in user option bytes with `minichlink -D`
#define MATRIX_GPIOD_0 0
#define MATRIX_GPIOD_1 2
#define MATRIX_GPIOD_2 3
#define MATRIX_GPIOD_3 4
#define MATRIX_GPIOD_4 5
#define MATRIX_GPIOD_5 6
#define MATRIX_GPIOD_6 7

// Extra LED on PD0, last in "wiring order"
// #define MATRIX_EXTRA_LED_GPIOD 0

// === Calculated declarations ===
// The below lines should not need to be changed for a new layout

// Calculate total number of pins and positions
#define MATRIX_NUM_PINS  (MATRIX_NUM_GPIOC_PINS + MATRIX_NUM_GPIOD_PINS)

// We assume charlieplex wiring using pins from GPIO ports C and D in the order defined in these arrays (first C, then D)
extern volatile const uint8_t MATRIX_GPIOC_PINS[MATRIX_NUM_GPIOC_PINS];
extern volatile const uint8_t MATRIX_GPIOD_PINS[MATRIX_NUM_GPIOD_PINS];


// === Methods ===

// Initialises the DMA, Timer1, and GPIO peripherals to drive the charlieplexed matrix on the pins
// defined in MATRIX_GPIOC/D_PINS.
// The frame buffer must have the configured MATRIX_NUM_LEDS entries.
// Takes control of the OUTDR for both GPIO ports but only reconfigures CFGR for the matrix pins.
// Unused pins may still be read or driven by other peripherals, they just cannot be used as GPIO outputs.
void matrix_setup(volatile matrix_pwm_t *frame_buffer);

// Replaces the frame buffer the LED PWM values are read from instantly (ie mid-frame or mid-line).
// The frame buffer must have the configured MATRIX_NUM_LEDS entries.
void matrix_swap_buffer(volatile matrix_pwm_t *frame_buffer);

// === Error checking ===
#ifndef MATRIX_NUM_LEDS
#error "config must define MATRIX_NUM_LEDS"
#endif

#ifndef MATRIX_PWM_COUNT
#error "config must define MATRIX_PWM_COUNT"
#endif

#if MATRIX_PWM_COUNT > 256
#error "MATRIX_PWM_COUNT should be <= 256"
#endif

#ifdef MATRIX_INDEPENDENT_PORTS
  #if MATRIX_NUM_LEDS > (MATRIX_NUM_GPIOC_PINS * (MATRIX_NUM_GPIOC_PINS - 1) + MATRIX_NUM_GPIOD_PINS * (MATRIX_NUM_GPIOD_PINS - 1))
  #error "MATRIX_NUM_LEDS is too large for the number of pins in independent port mode"
  #endif
#else
  #if MATRIX_NUM_LEDS > (MATRIX_NUM_PINS * (MATRIX_NUM_PINS - 1))
  #error "MATRIX_NUM_LEDS is too large for the number of pins in combined mode"
  #endif
#endif

#endif // MATRIX_H