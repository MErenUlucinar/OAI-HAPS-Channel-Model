/* Compile with test_delay.py; exercises the actual channel implementation. */
#include <assert.h>
#include <inttypes.h>
#include "haps_channel.c"

static void boundary_checks(void)
{
  /* Binary-exact inputs test both sides of a tie and a sub-sample delay. */
  const double samples[] = {.25, .5, .75, 1, 1.25, 1.5, 31.4990234375, 31.5, 31.5009765625};
  const uint64_t nearest[] = {0, 1, 1, 1, 1, 2, 31, 32, 32};
  const uint64_t upward[] = {1, 1, 1, 1, 2, 2, 32, 32, 32};
  for (unsigned i = 0; i < sizeof(samples)/sizeof(samples[0]); ++i) {
    haps_channel_ctx_t ctx = {0};
    ctx.cfg.enable_propagation_delay = true;
    ctx.cfg.sample_rate_hz = 1024;
    ctx.geometry.propagation_delay_s = samples[i]/1024;
    ctx.geometry.slant_range_m = ctx.geometry.propagation_delay_s*299792458;
    assert(haps_configure_integer_delay(&ctx) == 0);
    assert(ctx.delay.exact_samples == samples[i]);
#ifdef HAPS_DELAY_ROUNDING_CEIL
    assert(ctx.delay.applied_integer_samples == upward[i]);
#else
    assert(ctx.delay.applied_integer_samples == nearest[i]);
#endif
    assert(ctx.delay.floor_samples == (uint64_t)samples[i]);
    assert(fabs(ctx.delay.fractional_samples-(samples[i]-(uint64_t)samples[i])) < 1e-15);
  }
  (void)nearest;
  (void)upward;
  haps_channel_ctx_t disabled = {0};
  assert(haps_configure_integer_delay(&disabled) == 0);
  assert(!disabled.delay.enabled && disabled.delay.applied_integer_samples == 0);
}

int main(void)
{
  boundary_checks();
  const int elevations[] = {90, 60, 30, 10};
  const double longitudes[] = {32.859700000, 32.994297811, 33.261831217, 34.122126213};
  const uint64_t nearest[] = {4099, 4730, 8160, 22525};
  const uint64_t upward[] = {4099, 4731, 8160, 22526};
  for (unsigned i = 0; i < 4; ++i) {
    haps_config_t cfg = {0};
    strcpy(cfg.model_name, "delay_regression");
    cfg.carrier_frequency_hz = 3619200000;
    cfg.sample_rate_hz = 61440000;
    cfg.haps_latitude_deg = cfg.ue_latitude_deg = 39.9334;
    cfg.haps_longitude_deg = 32.8597;
    cfg.haps_altitude_m = 20000;
    cfg.ue_longitude_deg = longitudes[i];
    cfg.scenario = HAPS_SCENARIO_RURAL;
    cfg.los_mode = HAPS_LOS_FORCED;
    cfg.terminal_type = HAPS_TERMINAL_OUTDOOR;
    cfg.bel_percentile = .01;
    cfg.enable_fspl = true;
    cfg.enable_propagation_delay = true;
    cfg.link_budget_offset_db = 129.64;
    cfg.random_seed = 12345;
    haps_channel_ctx_t *ctx = haps_channel_create(&cfg, 1, 1, false, cfg.sample_rate_hz, cfg.carrier_frequency_hz);
    assert(ctx != NULL);
    haps_delay_info_t delay;
    assert(haps_channel_get_delay_info(ctx, &delay) == 0);
#ifdef HAPS_DELAY_ROUNDING_CEIL
    assert(delay.applied_integer_samples == upward[i]);
    assert(delay.quantization_error_s >= 0 && delay.quantization_error_s*cfg.sample_rate_hz < 1);
#else
    assert(delay.applied_integer_samples == nearest[i]);
    assert(fabs(delay.quantization_error_s*cfg.sample_rate_hz) <= .5+1e-10);
    assert(fabs(2*delay.quantization_error_s*cfg.sample_rate_hz) < 1);
#endif
    c16_t input[4] = {{1000,-2000},{1,-1},{300,400},{0,0}};
    c16_t *inputs[1] = {input};
    cf_t output[4] = {{0}};
    cf_t *outputs[1] = {output};
    assert(haps_channel_process(ctx, inputs, 1, outputs, 1, 4, 100000) == 0);
    const double gain = ctx->propagation.linear_amplitude_gain;
    for (unsigned n = 0; n < 4; ++n) {
      assert(fabs(output[n].r-input[n].r*gain) < .001);
      assert(fabs(output[n].i-input[n].i*gain) < .001);
    }
    printf("{\"elevation_deg\":%d,\"exact_samples\":%.12f,\"applied_samples\":%" PRIu64
           ",\"applied_delay_us\":%.12f,\"quantization_error_ns\":%.12f,\"round_trip_quantization_error_samples\":%.12f,"
           "\"fspl_db\":%.12f,\"applied_loss_db\":%.12f,\"iq_gain\":%.12f}\n",
           elevations[i], delay.exact_samples, delay.applied_integer_samples,
           delay.applied_delay_s*1e6, delay.quantization_error_s*1e9,
           2*delay.quantization_error_s*cfg.sample_rate_hz,
           ctx->propagation.fspl_db, ctx->propagation.applied_loss_db, gain);
    haps_channel_destroy(ctx);
  }
  (void)nearest;
  (void)upward;
  return 0;
}
