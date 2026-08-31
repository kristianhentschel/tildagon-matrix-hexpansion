all : flash

TARGET:=main

TARGET_MCU?=CH32V006
ADDITIONAL_C_FILES += ../shared/matrix.c
ADDITIONAL_C_FILES += ../shared/hexpansion_header.c
ADDITIONAL_C_FILES += ../shared/i2c_again.c

include ../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean

