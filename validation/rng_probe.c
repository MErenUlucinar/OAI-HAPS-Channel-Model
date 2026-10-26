/* Standalone propagation/RNG probe; this does not run OAI RFsim. */
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "haps_propagation.h"

int main(int argc, char **argv)
{
  if (argc != 7 && argc != 8) {
    fprintf(stderr, "usage: rng_probe dense_urban|rural elevation prob|los|nlos clutter sf count [first_seed]\n");
    return 2;
  }
  haps_config_t cfg = {0};
  haps_geometry_result_t geometry = {0};
  if (strcmp(argv[1], "dense_urban") == 0)
    cfg.scenario = HAPS_SCENARIO_DENSE_URBAN;
  else if (strcmp(argv[1], "rural") == 0)
    cfg.scenario = HAPS_SCENARIO_RURAL;
  else
    return 2;
  if (strcmp(argv[3], "prob") == 0)
    cfg.los_mode = HAPS_LOS_PROBABILISTIC;
  else if (strcmp(argv[3], "los") == 0)
    cfg.los_mode = HAPS_LOS_FORCED;
  else if (strcmp(argv[3], "nlos") == 0)
    cfg.los_mode = HAPS_NLOS_FORCED;
  else
    return 2;
  geometry.elevation_deg = strtod(argv[2], NULL);
  /* Synthetic distance: these tests check propagation/RNG, not geometry. */
  geometry.slant_range_m = 20000.0;
  cfg.carrier_frequency_hz = 3.6192e9;
  cfg.sample_rate_hz = 61.44e6;
  cfg.terminal_type = HAPS_TERMINAL_OUTDOOR;
  cfg.enable_fspl = true;
  cfg.enable_clutter = atoi(argv[4]) != 0;
  cfg.enable_shadow_fading = atoi(argv[5]) != 0;
  cfg.link_budget_offset_db = 129.64;
  const uint64_t count = strtoull(argv[6], NULL, 10);
  const uint64_t first = argc == 8 ? strtoull(argv[7], NULL, 10) : 1;
  if (count == 0 || count > 1000000 || first > UINT32_MAX || count - 1 > UINT32_MAX - first)
    return 2;
  puts("seed,plos,draw,los,sigma_sf,sf,cl,total,gain");
  for (uint64_t i = 0; i < count; i++) {
    haps_propagation_result_t result;
    cfg.random_seed = (uint32_t)(first + i);
    const int rc = haps_compute_propagation(&cfg, &geometry, &result);
    if (rc != 0) {
      fprintf(stderr, "haps_compute_propagation failed: %d\n", rc);
      return 1;
    }
    printf("%" PRIu32 ",%.17g,%.17g,%d,%.17g,%.17g,%.17g,%.17g,%.17g\n",
           cfg.random_seed, result.los_probability, result.los_uniform_draw,
           (int)result.is_los, result.shadow_sigma_db, result.shadow_fading_db,
           result.clutter_loss_db, result.total_loss_db, result.linear_amplitude_gain);
  }
  return 0;
}
