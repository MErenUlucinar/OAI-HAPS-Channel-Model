#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "haps_channel.h"
#include "haps_tdl.h"

static void check_atmosphere(haps_config_t cfg)
{
  haps_geometry_result_t geometry = {.slant_range_m = 20000, .elevation_deg = 30};
  haps_propagation_result_t result;
  cfg.enable_cloud = true;
  cfg.enable_scintillation = true;
  cfg.cloud_liquid_water_kg_m2 = 0.5;

  assert(haps_compute_propagation(&cfg, &geometry, &result) == 0);
  assert(result.cloud_loss_db > 0 && result.scintillation_loss_db == 0);

  cfg.carrier_frequency_hz = 20e9;
  assert(haps_compute_propagation(&cfg, &geometry, &result) == 0);
  /* Independent reference values from P.840-9 §3.1 and P.618-14 §2.4.1. */
  printf("20 GHz, 30 deg: cloud=%.12f dB scintillation=%.12f dB\n",
         result.cloud_loss_db, result.scintillation_loss_db);
  assert(fabs(result.cloud_loss_db - 0.33133039597737696) < 1e-12);
  assert(fabs(result.scintillation_loss_db - 0.33840271753420476) < 1e-12);
  assert(fabs(result.total_loss_db - result.cloud_loss_db - result.scintillation_loss_db) < 1e-12);

  haps_channel_ctx_t *ctx = haps_channel_create(&cfg, 1, 1, false, 0, 20e9);
  assert(ctx != NULL && haps_channel_get_history_samples(ctx) == 0);
  assert(haps_channel_get_snapshot(ctx, &geometry, &result) == 0);
  c16_t samples[] = {{1000, -50}};
  cf_t actual[1] = {0};
  c16_t *input[] = {samples};
  cf_t *output[] = {actual};
  assert(haps_channel_process(ctx, input, 1, output, 1, 1, 0) == 0);
  const double gain = pow(10, -(result.cloud_loss_db + result.scintillation_loss_db) / 20);
  assert(fabs(actual[0].r - 1000 * gain) < 5e-5);
  assert(fabs(actual[0].i + 50 * gain) < 5e-6);
  haps_channel_destroy(ctx);

  geometry.elevation_deg = 4;
  assert(haps_compute_propagation(&cfg, &geometry, &result) != 0);
  cfg.enable_scintillation = false;
  cfg.cloud_liquid_water_kg_m2 = -1;
  assert(haps_compute_propagation(&cfg, &geometry, &result) != 0);
  cfg.enable_cloud = false;
  assert(haps_compute_propagation(&cfg, &geometry, &result) == 0);
  assert(result.cloud_loss_db == 0 && result.scintillation_loss_db == 0 && result.total_loss_db == 0);
}

static void check_profiles(haps_config_t cfg)
{
  /* Check the PDP normalization, RMS delay and direction-specific replay. */
  const unsigned delays[][4] = {{0, 7, 17, 0}, {0, 4, 5, 35}, {0, 0, 91, 0}, {0, 0, 3, 45}};
  const unsigned path_counts[] = {3, 4, 3, 4};
  for (int profile = HAPS_NTN_TDL_A; profile <= HAPS_NTN_TDL_D; ++profile) {
    cfg.tdl_profile = profile;
    haps_tdl_t tdl;
    assert(haps_tdl_init(&tdl, &cfg, false) == 0);
    assert(tdl.path_count == path_counts[profile]);
    double mean = 0;
    double mean_square = 0;
    double total_power = 0;
    for (unsigned p = 0; p < tdl.path_count; ++p) {
      const haps_tdl_path_t *path = &tdl.paths[p];
      const double power = path->tone_count * path->tone_amplitude * path->tone_amplitude;
      const double delay = path->delay_samples / cfg.sample_rate_hz;
      total_power += power;
      mean += power * delay;
      mean_square += power * delay * delay;
      assert(path->delay_samples == delays[profile][p]);
    }
    assert(fabs(total_power - 1) < 1e-12);
    assert(fabs(sqrt(mean_square - mean * mean) - cfg.delay_spread_s) <= 0.5 / cfg.sample_rate_hz);
    haps_complex_t initial[HAPS_TDL_MAX_PATHS], later[HAPS_TDL_MAX_PATHS], repeated[HAPS_TDL_MAX_PATHS];
    haps_tdl_coefficients(&tdl, 0, initial);
    haps_tdl_coefficients(&tdl, 100000, later);
    haps_tdl_coefficients(&tdl, 0, repeated);
    assert(initial[0].r == repeated[0].r && initial[0].i == repeated[0].i);
    assert(hypot(initial[0].r - later[0].r, initial[0].i - later[0].i) > 1e-3);
    assert(haps_tdl_init(&tdl, &cfg, true) == 0);
    haps_tdl_coefficients(&tdl, 0, repeated);
    assert(hypot(initial[0].r - repeated[0].r, initial[0].i - repeated[0].i) > 1e-3);
  }

  cfg.tdl_profile = HAPS_NTN_TDL_C;
  cfg.k_factor_db = 0;
  haps_tdl_t tdl;
  assert(haps_tdl_init(&tdl, &cfg, false) == 0);
  assert(fabs(tdl.paths[0].tone_amplitude * tdl.paths[0].tone_amplitude - 0.5) < 1e-12);
  cfg.tdl_profile = HAPS_NTN_TDL_INVALID;
  assert(haps_tdl_init(&tdl, &cfg, false) != 0);
}

static void check_iq(haps_config_t cfg)
{
  cfg.enable_fast_fading = true;
  cfg.enable_propagation_delay = true;
  cfg.enable_doppler = true;
  cfg.fixed_doppler_hz = -140;
  cfg.tdl_max_doppler_hz = 500;
  cfg.update_period_s = 1e-6;
  cfg.tdl_profile = HAPS_NTN_TDL_D;
  haps_channel_ctx_t *ctx = haps_channel_create(&cfg, 1, 1, false, 0, 0);
  haps_channel_ctx_t *split_ctx = haps_channel_create(&cfg, 1, 1, false, 0, 0);
  assert(ctx != NULL && split_ctx != NULL);
  haps_delay_info_t delay;
  assert(haps_channel_get_delay_info(ctx, &delay) == 0 && delay.applied_integer_samples == 4099);
  const unsigned history = haps_channel_get_history_samples(ctx);
  assert(history == 45);
  enum { count = 256, split = 113 };
  c16_t *samples = calloc(history + count, sizeof(*samples));
  assert(samples != NULL);
  samples[history] = (c16_t){1000, -300};
  cf_t whole[count] = {0}, pieces[count] = {0};
  c16_t *input[] = {samples};
  cf_t *output[] = {whole};
  const uint64_t timestamp = 123456789;
  assert(haps_channel_process(ctx, input, 1, output, 1, count, timestamp) == 0);
  output[0] = pieces;
  assert(haps_channel_process(split_ctx, input, 1, output, 1, split, timestamp) == 0);
  output[0] = pieces + split;
  input[0] = samples + split;
  assert(haps_channel_process(split_ctx, input, 1, output, 1, count - split, timestamp + split) == 0);

  haps_tdl_t tdl;
  assert(haps_tdl_init(&tdl, &cfg, false) == 0);
  for (unsigned n = 0; n < count; ++n) {
    assert(fabs(whole[n].r - pieces[n].r) < 1e-4);
    assert(fabs(whole[n].i - pieces[n].i) < 1e-4);
    haps_complex_t taps[HAPS_TDL_MAX_PATHS];
    haps_tdl_coefficients(&tdl, timestamp + n, taps);
    double real = 0, imag = 0;
    for (unsigned k = 0; k < tdl.path_count; ++k) {
      if (n == tdl.paths[k].delay_samples) {
        real += 1000 * taps[k].r + 300 * taps[k].i;
        imag += 1000 * taps[k].i - 300 * taps[k].r;
      }
    }
    const double phase = 6.28318530717958647692 * (double)(timestamp + n) * cfg.fixed_doppler_hz / cfg.sample_rate_hz;
    assert(fabs(whole[n].r - (real * cos(phase) - imag * sin(phase))) < 1e-4);
    assert(fabs(whole[n].i - (real * sin(phase) + imag * cos(phase))) < 1e-4);
  }
  free(samples);
  haps_channel_destroy(split_ctx);
  haps_channel_destroy(ctx);
}

int main(void)
{
  haps_config_t cfg = {
    .carrier_frequency_hz = 3619200000,
    .sample_rate_hz = 61440000,
    .haps_altitude_m = 20000,
    .scintillation_nwet = 50,
    .scintillation_antenna_diameter_m = 0.3,
    .scintillation_antenna_efficiency = 0.5,
    .scintillation_exceedance_percent = 1,
    .delay_spread_s = 1e-7,
    .k_factor_db = NAN,
    .tdl_max_doppler_hz = 30,
    .random_seed = 1,
    .update_period_s = 0.001,
  };
  check_atmosphere(cfg);
  check_profiles(cfg);
  check_iq(cfg);
  puts("PASS: atmosphere, NTN-TDL A–D, K-factor, IQ impulse, delay and block continuity");
  return 0;
}
