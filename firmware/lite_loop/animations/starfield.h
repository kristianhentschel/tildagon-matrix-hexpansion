#ifndef STARFIELD_H
#define STARFIELD_H
#include "../../shared/core_registers.h"
#include "../../shared/matrix.h"

void animation_starfield(core_registers_t *core_registers, matrix_pwm_t *frame_buffer);
#endif