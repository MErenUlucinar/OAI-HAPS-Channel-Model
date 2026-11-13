#ifndef HAPS_TDL_H
#define HAPS_TDL_H

#include "haps_config.h"

#define HAPS_TDL_MAX_PATHS 4
#define HAPS_TDL_DIFFUSE_TONES 16

typedef struct {
  double r;
  double i;
} haps_complex_t;

typedef struct {
  double phase;
  double cycles_per_step;
} haps_tdl_tone_t;

typedef struct {
  unsigned delay_samples;
  double tone_amplitude;
  unsigned tone_count;
  haps_tdl_tone_t tones[HAPS_TDL_DIFFUSE_TONES];
} haps_tdl_path_t;

typedef struct {
  unsigned history_samples;
  unsigned path_count;
  uint32_t step_samples;
  uint64_t interval;
  bool coefficients_valid;
  haps_tdl_path_t paths[HAPS_TDL_MAX_PATHS];
  haps_complex_t current[HAPS_TDL_MAX_PATHS];
  haps_complex_t next[HAPS_TDL_MAX_PATHS];
} haps_tdl_t;

int haps_tdl_init(haps_tdl_t *tdl, const haps_config_t *cfg, bool is_uplink);
void haps_tdl_coefficients(haps_tdl_t *tdl, uint64_t timestamp,
                           haps_complex_t coefficients[HAPS_TDL_MAX_PATHS]);

#endif
