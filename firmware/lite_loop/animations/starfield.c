#include "ch32fun.h"
#include "starfield.h"
#include "leds.h"
#include "stdbool.h"

#define RANDOM_STRENGTH 3
#include "lib_rand.h"

typedef struct particle {
  float r;
  float phi;
  float vr;
  float vphi;
} particle_t;

#define NUM_PARTICLES 8
static particle_t particles[NUM_PARTICLES] = {};
int next = 0;

static bool init = false;

static float phi_min = 360;
static float phi_max = -360;
static float r_min = 1000;
static float r_max = 0;
static float r_range;
static float phi_range;
static float phi_threshold;
static float r_threshold;

#define MAX_UINT32 ((uint32_t)0xffffffff)

void animation_starfield(core_registers_t *core_registers, matrix_pwm_t *frame_buffer) {
  if (!init) {
    for (int i = 0; i < LEDS_COUNT; i++) {
      float r = leds_polar_positions[i].radius;
      float phi = leds_polar_positions[i].angle;
      if (phi > phi_max) {
        phi_max = phi;
      }
      if (phi < phi_min) {
        phi_min = phi;
      }
      if (r > r_max) {
        r_max = r;
      }
      if (r < r_min) {
        r_min = r;
      }
    }

    r_range = r_max - r_min;
    phi_range = phi_max - phi_min;

    r_threshold = 2 * r_range / LEDS_GRID_ROWS;
    phi_threshold = 0.5 * phi_range / LEDS_GRID_COLS;
    

    init = true;
  }

  // Generate new random particles, replacing all that have gone out of range
  seed(SysTick->CNT);

  for (int i = 0; i < NUM_PARTICLES; i++) {
    particle_t *p = &particles[i];
    if (p->r >= r_min && p->r <= r_max) {
      continue;
    }

    p->r = r_min; // + rand() * 1.0f / MAX_UINT32 * r_range;
    p->phi = phi_min + rand() * 1.0f / MAX_UINT32 * phi_range;
    p->vphi = 0;
    p->vr = (rand() * 1.0f / MAX_UINT32 + 0.1) * r_range * 0.1f;
  }

  // Update matrix
  for (int i = 0; i < MATRIX_NUM_LEDS; i++) {
    frame_buffer[i] = 0;
    polar_position_t pos = leds_polar_positions[i];
    for (int j = 0; j < NUM_PARTICLES; j++) {
      particle_t *p = &particles[j];
      float dphi = pos.angle - p->phi;
      dphi = dphi < 0 ? -1 * dphi : dphi;
      float dr = pos.radius - p->r;
      // dr = dr < 0 ? -1 * dr : dr;

      if (dphi < phi_threshold && dr < r_threshold) {
        frame_buffer[i] = 255 * (dphi / phi_threshold) * (dr / r_threshold * p->vr);
      }
    }
  }

  // advance all particles
  // TODO scale by dt
  for (int i = 0; i < NUM_PARTICLES; i++) {
    particle_t *p = &particles[i];
    p->r += p->vr;
    p->phi += p->vphi;
  }

  // Delay_Ms(16);
}