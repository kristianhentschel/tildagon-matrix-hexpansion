all : flash

TARGET:=main

TARGET_MCU?=CH32V006
ADDITIONAL_C_FILES += ../shared/matrix.c
ADDITIONAL_C_FILES += ../shared/hexpansion_header.c
ADDITIONAL_C_FILES += ../shared/i2c_again.c

# EXTRA_CFLAGS+=-DMATRIX_INDEPENDENT_PORTS
# EXTRA_CFLAGS+=-DMATRIX_NUM_GPIOC_PINS=6
# EXTRA_CFLAGS+=-DMATRIX_NUM_GPIOD_PINS=6
# EXTRA_CFLAGS+=-DMATRIX_NUM_LEDS=60

include ../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean

