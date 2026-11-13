#include "haps_channel.h"
#include "haps_rain.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"

#include "haps_geometry.h"
#include "haps_propagation.h"
#include "haps_tdl.h"

#define HAPS_TWO_PI 6.28318530717958647692

static double haps_wrap_phase_rad(double phase_rad)
{
  phase_rad = fmod(phase_rad, HAPS_TWO_PI);

  if (phase_rad < 0.0)
    phase_rad += HAPS_TWO_PI;

  return phase_rad;
}

struct haps_channel_ctx_s {
  haps_config_t cfg;

  int nb_tx;
  int nb_rx;
  bool is_uplink;

  haps_geometry_result_t geometry;
  haps_propagation_result_t propagation;

  haps_delay_info_t delay;
  haps_tdl_t tdl;

  double doppler_hz;
  double doppler_phase_rad;
  double doppler_phase_increment_rad;

  uint64_t last_update_timestamp;
  uint64_t last_log_timestamp;
};

static int haps_update_rain_loss(
    haps_channel_ctx_t *ctx)
{
  if (ctx == NULL)
    return -1;

  /*
   * Always reset rain outputs before calculating.
   */
  ctx->propagation.rain_k = 0.0;
  ctx->propagation.rain_alpha = 0.0;

  ctx->propagation
      .rain_specific_attenuation_db_per_km =
      0.0;

  ctx->propagation.rain_path_km = 0.0;
  ctx->propagation.rain_loss_db = 0.0;

  if (!ctx->cfg.enable_rain ||
      ctx->cfg.rain_mode ==
          HAPS_RAIN_DISABLED) {
    return 0;
  }

  if (ctx->cfg.rain_mode !=
      HAPS_RAIN_FIXED_UNIFORM_LAYER) {

    LOG_E(
        HW,
        "HAPS RAIN: unsupported mode %d\n",
        ctx->cfg.rain_mode);

    return -2;
  }

  haps_rain_input_t rain_input = {
      .frequency_ghz =
          ctx->cfg.carrier_frequency_hz /
          1.0e9,

      .elevation_deg =
          ctx->geometry.elevation_deg,

      .polarization_tilt_deg =
          ctx->cfg
              .rain_polarization_tilt_deg,

      .rain_rate_mm_per_h =
          ctx->cfg.rain_rate_mm_per_h,

      .rain_height_km =
          ctx->cfg.rain_height_km,

      .station_height_km =
          ctx->cfg.ue_altitude_m /
          1000.0
  };

  haps_rain_result_t rain_result = {0};

  const int rain_rc =
      haps_rain_compute(
          &rain_input,
          &rain_result);

  if (rain_rc != 0 ||
      !rain_result.valid) {

    LOG_E(
        HW,
        "HAPS RAIN calculation failed: "
        "link=%s rc=%d\n",
        ctx->cfg.model_name,
        rain_rc);

    return -3;
  }

  ctx->propagation.rain_k =
      rain_result.k;

  ctx->propagation.rain_alpha =
      rain_result.alpha;

  ctx->propagation
      .rain_specific_attenuation_db_per_km =
      rain_result
          .specific_attenuation_db_per_km;

  ctx->propagation.rain_path_km =
      rain_result.geometric_rain_path_km;

  ctx->propagation.rain_loss_db =
      rain_result.attenuation_db;

  LOG_I(
      HW,
      "HAPS RAIN CONFIG: "
      "link=%s "
      "mode=P838_FIXED_LAYER "
      "f=%.6f GHz "
      "elevation=%.2f deg "
      "R=%.3f mm/h "
      "rain_height=%.3f km "
      "k=%.12f "
      "alpha=%.12f "
      "gamma_R=%.9f dB/km "
      "rain_path=%.6f km "
      "RAIN=%.6f dB\n",
      ctx->cfg.model_name,
      rain_input.frequency_ghz,
      rain_input.elevation_deg,
      rain_input.rain_rate_mm_per_h,
      rain_input.rain_height_km,
      rain_result.k,
      rain_result.alpha,
      rain_result
          .specific_attenuation_db_per_km,
      rain_result.geometric_rain_path_km,
      rain_result.attenuation_db);

  return 0;
}

static int validate_channel(
    const haps_config_t *cfg,
    int nb_tx,
    int nb_rx)
{
  if (nb_tx != 1 || nb_rx != 1) {
    LOG_E(
        HW,
        "HAPS requires one Tx and one Rx antenna. Tx=%d Rx=%d\n",
        nb_tx,
        nb_rx);

    return -1;
  }

  if (cfg->enable_fractional_delay || cfg->enable_noise) {

    LOG_E(
        HW,
        "HAPS fractional delay and internal noise are unsupported\n");

    return -1;
  }

  return 0;
}

#define HAPS_SPEED_OF_LIGHT_M_S 299792458.0

static int haps_configure_integer_delay(
    haps_channel_ctx_t *ctx)
{
  if (ctx == NULL)
    return -1;

  memset(
      &ctx->delay,
      0,
      sizeof(ctx->delay));

  ctx->delay.enabled =
      ctx->cfg.enable_propagation_delay;

  /*
   * Delay kapalıysa channel_offset sıfır kalır.
   */
  if (!ctx->delay.enabled)
    return 0;

  const double sample_rate_hz =
      ctx->cfg.sample_rate_hz;

  if (!isfinite(sample_rate_hz) ||
      sample_rate_hz <= 0.0) {

    LOG_E(
        HW,
        "HAPS DELAY: invalid sample rate %.6f Hz\n",
        sample_rate_hz);

    return -2;
  }

  /*
   * Geometri katmanında propagation_delay_s zaten
   * hesaplanıyorsa onu doğrudan kullanıyoruz.
   */
  double delay_s =
      ctx->geometry.propagation_delay_s;

  /*
   * Güvenlik amacıyla geometri alanı geçersizse
   * slant mesafesinden yeniden hesapla.
   */
  if (!isfinite(delay_s) ||
      delay_s <= 0.0) {

    const double slant_range_m =
        ctx->geometry.slant_range_m;

    if (!isfinite(slant_range_m) ||
        slant_range_m <= 0.0) {

      LOG_E(
          HW,
          "HAPS DELAY: invalid slant distance %.6f m\n",
          slant_range_m);

      return -3;
    }

    delay_s =
        slant_range_m /
        HAPS_SPEED_OF_LIGHT_M_S;
  }

  const double exact_samples =
      delay_s * sample_rate_hz;

  if (!isfinite(exact_samples) ||
      exact_samples < 0.0) {

    LOG_E(
        HW,
        "HAPS DELAY: invalid exact sample delay %.9f\n",
        exact_samples);

    return -4;
  }

  const double floor_samples_double =
      floor(exact_samples);

  ctx->delay.one_way_delay_s =
      delay_s;

  ctx->delay.exact_samples =
      exact_samples;

  ctx->delay.floor_samples =
      (uint64_t)floor_samples_double;

  ctx->delay.fractional_samples =
      exact_samples -
      floor_samples_double;

  /*
   * Integer-only version.
   *
   * Round non-negative delays to nearest: N = floor(tau*Fs + 1/2).
   * Half-sample ties round upward. The error is bounded by 1/(2 Fs)
   * (8.14 ns at 61.44 Msps), matching the manuscript's delay rule.
   * ceil remains available through HAPS_DELAY_ROUNDING_CEIL for
   * comparison with the v0.1.0/v0.1.1 delay behavior.
   */
#ifdef HAPS_DELAY_ROUNDING_CEIL
  ctx->delay.applied_integer_samples =
      (uint64_t)ceil(exact_samples);
#else
  ctx->delay.applied_integer_samples =
      (uint64_t)floor(exact_samples + 0.5);
#endif

  ctx->delay.applied_delay_s =
      (double)ctx->delay.applied_integer_samples /
      sample_rate_hz;

  ctx->delay.quantization_error_s =
      ctx->delay.applied_delay_s -
      ctx->delay.one_way_delay_s;

  LOG_I(
      HW,
      "HAPS DELAY CONFIG: "
      "link=%s enabled=1 "
      "slant=%.3f km "
      "one_way_delay=%.6f us "
      "exact_samples=%.6f "
      "floor_samples=%llu "
      "fractional=%.6f "
      "applied_integer=%llu "
      "applied_delay=%.6f us "
      "quant_error=%+.3f ns\n",
      ctx->cfg.model_name,
      ctx->geometry.slant_range_m / 1000.0,
      ctx->delay.one_way_delay_s * 1.0e6,
      ctx->delay.exact_samples,
      (unsigned long long)
          ctx->delay.floor_samples,
      ctx->delay.fractional_samples,
      (unsigned long long)
          ctx->delay.applied_integer_samples,
      ctx->delay.applied_delay_s * 1.0e6,
      ctx->delay.quantization_error_s * 1.0e9);

  return 0;
}

haps_channel_ctx_t *haps_channel_create(
    const haps_config_t *input_cfg,
    int nb_tx,
    int nb_rx,
    bool is_uplink,
    double runtime_sample_rate_hz,
    double runtime_center_frequency_hz)
{
  if (input_cfg == NULL)
    return NULL;

  if (validate_channel(input_cfg, nb_tx, nb_rx) != 0)
    return NULL;

  haps_channel_ctx_t *ctx =
      calloc(1, sizeof(*ctx));

  if (ctx == NULL)
    return NULL;

  ctx->cfg = *input_cfg;
  ctx->nb_tx = nb_tx;
  ctx->nb_rx = nb_rx;
  ctx->is_uplink = is_uplink;

  /*
   * RFsimulator'ın gerçek çalışma değerlerini tercih ediyoruz.
   */
  if (runtime_sample_rate_hz > 0.0)
    ctx->cfg.sample_rate_hz = runtime_sample_rate_hz;

  if (runtime_center_frequency_hz > 0.0)
    ctx->cfg.carrier_frequency_hz =
        runtime_center_frequency_hz;

  if (ctx->cfg.enable_fast_fading) {
    if (haps_tdl_init(&ctx->tdl, &ctx->cfg, is_uplink) != 0) {
      LOG_E(HW, "HAPS NTN-TDL: invalid profile, sample rate, delay spread, Doppler, update period or K factor\n");
      free(ctx);
      return NULL;
    }
    LOG_I(HW, "HAPS NTN-TDL CONFIG: link=%s profile=%c DS=%.3f ns max_Doppler=%.3f Hz "
              "history=%u samples coefficient_step=%u samples\n",
          ctx->cfg.model_name, 'A' + ctx->cfg.tdl_profile, ctx->cfg.delay_spread_s * 1e9,
          ctx->cfg.tdl_max_doppler_hz, ctx->tdl.history_samples, ctx->tdl.step_samples);
  }

   /*
 * Sabit Doppler NCO başlangıcı.
 */
ctx->doppler_hz =
    ctx->cfg.enable_doppler
        ? ctx->cfg.fixed_doppler_hz
        : 0.0;

ctx->doppler_phase_rad = 0.0;

if (ctx->cfg.enable_doppler) {
  if (!isfinite(ctx->cfg.sample_rate_hz) || ctx->cfg.sample_rate_hz <= 0.0 ||
      !isfinite(ctx->doppler_hz)) {
    LOG_E(
        HW,
        "HAPS Doppler requires a valid sample rate and frequency shift, got %.3f and %.3f Hz\n",
        ctx->cfg.sample_rate_hz, ctx->doppler_hz);

    free(ctx);
    return NULL;
  }

  ctx->doppler_phase_increment_rad =
      HAPS_TWO_PI *
      ctx->doppler_hz /
      ctx->cfg.sample_rate_hz;
} else {
  ctx->doppler_phase_increment_rad = 0.0;
}

  if (haps_compute_geometry(
          &ctx->cfg,
          &ctx->geometry) != 0) {

    LOG_E(HW, "HAPS geometry calculation failed\n");
    free(ctx);
    return NULL;
  }

LOG_I(
    HW,
    "HAPS DOPPLER CONFIG: "
    "link=%s direction=%s enabled=%d "
    "HAPS=(lat=%.6f lon=%.6f alt=%.1f m) "
    "UE=(lat=%.6f lon=%.6f alt=%.1f m) "
    "slant=%.3f km "
    "elevation=%.2f deg "
    "fD=%.2f Hz fs=%.6f Msps "
    "phase_step=%+.9e rad/sample "
    "phase_change_1ms=%+.6f rad\n",
    ctx->cfg.model_name,
    ctx->is_uplink ? "UL" : "DL",
    ctx->cfg.enable_doppler,
    ctx->cfg.haps_latitude_deg,
    ctx->cfg.haps_longitude_deg,
    ctx->cfg.haps_altitude_m,
    ctx->cfg.ue_latitude_deg,
    ctx->cfg.ue_longitude_deg,
    ctx->cfg.ue_altitude_m,
    ctx->geometry.slant_range_m / 1000.0,
    ctx->geometry.elevation_deg,
    ctx->doppler_hz,
    ctx->cfg.sample_rate_hz / 1.0e6,
    ctx->doppler_phase_increment_rad,
    HAPS_TWO_PI * ctx->doppler_hz * 0.001);

const int delay_result =
    haps_configure_integer_delay(ctx);

if (delay_result != 0) {
  LOG_E(
      HW,
      "HAPS delay initialization failed: %d\n",
      delay_result);

  /*
   * Burada mevcut create() fonksiyonundaki diğer
   * hata dalları hangi cleanup yöntemini kullanıyorsa
   * aynısını kullanın.
   */
  free(ctx);
  return NULL;
}

  if (haps_compute_propagation(
          &ctx->cfg,
          &ctx->geometry,
          &ctx->propagation) != 0) {

    LOG_E(HW, "HAPS propagation calculation failed\n");
    free(ctx);
    return NULL;
  }
  
    const int rain_rc =
      haps_update_rain_loss(ctx);

  if (rain_rc != 0) {

    LOG_E(
        HW,
        "HAPS rain initialization failed: "
        "link=%s rc=%d\n",
        ctx->cfg.model_name,
        rain_rc);

    free(ctx);
    return NULL;
  }

  ctx->propagation.total_loss_db +=
      ctx->propagation.rain_loss_db;

  ctx->propagation.applied_loss_db +=
      ctx->propagation.rain_loss_db;

  const double rain_amplitude_gain =
      pow(
          10.0,
          -ctx->propagation.rain_loss_db / 20.0);

  ctx->propagation.linear_amplitude_gain *=
      rain_amplitude_gain;

    LOG_I(
      HW,
      "HAPS RAIN INSTALLED: "
      "link=%s direction=%s "
      "RAIN=%.6f dB "
      "rain_gain=%e "
      "total_loss=%.6f dB "
      "applied_loss=%.6f dB "
      "total_gain=%e\n",
      ctx->cfg.model_name,
      ctx->is_uplink ? "UL" : "DL",
      ctx->propagation.rain_loss_db,
      rain_amplitude_gain,
      ctx->propagation.total_loss_db,
      ctx->propagation.applied_loss_db,
      ctx->propagation.linear_amplitude_gain);


LOG_I(
    HW,
    "HAPS CHANNEL CREATED: "
    "link=%s direction=%s "
    "distance=%.2f m elevation=%.2f deg "
    "ref_elevation=%.0f deg "
    "LOS=%d pLOS=%.3f draw=%.3f "
    "FSPL=%.2f dB CL=%.2f dB "
    "sigmaSF=%.2f dB SF=%.2f dB "
    "total=%.2f dB calibration=%.2f dB "
    "applied=%.2f dB gain=%e "
    "delay=%.3f us\n",
    ctx->cfg.model_name,
    ctx->is_uplink ? "UL" : "DL",
    ctx->geometry.slant_range_m,
    ctx->geometry.elevation_deg,
    ctx->propagation.reference_elevation_deg,
    ctx->propagation.is_los,
    ctx->propagation.los_probability,
    ctx->propagation.los_uniform_draw,
    ctx->propagation.fspl_db,
    ctx->propagation.clutter_loss_db,
    ctx->propagation.shadow_sigma_db,
    ctx->propagation.shadow_fading_db,
    ctx->propagation.total_loss_db,
    ctx->cfg.link_budget_offset_db,
    ctx->propagation.applied_loss_db,
    ctx->propagation.linear_amplitude_gain,
    ctx->geometry.propagation_delay_s * 1.0e6);

  return ctx;
}

int haps_channel_update(
    haps_channel_ctx_t *ctx,
    uint64_t timestamp,
    int nb_samples)
{
  if (ctx == NULL || nb_samples <= 0)
    return -1;

  /*
   * İlk sürüm statik geometri kullanmaktadır.
   */
  ctx->last_update_timestamp =
      timestamp + (uint64_t)nb_samples;

  return 0;
}

int haps_channel_process(
    haps_channel_ctx_t *ctx,
    c16_t **input,
    int nb_tx,
    cf_t **output,
    int nb_rx,
    int nb_samples,
    uint64_t timestamp)
{
  if (ctx == NULL ||
      input == NULL ||
      output == NULL ||
      nb_samples <= 0) {
    return -1;
  }

  if (nb_tx != 1 || nb_rx != 1)
    return -1;

  if (input[0] == NULL || output[0] == NULL || timestamp > UINT64_MAX - (uint64_t)nb_samples)
    return -1;

  const double gain = ctx->propagation.linear_amplitude_gain;
  const bool apply_doppler = ctx->cfg.enable_doppler && fabs(ctx->doppler_hz) > 1.0e-12;
  double oscillator_real = 1;
  double oscillator_imag = 0;
  double step_real = 1;
  double step_imag = 0;
  if (apply_doppler) {
    /* With TDL, use the sample clock for both Doppler terms, even after gaps. */
    double phase = ctx->doppler_phase_rad;
    if (ctx->cfg.enable_fast_fading) {
      const long double cycles = (long double)timestamp * (ctx->doppler_hz / ctx->cfg.sample_rate_hz);
      phase = HAPS_TWO_PI * (double)fmodl(cycles, 1);
    }
    oscillator_real = cos(phase);
    oscillator_imag = sin(phase);
    step_real = cos(ctx->doppler_phase_increment_rad);
    step_imag = sin(ctx->doppler_phase_increment_rad);
  }

  for (int n = 0; n < nb_samples; ++n) {
    double real;
    double imag;
    if (ctx->cfg.enable_fast_fading) {
      haps_complex_t taps[HAPS_TDL_MAX_PATHS];
      haps_tdl_coefficients(&ctx->tdl, timestamp + (uint64_t)n, taps);
      const size_t index = (size_t)ctx->tdl.history_samples + n;
      real = 0;
      imag = 0;
      for (unsigned tap = 0; tap < ctx->tdl.path_count; ++tap) {
        const c16_t sample = input[0][index - ctx->tdl.paths[tap].delay_samples];
        real += sample.r * taps[tap].r - sample.i * taps[tap].i;
        imag += sample.r * taps[tap].i + sample.i * taps[tap].r;
      }
    } else {
      real = input[0][n].r;
      imag = input[0][n].i;
    }

    if (apply_doppler) {
      const double rotated_real = real * oscillator_real - imag * oscillator_imag;
      imag = real * oscillator_imag + imag * oscillator_real;
      real = rotated_real;
      const double next_real = oscillator_real * step_real - oscillator_imag * step_imag;
      oscillator_imag = oscillator_real * step_imag + oscillator_imag * step_real;
      oscillator_real = next_real;
      if ((n & 1023) == 1023) {
        const double magnitude = hypot(oscillator_real, oscillator_imag);
        oscillator_real /= magnitude;
        oscillator_imag /= magnitude;
      }
    }

    output[0][n].r += (float)(real * gain);
    output[0][n].i += (float)(imag * gain);
  }
  if (apply_doppler)
    ctx->doppler_phase_rad = haps_wrap_phase_rad(atan2(oscillator_imag, oscillator_real));

  haps_channel_update(
      ctx,
      timestamp,
      nb_samples);

  /*
   * Yaklaşık saniyede bir durum logu.
   */
  const uint64_t log_period_samples =
      (uint64_t)ctx->cfg.sample_rate_hz;

  if (log_period_samples > 0 &&
      timestamp - ctx->last_log_timestamp >=
          log_period_samples) {

    LOG_I(
    HW,
    "HAPS ACTIVE: "
    "link=%s "
    "direction=%s "
    "HAPS_lon=%.6f deg "
    "UE_lon=%.6f deg "
    "slant=%.3f km "
    "elevation=%.2f deg "
    "LOS=%d "
    "pLOS=%.3f "
    "draw=%.3f "
    "FSPL=%.2f dB "
    "CL=%.2f dB "
    "sigmaSF=%.2f dB "
    "SF=%.2f dB "
    "BEL=%.2f dB "
    "GAS=%.6f dB "
    "RAIN=%.6f dB CLOUD=%.6f dB SCINT=%.6f dB "
    "total_loss=%.6f dB "
    "applied_loss=%.6f dB "
    "Doppler=%+.2f Hz "
    "phase=%.6f rad\n",
    ctx->cfg.model_name,
    ctx->is_uplink ? "UL" : "DL",
    ctx->cfg.haps_longitude_deg,
    ctx->cfg.ue_longitude_deg,
    ctx->geometry.slant_range_m / 1000.0,
    ctx->geometry.elevation_deg,
    (int)ctx->propagation.is_los,
    ctx->propagation.los_probability,
    ctx->propagation.los_uniform_draw,
    ctx->propagation.fspl_db,
    ctx->propagation.clutter_loss_db,
    ctx->propagation.shadow_sigma_db,
    ctx->propagation.shadow_fading_db,
    ctx->propagation.building_entry_loss_db,
    ctx->propagation.gas_loss_db,
    ctx->propagation.rain_loss_db,
    ctx->propagation.cloud_loss_db,
    ctx->propagation.scintillation_loss_db,
    ctx->propagation.total_loss_db,
    ctx->propagation.applied_loss_db,
    ctx->doppler_hz,
    ctx->doppler_phase_rad);

     if (ctx->delay.enabled) {
  LOG_I(
      HW,
      "HAPS DELAY ACTIVE: "
      "link=%s direction=%s "
      "slant=%.3f km "
      "one_way_delay=%.6f us "
      "exact_samples=%.6f "
      "integer_samples=%llu "
      "fractional_residual=%.6f "
      "applied_delay=%.6f us "
      "quant_error=%+.3f ns\n",
      ctx->cfg.model_name,
      ctx->is_uplink ? "UL" : "DL",
      ctx->geometry.slant_range_m / 1000.0,
      ctx->delay.one_way_delay_s * 1.0e6,
      ctx->delay.exact_samples,
      (unsigned long long)
          ctx->delay.applied_integer_samples,
      ctx->delay.fractional_samples,
      ctx->delay.applied_delay_s * 1.0e6,
      ctx->delay.quantization_error_s * 1.0e9);
}
      
    if (ctx->cfg.enable_rain) {

      LOG_I(
          HW,
          "HAPS RAIN ACTIVE: "
          "link=%s direction=%s "
          "R=%.3f mm/h "
          "elevation=%.2f deg "
          "rain_height=%.3f km "
          "k=%.12f "
          "alpha=%.12f "
          "gamma_R=%.9f dB/km "
          "rain_path=%.6f km "
          "RAIN=%.6f dB\n",
          ctx->cfg.model_name,
          ctx->is_uplink ? "UL" : "DL",
          ctx->cfg.rain_rate_mm_per_h,
          ctx->geometry.elevation_deg,
          ctx->cfg.rain_height_km,
          ctx->propagation.rain_k,
          ctx->propagation.rain_alpha,
          ctx->propagation
              .rain_specific_attenuation_db_per_km,
          ctx->propagation.rain_path_km,
          ctx->propagation.rain_loss_db);
    } 
 

    ctx->last_log_timestamp = timestamp;
  }

  return 0;
}

unsigned haps_channel_get_history_samples(const haps_channel_ctx_t *ctx)
{
  return ctx != NULL ? ctx->tdl.history_samples : 0;
}

int haps_channel_get_delay_info(
    const haps_channel_ctx_t *ctx,
    haps_delay_info_t *info)
{
  if (ctx == NULL ||
      info == NULL) {
    return -1;
  }

  *info = ctx->delay;

  return 0;
}


int haps_channel_get_snapshot(
    const haps_channel_ctx_t *ctx,
    haps_geometry_result_t *geometry,
    haps_propagation_result_t *propagation)
{
  if (ctx == NULL || geometry == NULL || propagation == NULL)
    return -1;
  *geometry = ctx->geometry;
  *propagation = ctx->propagation;
  return 0;
}

void haps_channel_destroy(
    haps_channel_ctx_t *ctx)
{
  if (ctx == NULL)
    return;

  LOG_I(
      HW,
      "Destroying HAPS channel %s\n",
      ctx->cfg.model_name);

  free(ctx);
}
