#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// Place configuration items here, you can see a full list in ch32fun/ch32fun.h
// To reconfigure to a different processor, update TARGET_MCU in the  Makefile

// Configuration for shared/matrix.h
#define MATRIX_INDEPENDENT_PORTS
#define MATRIX_NUM_GPIOC_PINS 6
#define MATRIX_NUM_GPIOD_PINS 6
#define MATRIX_NUM_LEDS 60

// Configuration for ch32fun/extra_libs/librand.h
#define RANDOM_STRENGTH 3

#endif

