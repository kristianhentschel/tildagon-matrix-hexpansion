# Boards

* `base_v2_lite` is the base board. It has to be manufactured as a 1mm board with ENIG.
* `lite_loop` is the main "Matrix Hexpansion" LED daughter board. It can be connected to the top of the base board with solder paste and a hot air tool.

# Symbols and Footprints

* `matrix-hexpansion.kicad_sym` has schematic symbols for the base board's breakout pin array, including a version that only has the first two rows of pins as used on the `lite_loop` board.
* `matrix-hexpansion.pretty` is a footprint library including THT (used on the base) and SMD (used on the daughter boards) variations of the breakout pin array. The footprints have alignment aids on silkscreen and user layers.

# Acknowledgements

The base board is derived from the [badge-2024-hardware hexpansion template](https://github.com/emfcamp/badge-2024-hardware/tree/main/hexpansion) by kliment under CERN-OHL-P. The `hexpansion-edge-connector` symbol and footprint included with this repository for convenience are also from the same project.

# Stencils for previous version

If you want to make a stencil for your soldering kit picked up at EMF Camp 2026 (marked `v2`, `2026-05-22`), here are links to the [gerber files](https://stuff.5yk.de/20260719_lite_loop_gerber.zip) and [DXF exports](https://stuff.5yk.de/20260719_lite_loop_stencil.zip).
