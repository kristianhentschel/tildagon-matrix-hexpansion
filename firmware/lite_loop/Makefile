all : flash

TARGET:=main

TARGET_MCU?=CH32V006
ADDITIONAL_C_FILES += ../shared/matrix.c
ADDITIONAL_C_FILES += ../shared/hexpansion_header.c
ADDITIONAL_C_FILES += ../shared/i2c_again.c
ADDITIONAL_C_FILES += leds.c
ADDITIONAL_C_FILES += animations/level.c
ADDITIONAL_C_FILES += animations/starfield.c
ADDITIONAL_C_FILES += animations/text.c

include ../ch32fun/ch32fun/ch32fun.mk

# FORCE: ;
# eeprom_image.h : ../../apps/lite_loop/app.py
# 	python ../../scripts/embed_badge_app.py --size 8192 ../../apps/lite_loop/ .

flash : cv_flash
clean : cv_clean

