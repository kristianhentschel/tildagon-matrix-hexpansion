#include "./matrix.h"

// DMA and timer configuration based on the ch32fun dma_gpio example.

volatile const uint8_t MATRIX_GPIOC_PINS[MATRIX_NUM_GPIOC_PINS] = { 
  MATRIX_GPIOC_0,
  MATRIX_GPIOC_1,
  MATRIX_GPIOC_2,
  MATRIX_GPIOC_3,
  #ifdef MATRIX_GPIOC_4
  MATRIX_GPIOC_4,
  #endif
  #ifdef MATRIX_GPIOC_5
  MATRIX_GPIOC_5,
  #endif
  #ifdef MATRIX_GPIOC_6
  MATRIX_GPIOC_6,
  #endif
  #ifdef MATRIX_GPIOC_7
  MATRIX_GPIOC_7,
  #endif
};
volatile const uint8_t MATRIX_GPIOD_PINS[MATRIX_NUM_GPIOD_PINS] = {
  MATRIX_GPIOD_0,
  MATRIX_GPIOD_1,
  MATRIX_GPIOD_2,
  MATRIX_GPIOD_3,
  #ifdef MATRIX_GPIOD_4
  MATRIX_GPIOD_4,
  #endif
  #ifdef MATRIX_GPIOD_5
  MATRIX_GPIOD_5,
  #endif
  #ifdef MATRIX_GPIOD_6
  MATRIX_GPIOD_6,
  #endif
  #ifdef MATRIX_GPIOD_7
  MATRIX_GPIOD_7,
  #endif
};

// Reference to the currently active frame buffer, expected to hold a PWM value for each LED.
static volatile matrix_pwm_t *current_frame_buffer = 0;
static volatile matrix_pwm_t *next_frame_buffer = 0;
static volatile uint8_t buffer_swap_flag = 0;

// Define buffers to hold the PWM signals to output to the GPIO ports
#define BUF_SIZE MATRIX_BUF_SIZE
#define HALF_BUF_SIZE (BUF_SIZE / 2)

static volatile uint8_t PWM_BUF_GPIOC[BUF_SIZE];
static volatile uint8_t PWM_BUF_GPIOD[BUF_SIZE];

// For each column, for each row pin, store the index into the frame buffer
#ifdef MATRIX_INDEPENDENT_PORTS
  static volatile uint8_t ROWS_GPIOC[MATRIX_NUM_GPIOC_PINS][MATRIX_NUM_GPIOC_PINS];
  static volatile uint8_t ROWS_GPIOD[MATRIX_NUM_GPIOD_PINS][MATRIX_NUM_GPIOD_PINS];
#else
  static volatile uint8_t ROWS_GPIOC[MATRIX_NUM_PINS][MATRIX_NUM_GPIOC_PINS];
  static volatile uint8_t ROWS_GPIOD[MATRIX_NUM_PINS][MATRIX_NUM_GPIOD_PINS];
#endif

#ifdef MATRIX_EXTRA_LED_GPIOD
  // Holds the frame buffer index for the extra LED
  static volatile uint8_t EXTRA_GPIOD;
#endif

static volatile uint32_t RESET_GPIOC_CFGLR;
static volatile uint32_t RESET_GPIOD_CFGLR;

void matrix_setup(volatile matrix_pwm_t *frame_buffer) {
  // Keep a reference to the current frame buffer
  if (current_frame_buffer != 0) {
    // already initialised
    matrix_swap_buffer(frame_buffer);
    return;
  } else {
    matrix_swap_buffer(frame_buffer);
  }

  #ifdef MATRIX_INDEPENDENT_PORTS
    // #define DEBUG_POSITIONS
    uint8_t frame_buffer_index = 0;

    for (uint8_t col = 0; col < MATRIX_NUM_GPIOC_PINS; col++) {
      for (uint8_t row = 0; row < MATRIX_NUM_GPIOC_PINS; row++) {
        if (frame_buffer_index >= MATRIX_NUM_LEDS) {
          break;
        }
        ROWS_GPIOC[col][row] = frame_buffer_index;

        // Only increment the index if there can be an LED at the col/row position.
        // NB this means the column pins will get assigned the value of the next LED here,
        // but will be overwritten by its correct use as a column pin in the buffer update.
        if (row != col) {
          frame_buffer_index++;
        }
      }
    }

    for (uint8_t col = 0; col < MATRIX_NUM_GPIOD_PINS; col++) {
      for (uint8_t row = 0; row < MATRIX_NUM_GPIOD_PINS; row++) {
        if (frame_buffer_index >= MATRIX_NUM_LEDS) {
          break;
        }
        ROWS_GPIOD[col][row] = frame_buffer_index;

        if (row != col) {
          frame_buffer_index++;
        }
      }
    }
  #else
    uint8_t frame_buffer_index = 0;
    for (uint8_t col = 0; col < MATRIX_NUM_PINS; col++) {
      for (uint8_t row = 0; row < MATRIX_NUM_PINS; row++) {
        if (frame_buffer_index >= MATRIX_NUM_LEDS) {
          break;
        }
        if (row < MATRIX_NUM_GPIOC_PINS) {
          ROWS_GPIOC[col][row] = frame_buffer_index;
        } else {
          ROWS_GPIOD[col][row - MATRIX_NUM_GPIOC_PINS] = frame_buffer_index;
        }

        // Only increment the index if there can be an LED at the col/row position.
        // NB this means the row for will get assigned the value of the next LED here,
        // but will be overwritten by its correct use as a column pin in the buffer update.
        if (row != col) {
          frame_buffer_index++;
        }
      }
    }
  #endif

  #ifdef MATRIX_EXTRA_LED_GPIOD
    if (frame_buffer_index < MATRIX_NUM_LEDS) {
      EXTRA_GPIOD = inverted_matrix_leds[frame_buffer_index];
      frame_buffer_index++;
    } else {
      EXTRA_GPIOD = 0; // NB this should not happen, but need to protect against overflow
    }
  #endif

  // Enable DMA peripheral
	RCC->AHBPCENR = RCC_AHBPeriph_SRAM | RCC_AHBPeriph_DMA1;

	// Enable GPIOC, GPIOD and Timer 1 peripherals
	RCC->APB2PCENR = RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1;

  // Configure all used pins as open drain outputs; the active column pin will be
  // set as a Push-Pull output later.
  // Port C
  RESET_GPIOC_CFGLR = GPIOC->CFGLR;
  for (int i = 0; i < MATRIX_NUM_GPIOC_PINS; i++) {
    uint8_t pin = MATRIX_GPIOC_PINS[i];
    
    // Clear CNF and MODE bits for the current pin
    RESET_GPIOC_CFGLR &= ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * pin);
    
    // Set CNF and MODE bits to the config for 10MHz open-drain output
    RESET_GPIOC_CFGLR |= GPIO_CFGLR_OUT_10Mhz_OD << 4 * pin;
  }
  GPIOC->CFGLR = RESET_GPIOC_CFGLR;
  GPIOC->OUTDR = 0xff;

  // Same for Port D
  RESET_GPIOD_CFGLR = GPIOC->CFGLR;
  for (int i = 0; i < MATRIX_NUM_GPIOD_PINS; i++) {
    uint8_t pin = MATRIX_GPIOD_PINS[i];
    RESET_GPIOD_CFGLR &= ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * pin);
    RESET_GPIOD_CFGLR |= GPIO_CFGLR_OUT_10Mhz_OD << 4 * pin;
  }
  
  // Optional extra LED initially also configured as OD
  #ifdef MATRIX_EXTRA_LED_GPIOD
  {
    uint8_t pin = MATRIX_EXTRA_LED_GPIOD;
    RESET_GPIOD_CFGLR &= ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * pin);
    RESET_GPIOD_CFGLR |= GPIO_CFGLR_OUT_10Mhz_OD << 4 * pin;
  }
  #endif
  
  GPIOD->CFGLR = RESET_GPIOD_CFGLR;
  GPIOD->OUTDR = 0xff;

  // Configure the DMA peripheral to use Channel 2 to output to GPIOC
  DMA1_Channel2->MADDR = (uint32_t) &PWM_BUF_GPIOC[0];
  DMA1_Channel2->PADDR = (uint32_t) &GPIOC->OUTDR;
  DMA1_Channel2->CNTR = MATRIX_PWM_COUNT;

  DMA1_Channel2->CFGR =
    DMA_DIR_PeripheralDST |
    DMA_Priority_High |
    DMA_MemoryDataSize_Byte |
    DMA_PeripheralDataSize_Byte |
    DMA_MemoryInc_Enable |
    DMA_PeripheralInc_Disable |
    DMA_Mode_Circular |
    DMA_CFGR3_HTIE |
    DMA_CFGR3_TCIE;

  NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  DMA1_Channel2->CFGR |= DMA_CFGR2_EN;

  // Same for DMA Channel 3 to output to GPIOD
  DMA1_Channel3->MADDR = (uint32_t) &PWM_BUF_GPIOD[0];
  DMA1_Channel3->PADDR = (uint32_t) &GPIOD->OUTDR;
  DMA1_Channel3->CNTR = MATRIX_PWM_COUNT;

  DMA1_Channel3->CFGR =
    DMA_DIR_PeripheralDST |
    DMA_Priority_High |
    DMA_MemoryDataSize_Byte |
    DMA_PeripheralDataSize_Byte |
    DMA_MemoryInc_Enable |
    DMA_PeripheralInc_Disable |
    DMA_Mode_Circular;

  DMA1_Channel3->CFGR |= DMA_CFGR3_EN;
  
  // Configure the timer to generate DMA requests offset from each other
  // Timer channel 1 is connected to DMA channel 2
  // Timer channel 2 is connected to DMA channel 3
  RCC->APB2PRSTR = RCC_APB2Periph_TIM1; // Reset timer
  RCC->APB2PRSTR = 0;

  // TODO set this correctly - currently slowed down massively to test interrupt bugs
	TIM1->PSC = MATRIX_TIM1_PRESCALER;        // Prescaler

  #ifdef CH32V00x
	TIM1->ATRLR = 17;                                           // Auto Reload - sets period (48MHz / (17+1) = 2.667MHz)
	TIM1->SWEVGR = TIM1_SWEVGR_UG | TIM1_SWEVGR_TG;             // Reload immediately + Trigger DMA
	TIM1->CCER = TIM1_CCER_CC1E | TIM1_CCER_CC1P;               // Enable CH1 output, positive pol
	TIM1->CCER |= TIM1_CCER_CC2E | TIM1_CCER_CC2P;              // Also enable CH2 output
	TIM1->CHCTLR1 = TIM1_CHCTLR1_OC1M_2 | TIM1_CHCTLR1_OC1M_1;  // CH1 Mode is output, PWM1 (CC1S = 00, OC1M = 110)
	TIM1->CHCTLR1 |= TIM1_CHCTLR1_OC2M_2 | TIM1_CHCTLR1_OC2M_1; // CH2 Mode is output, PWM1 (CC2S = 00, OC2M = 110)
	TIM1->CH1CVR = 0;                                           // Trigger Ch1 at the start of the period
	TIM1->CH2CVR = 8;                                           // Trigger Ch2 at 50% of the period
	TIM1->BDTR = TIM1_BDTR_MOE;                                 // Enable TIM1 outputs

	TIM1->CTLR1 = TIM1_CTLR1_CEN;                               // Enable TIM1
	TIM1->DMAINTENR = TIM1_DMAINTENR_UDE
    | TIM1_DMAINTENR_CC1DE | TIM1_DMAINTENR_CC2DE;            // Trigger DMA on TC match 1 (DMA Ch2) and TC match 2 (DMA Ch3)
  #else
  #warning "CH32V003"
  TIM1->ATRLR = 17;                         // Auto Reload - sets period (48MHz / (17+1) = 2.667MHz)
  TIM1->SWEVGR = TIM_UG | TIM_TG;           // Reload immediately + Trigger DMA
  TIM1->CCER = TIM_CC1E | TIM_CC1P;         // Enable CH1 output, positive pol
  TIM1->CCER |= TIM_CC2E | TIM_CC2P;        // Also enable CH2 output
  TIM1->CHCTLR1 = TIM_OC1M_2 | TIM_OC1M_1;  // CH1 Mode is output, PWM1 (CC1S = 00, OC1M = 110)
  TIM1->CHCTLR1 |= TIM_OC2M_2 | TIM_OC2M_1; // CH2 Mode is output, PWM1 (CC2S = 00, OC2M = 110)
  TIM1->CH1CVR = 0;                         // Trigger Ch1 at the start of the period
  TIM1->CH2CVR = 8;                         // Trigger Ch2 at 50% of the period
  TIM1->BDTR = TIM_MOE;                     // Enable TIM1 outputs

  TIM1->CTLR1 = TIM_CEN;                    // Enable TIM1
  TIM1->DMAINTENR = TIM_UDE | TIM_CC1DE | TIM_CC2DE;   // Trigger DMA on TC match 1 (DMA Ch2) and TC match 2 (DMA Ch3)
  #endif

  // debug_printf("Matrix set up with prescaler %04x and count = %d\n", MATRIX_TIM1_PRESCALER, MATRIX_PWM_COUNT);
}

void matrix_swap_buffer(volatile matrix_pwm_t *frame_buffer) {
  if (current_frame_buffer == 0) {
    current_frame_buffer = frame_buffer;
  } else if (frame_buffer != current_frame_buffer) {
	  next_frame_buffer = frame_buffer;
	  buffer_swap_flag = 1;
  }
}

static volatile uint8_t active_column = 0;

// DMA Channel 2 interrupt handler
void DMA1_Channel2_IRQHandler(void) __attribute__((interrupt)) __attribute__((section(".srodata")));
void DMA1_Channel2_IRQHandler(void) {
  volatile uint32_t intfr = DMA1->INTFR;
  DMA1->INTFCR |= DMA1_IT_GL2; // clear interrupt flags for ch 2

  uint8_t next_column = (active_column + 1);
  #ifdef MATRIX_INDEPENDENT_PORTS
    // Independent matrices: column scan over the bigger matrix
    #if MATRIX_NUM_GPIOC_PINS > MATRIX_NUM_GPIOD_PINS
      next_column = next_column % MATRIX_NUM_GPIOC_PINS;
    #else
      next_column = next_column % MATRIX_NUM_GPIOD_PINS;
    #endif
  #else
    // Single matrix: column scan over all pins
    next_column = next_column % MATRIX_NUM_PINS;
  #endif
  uint16_t i;
  uint8_t level;
  uint8_t pin;
  uint16_t start = 0;
  uint16_t end = 0;


  if (intfr & DMA1_IT_TC2) {
    // Transfer complete, advance column counter and update second half of the buffer.
    // TODO, handle the case where BUF_SIZE < PWM_COUNT
    active_column = next_column;

    // update GPIO CFGLRs to set the new active column to PP, and reset the others to OD
    // briefly set OUTDR before doing this to turn everything off while the data registers are updated
    GPIOC->OUTDR = 0xff;

    #ifdef MATRIX_EXTRA_LED_GPIOD
    GPIOD->OUTDR = 0xff & ~(1 << MATRIX_EXTRA_LED_GPIOD);
    #else
    GPIOD->OUTDR = 0xff;
    #endif

    // Configure pins as push-pull (current column) or open-drain (all others)
    #ifdef MATRIX_INDEPENDENT_PORTS
      // port C and port D are independent matrices and both need to be updated at the same time
      // as two columns can be active at the same time.
      GPIOC->CFGLR = RESET_GPIOC_CFGLR;
      GPIOD->CFGLR = RESET_GPIOD_CFGLR;

      if (active_column < MATRIX_NUM_GPIOC_PINS) {
        pin = MATRIX_GPIOC_PINS[active_column];
        GPIOC->CFGLR = RESET_GPIOC_CFGLR & ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * pin);
        GPIOC->CFGLR |= (GPIO_CFGLR_OUT_10Mhz_PP) << 4 * pin;
      }

      if (active_column < MATRIX_NUM_GPIOD_PINS) {
        pin = MATRIX_GPIOD_PINS[active_column];
        GPIOD->CFGLR = RESET_GPIOD_CFGLR & ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * pin);
        GPIOD->CFGLR |= (GPIO_CFGLR_OUT_10Mhz_PP) << 4 * pin;
      }
    #else
      // port C and port D are part of one single matrix, update the currently active port
      if (active_column < MATRIX_NUM_GPIOC_PINS) {
        GPIOD->CFGLR = RESET_GPIOD_CFGLR;

        pin = MATRIX_GPIOC_PINS[active_column];
        GPIOC->CFGLR = RESET_GPIOC_CFGLR & ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * pin);
        GPIOC->CFGLR |= (GPIO_CFGLR_OUT_10Mhz_PP) << 4 * pin;
      } else {
        GPIOC->CFGLR = RESET_GPIOC_CFGLR;

        pin = MATRIX_GPIOD_PINS[active_column - MATRIX_NUM_GPIOC_PINS];
        GPIOD->CFGLR = RESET_GPIOD_CFGLR & ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * pin);
        GPIOD->CFGLR |= (GPIO_CFGLR_OUT_10Mhz_PP) << 4 * pin;
      }
    #endif

    #ifdef MATRIX_EXTRA_LED_GPIOD
      // Also configure the 'extra' single LED on port D as push pull output in column 0
      if (active_column == 0) {
        GPIOD->CFGLR &= ~((GPIO_CFGLR_CNF0 | GPIO_CFGLR_MODE0) << 4 * MATRIX_EXTRA_LED_GPIOD);
        GPIOD->CFGLR |= (GPIO_CFGLR_OUT_10Mhz_PP) << 4 * MATRIX_EXTRA_LED_GPIOD;
      }
    #endif
    
    // Update the second half of the bitstream for the current column.
    start = HALF_BUF_SIZE;
    end = BUF_SIZE;
  }

  if (intfr & DMA1_IT_HT2) {
    // Update the first half of the bitstream for the next column.
    start = 0;
    end = HALF_BUF_SIZE;

    // swap buffers here as we are starting the next buffer from scratch
    // TODO this allows a swap after any column, we may want to limit this to swap at the start of the picture only.
    if (buffer_swap_flag) {
      current_frame_buffer = next_frame_buffer;
      buffer_swap_flag = 0;
    }
  }

  if (end > start) {
    for (i = start; i < end; i++) {
      // Fill port C buffer
      // PWM_BUF_GPIOC[i] = 0xff;
      //  for (j = 0; j < MATRIX_NUM_GPIOC_PINS; j++) {
      //    bit = (current_frame_buffer[ROWS_GPIOC[next_column][j]] >> MATRIX_SHIFT_IN_ISR > i);
      //    PWM_BUF_GPIOC[i] &= ~(bit << MATRIX_GPIOC_PINS[j]);
      //  }
      
      // Shift the current buffer index to account for the matrix supporting fewer bits than the frame buffer
      level = i << (8 - MATRIX_PWM_BITS);

      // Expands to a mask with one bit cleared if the corresponding pin should be low at this point.
      // - BIT refers to the n'th used pin on the given PORT.
      // - Looks up the frame buffer index corresponding to the given pin and next column from the ROWS array filled in matrix_setup,
      //   and the required PWM value in the current frame buffer.
      #define MASK_ROW_BIT(PORT, BIT) ~(((current_frame_buffer[ROWS_GPIO##PORT[next_column][BIT]] > level) ? 1 : 0) << MATRIX_GPIO##PORT##_##BIT)
      
      // AND all the masks for Port C, up to the configured number of port C pins
      PWM_BUF_GPIOC[i] = 
        MASK_ROW_BIT(C, 0)
        & MASK_ROW_BIT(C, 1)
        & MASK_ROW_BIT(C, 2)
        & MASK_ROW_BIT(C, 3)
        #ifdef MATRIX_GPIOC_4
        & MASK_ROW_BIT(C, 4)
        #endif
        #ifdef MATRIX_GPIOC_5
        & MASK_ROW_BIT(C, 5)
        #endif
        #ifdef MATRIX_GPIOC_6
        & MASK_ROW_BIT(C, 6)
        #endif
        #ifdef MATRIX_GPIOC_7
        & MASK_ROW_BIT(C, 7)
        #endif
      ;

      // Fill port D buffer the same way
      // PWM_BUF_GPIOD[i] = 0xff;
      // for (j = 0; j < MATRIX_NUM_GPIOD_PINS; j++) {
      //   bit = (current_frame_buffer[ROWS_GPIOD[next_column][j]] >> MATRIX_SHIFT_IN_ISR > i);
      //   PWM_BUF_GPIOD[i] &= ~(bit << MATRIX_GPIOD_PINS[j]);
      // }
      PWM_BUF_GPIOD[i] =
        MASK_ROW_BIT(D, 0)
        & MASK_ROW_BIT(D, 1)
        & MASK_ROW_BIT(D, 2)
        & MASK_ROW_BIT(D, 3)
        #ifdef MATRIX_GPIOD_4
        & MASK_ROW_BIT(D, 4)
        #endif
        #ifdef MATRIX_GPIOD_5
        & MASK_ROW_BIT(D, 5)
        #endif
        #ifdef MATRIX_GPIOD_6
        & MASK_ROW_BIT(D, 6)
        #endif
        #ifdef MATRIX_GPIOD_7
        & MASK_ROW_BIT(D, 7)
        #endif
      ;

      // Additionally, if next column is 0, enable the 'extra' LED with it
      #ifdef MATRIX_EXTRA_LED_GPIOD
        if (next_column == 0 && current_frame_buffer[EXTRA_GPIOD] > level) {
          // Contrary to the matrix LEDs, this one is active high so when the LED should be on, the pin sould be be driven high.
          PWM_BUF_GPIOD[i] |= 1 << MATRIX_EXTRA_LED_GPIOD;
        } else {
          PWM_BUF_GPIOD[i] &= ~(1 << MATRIX_EXTRA_LED_GPIOD);
        }
      #endif

      // However, ensure the column pin is always high
      #ifdef MATRIX_INDEPENDENT_PORTS
        if (next_column < MATRIX_NUM_GPIOC_PINS) {
          PWM_BUF_GPIOC[i] |= 1 << MATRIX_GPIOC_PINS[next_column];
        }
        if (next_column < MATRIX_NUM_GPIOD_PINS) {
          PWM_BUF_GPIOD[i] |= 1 << MATRIX_GPIOD_PINS[next_column];
        }
      #else
        if (next_column < MATRIX_NUM_GPIOC_PINS) {
          PWM_BUF_GPIOC[i] |= 1 << MATRIX_GPIOC_PINS[next_column];
        } else {
          PWM_BUF_GPIOD[i] |= 1 << MATRIX_GPIOD_PINS[next_column - MATRIX_NUM_GPIOC_PINS];
        }
      #endif
    }
  }
}