# Matrix Hexpansion

A breakout board, CH32V006 co-processor expansion, and base to add custom LED arrangements on cheap PCBs for the Electromagnetic Field Tildagon badge.

The hexpansion can be controlled from the badge with the [Matrix Hexpansion companion app](https://github.com/kristianhentschel/tildagon-matrix-hexpansion-app).

The common base board is produced with ENIG finish and holds the processor, status LEDs, configuration jumpers, and I2C pull-up resistors. It has large through hole pads for all GPIO pins and Hexpansion connector pins, with sufficient spacing for signal routing.

![Rendering of the front and back sides of the v2-lite matrix hexpansion PCB. The trapezoidal top half has the hexpansion connector, processor, status LEDs, pull-up resistors, and small debug connector bads for programming the processor and observing the I2C bus. On the back there are additional configuration solder jumpers. The rectangular bottom half has a grid of rectangular breakout pads with circular holes for soldering to jumper wires or directly to rectangular SMD pads on a daughter board.](./doc/base-render.png)

Daughter boards are soldered to the base pads. They can be cheaper (e.g. HASL finish) and hold the LEDs. So far, there is only the `lite-loop` daughter board as an example.

![Looping video of two Matrix Hexpansions with the lite-loop daughter board plugged into the bottom expansion slots of a 2024 Tildagon Badge. The LED matrix displays a starfield pattern where specks of light move outwards from the badge center. Each hexpansion is a grid of 156 tiny orange LEDs on a black PCB arranged in a 9-row wide circular strip tangentially to the badge outline. The inner-most row has a gap of three LEDs on each side, the remaining rows join seamlessly with the adjacent board.](./doc/starfield.gif)

## Hardware

_Sorry, the hardware projects are not published yet, I'll get round to it after the camp..._

The `hardware` directory contains KiCad projects for the PCBs.

* `base-v2-lite`: The current base hexpansion board; with a CH32V006 co-processor and all GPIO pins and hexpansion interface pins broken out.
* `lite-loop`: 156-LED matrix display daughter board (13 pin charlie-plexing, arranged in 8 rows of 18 columns and a 9th row of 12 columns). Six instances of this can (almost) seamlessly surround the badge.

There is also a library of symbols and footprints to aid in making custom daughter boards.

## Firmware

The CH32V006 processor on the base board runs native firmware code built with the [ch32fun framework](https://github.com/cnlohr/ch32fun) (loaded here as a git submodule).

* `firmware/lite_loop` is the driver for the `lite-loop` daughter board. It maintains the LED matrix display and presents an I2C interface for identifying and controlling the hexpansion from the badge.
* `firmware/bootloader_006` presents an I2C EEPROM-like interface so the badge can write to the main code flash storage area of the chip and thus replace the main program with a new firmware image.

To provision a factory-fresh processor, the bootloader must first be flashed using a dedicated programmer like the WCH-LinkE (flash bootloader, enable PD7 use as GPIO, flash main program -- see the `provision.sh` script). After this, firmware updates can be applied from the badge app and a dedicated programmer is not required (but may still be helpful for debugging).

## Note on previous iteration

The base is called `base-v2-lite` because the first iteration of this also had a [IS31FL3731](https://lumissil.com/assets/pdf/core/IS31FL3731_DS.pdf) LED driver, but this was found to add unnecessary cost and complexity and was never assembled or programmed properly. In the current `v2` the current limiting resistors were moved to the daughter board and the breakout pads have a more generic layout, also including the hexpansion connector pins. I still have a batch of unpopulated `v1` boards if anyone is interested in making this work -- it will probably make for a faster and brighter display, but can only address 144 LEDs.

## Acknowledgements

The base board is derived from the [hexpansion template](https://github.com/emfcamp/badge-2024-hardware/tree/main/hexpansion) by kliment. 
