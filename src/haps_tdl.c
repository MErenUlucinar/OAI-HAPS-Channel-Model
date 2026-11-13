#include "haps_tdl.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

#include "haps_random.h"

#define HAPS_TWO_PI 6.28318530717958647692

typedef struct {
  double delay;
  double power_db;
  bool specular;
} haps_tdl_entry_t;

typedef struct {
  unsigned count;
  haps_tdl_entry_t entries[HAPS_TDL_MAX_PATHS];
} haps_tdl_profile_data_t;

/* 3GPP TR 38.811 V15.3.0, tables 6.9.2-1 through 6.9.2-4.
 * The first Ricean tap in C/D is represented by two co-delayed paths.
 */
static const haps_tdl_profile_data_t profiles[] = {
  [HAPS_NTN_TDL_A] = {3, {{0, 0, false}, {1.0811, -4.675, false}, {2.8416, -6.482, false}}},
  [HAPS_NTN_TDL_B] = {4, {{0, 0, false}, {0.7249, -1.973, false},
                         {0.7410, -4.332, false}, {5.7392, -11.914, false}}},
  [HAPS_NTN_TDL_C] = {3, {{0, -0.394, true}, {0, -10.618, false}, {14.8124, -23.373, false}}},
  [HAPS_NTN_TDL_D] = {4, {{0, -0.284, true}, {0, -11.991, false},
                         {0.5596, -9.887, false}, {7.3340, -16.771, false}}},
};

int haps_tdl_init(haps_tdl_t *tdl, const haps_config_t *cfg, bool is_uplink)
{
  const double fs = cfg->sample_rate_hz;
  const double fd = cfg->tdl_max_doppler_hz;
  if (cfg->tdl_profile < HAPS_NTN_TDL_A || cfg->tdl_profile > HAPS_NTN_TDL_D ||
      !isfinite(fs) || fs <= 0 || !(fd >= 0 && fd <= fs / 2) ||
      !isfinite(cfg->delay_spread_s) || cfg->delay_spread_s < 0 ||
      !isfinite(cfg->update_period_s) || cfg->update_period_s <= 0)
    return -1;

  const haps_tdl_profile_data_t *profile = &profiles[cfg->tdl_profile];
  double powers[HAPS_TDL_MAX_PATHS];
  double diffuse_power = 0;
  for (unsigned n = 0; n < profile->count; ++n) {
    powers[n] = pow(10, profile->entries[n].power_db / 10);
    if (!profile->entries[n].specular)
      diffuse_power += powers[n];
  }

  if (profile->entries[0].specular && !isnan(cfg->k_factor_db)) {
    if (!isfinite(cfg->k_factor_db))
      return -1;
    /* TR 38.901 §7.7.6 uses specular power divided by total diffuse power.
     * Express the two fractions directly to avoid overflow at extreme K.
     */
    const double inverse_ratio = pow(10, -fabs(cfg->k_factor_db) / 10);
    const double weaker_fraction = inverse_ratio / (1 + inverse_ratio);
    const double diffuse_fraction = cfg->k_factor_db >= 0 ? weaker_fraction : 1 - weaker_fraction;
    powers[0] = cfg->k_factor_db >= 0 ? 1 - weaker_fraction : weaker_fraction;
    for (unsigned n = 1; n < profile->count; ++n)
      powers[n] *= diffuse_fraction / diffuse_power;
  } else {
    const double total = profile->entries[0].specular ? diffuse_power + powers[0] : diffuse_power;
    for (unsigned n = 0; n < profile->count; ++n)
      powers[n] /= total;
  }

  double mean_delay = 0;
  for (unsigned n = 0; n < profile->count; ++n)
    mean_delay += powers[n] * profile->entries[n].delay;

  double variance = 0;
  for (unsigned n = 0; n < profile->count; ++n) {
    const double delta = profile->entries[n].delay - mean_delay;
    variance += powers[n] * delta * delta;
  }
  if (cfg->delay_spread_s > 0 && variance <= 0)
    return -1;

  const double delay_scale = cfg->delay_spread_s > 0 ? cfg->delay_spread_s / sqrt(variance) : 0;
  double step = fmin(cfg->update_period_s * fs, UINT32_MAX);
  if (fd > 0)
    step = fmin(step, fs / fd / 16);
  *tdl = (haps_tdl_t){.path_count = profile->count, .step_samples = (uint32_t)fmax(1, floor(step))};

  /* Keep the fast-fading stream independent of the large-scale RNG and link direction. */
  uint32_t seed = haps_seed_mix(cfg->random_seed ^ (is_uplink ? 0xd1b54a35U : 0x94d049bbU));
  const double doppler_per_step = fd / fs * tdl->step_samples;
  for (unsigned n = 0; n < profile->count; ++n) {
    const haps_tdl_entry_t *entry = &profile->entries[n];
    haps_tdl_path_t *path = &tdl->paths[n];
    const double samples = floor(entry->delay * delay_scale * fs + 0.5);
    if (!(samples >= 0 && samples < INT_MAX))
      return -1;
    path->delay_samples = (unsigned)samples;
    if (path->delay_samples > tdl->history_samples)
      tdl->history_samples = path->delay_samples;
    path->tone_count = entry->specular ? 1 : HAPS_TDL_DIFFUSE_TONES;
    path->tone_amplitude = sqrt(powers[n] / path->tone_count);
    for (unsigned k = 0; k < path->tone_count; ++k) {
      /* One randomized angle per stratum on [0, pi] samples the Jakes spectrum. */
      const double angle = (HAPS_TWO_PI / 2) * (k + haps_uniform_open01(&seed)) / path->tone_count;
      const double doppler = entry->specular ? 0.7 : cos(angle);
      path->tones[k] = (haps_tdl_tone_t){haps_uniform_open01(&seed), doppler * doppler_per_step};
    }
  }
  return 0;
}

static haps_complex_t sample_path(const haps_tdl_path_t *path, long double interval)
{
  haps_complex_t value = {0};
  for (unsigned k = 0; k < path->tone_count; ++k) {
    const haps_tdl_tone_t *tone = &path->tones[k];
    const double angle = HAPS_TWO_PI * (double)fmodl(tone->phase + interval * tone->cycles_per_step, 1);
    value.r += cos(angle);
    value.i += sin(angle);
  }
  value.r *= path->tone_amplitude;
  value.i *= path->tone_amplitude;
  return value;
}

void haps_tdl_coefficients(haps_tdl_t *tdl, uint64_t timestamp, haps_complex_t coefficients[HAPS_TDL_MAX_PATHS])
{
  const uint64_t interval = timestamp / tdl->step_samples;
  if (!tdl->coefficients_valid || tdl->interval != interval) {
    for (unsigned n = 0; n < tdl->path_count; ++n) {
      tdl->current[n] = tdl->coefficients_valid && interval > 0 && tdl->interval == interval - 1
                        ? tdl->next[n] : sample_path(&tdl->paths[n], interval);
      tdl->next[n] = sample_path(&tdl->paths[n], (long double)interval + 1);
    }
    tdl->interval = interval;
    tdl->coefficients_valid = true;
  }
  const double fraction = (double)(timestamp % tdl->step_samples) / tdl->step_samples;
  for (unsigned n = 0; n < tdl->path_count; ++n) {
    coefficients[n].r = tdl->current[n].r + fraction * (tdl->next[n].r - tdl->current[n].r);
    coefficients[n].i = tdl->current[n].i + fraction * (tdl->next[n].i - tdl->current[n].i);
  }
}
