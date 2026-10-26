#include "haps_propagation.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "haps_gas.h"
#include "common/utils/LOG/log.h"

#define HAPS_PI 3.14159265358979323846
#define SPEED_OF_LIGHT_MPS 299792458.0
#define HAPS_NUM_REFERENCE_ANGLES 9

/*
 * 3GPP TR 38.811:
 * Reference elevation angles are 10, 20, ..., 90 degrees.
 */
static const double reference_elevation_deg[
    HAPS_NUM_REFERENCE_ANGLES] = {
    10.0,
    20.0,
    30.0,
    40.0,
    50.0,
    60.0,
    70.0,
    80.0,
    90.0
};

typedef struct {
  /*
   * LOS probability values in the range [0,1].
   */
  double los_probability[HAPS_NUM_REFERENCE_ANGLES];

  /*
   * Shadow-fading standard deviation in dB.
   */
  double sigma_los_db[HAPS_NUM_REFERENCE_ANGLES];
  double sigma_nlos_db[HAPS_NUM_REFERENCE_ANGLES];

  /*
   * NLOS clutter loss in dB.
   * LOS clutter loss is always zero.
   */
  double clutter_nlos_db[HAPS_NUM_REFERENCE_ANGLES];
} haps_large_scale_table_t;

/*
 * Dense urban, S-band.
 *
 * 3GPP TR 38.811:
 * Tables 6.6.1-1 and 6.6.2-1.
 */
static const haps_large_scale_table_t dense_urban_sband = {
    .los_probability = {
        0.282,
        0.331,
        0.398,
        0.468,
        0.537,
        0.612,
        0.738,
        0.820,
        0.981
    },

    .sigma_los_db = {
        3.5,
        3.4,
        2.9,
        3.0,
        3.1,
        2.7,
        2.5,
        2.3,
        1.2
    },

    .sigma_nlos_db = {
        15.5,
        13.9,
        12.4,
        11.7,
        10.6,
        10.5,
        10.1,
        9.2,
        9.2
    },

    .clutter_nlos_db = {
        34.3,
        30.9,
        29.0,
        27.7,
        26.8,
        26.2,
        25.8,
        25.5,
        25.5
    }
};

/*
 * Urban, S-band.
 *
 * 3GPP TR 38.811:
 * Tables 6.6.1-1 and 6.6.2-2.
 */
static const haps_large_scale_table_t urban_sband = {
    .los_probability = {
        0.246,
        0.386,
        0.493,
        0.613,
        0.726,
        0.805,
        0.919,
        0.968,
        0.992
    },

    .sigma_los_db = {
        4.0,
        4.0,
        4.0,
        4.0,
        4.0,
        4.0,
        4.0,
        4.0,
        4.0
    },

    .sigma_nlos_db = {
        6.0,
        6.0,
        6.0,
        6.0,
        6.0,
        6.0,
        6.0,
        6.0,
        6.0
    },

    .clutter_nlos_db = {
        34.3,
        30.9,
        29.0,
        27.7,
        26.8,
        26.2,
        25.8,
        25.5,
        25.5
    }
};

/*
 * Suburban and rural, S-band.
 *
 * 3GPP TR 38.811:
 * Tables 6.6.1-1 and 6.6.2-3.
 */
static const haps_large_scale_table_t suburban_rural_sband = {
    .los_probability = {
        0.782,
        0.869,
        0.919,
        0.929,
        0.935,
        0.940,
        0.949,
        0.952,
        0.998
    },

    .sigma_los_db = {
        1.79,
        1.14,
        1.14,
        0.92,
        1.42,
        1.56,
        0.85,
        0.72,
        0.72
    },

    .sigma_nlos_db = {
        8.93,
        9.08,
        8.78,
        10.25,
        10.56,
        10.74,
        10.17,
        11.52,
        11.52
    },

    .clutter_nlos_db = {
        19.52,
        18.17,
        18.42,
        18.28,
        18.63,
        17.68,
        16.50,
        16.30,
        16.30
    }
};

/*
 * Global rand()/srand() kullanmak yerine kanal context'inden
 * bağımsız, tekrar üretilebilir basit bir PRNG kullanıyoruz.
 */
static uint32_t haps_xorshift32(uint32_t *state)
{
  uint32_t x = *state;

  if (x == 0)
    x = 0x6d2b79f5U;

  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;

  *state = x;
  return x;
}

/*
 * Review fix (E9): SplitMix32-style seed finaliser (Murmur3 fmix32 constants).
 * Any nonzero 32-bit state is a valid xorshift32 state; mixing makes the
 * first outputs independent of the magnitude of the user seed.
 */
static uint32_t haps_seed_mix(uint32_t seed)
{
  uint32_t z = seed + 0x9E3779B9U;
  z = (z ^ (z >> 16)) * 0x85EBCA6BU;
  z = (z ^ (z >> 13)) * 0xC2B2AE35U;
  z ^= z >> 16;
  return z != 0 ? z : 0x6d2b79f5U;
}

/*
 * Açık aralıkta uniform rastgele değişken:
 *
 * 0 < u < 1
 */
static double haps_uniform_open01(uint32_t *state)
{
  const uint32_t value = haps_xorshift32(state);

  return ((double)value + 0.5) / 4294967296.0;
}

/*
 * Box-Muller dönüşümüyle standart normal değişken:
 *
 * z ~ N(0,1)
 */
static double haps_standard_normal(uint32_t *state)
{
  const double u1 = haps_uniform_open01(state);
  const double u2 = haps_uniform_open01(state);

  return sqrt(-2.0 * log(u1)) *
         cos(2.0 * HAPS_PI * u2);
}

static const haps_large_scale_table_t *
get_large_scale_table(haps_scenario_t scenario)
{
  switch (scenario) {
    case HAPS_SCENARIO_DENSE_URBAN:
      return &dense_urban_sband;

    case HAPS_SCENARIO_URBAN:
      return &urban_sband;

    case HAPS_SCENARIO_SUBURBAN:
    case HAPS_SCENARIO_RURAL:
    default:
      return &suburban_rural_sband;
  }
}

/*
 * 3GPP TR 38.811, ölçülen elevation değerine en yakın
 * 10 derecelik referans açının kullanılmasını ister.
 *
 * Örnek:
 *   44.87 derece -> 40 derece
 *   46.00 derece -> 50 derece
 */
static int nearest_reference_angle_index(
    double elevation_deg)
{
  if (elevation_deg <= 10.0)
    return 0;

  if (elevation_deg >= 90.0)
    return HAPS_NUM_REFERENCE_ANGLES - 1;

  int index =
      (int)floor((elevation_deg + 5.0) / 10.0) - 1;

  if (index < 0)
    index = 0;

  if (index >= HAPS_NUM_REFERENCE_ANGLES)
    index = HAPS_NUM_REFERENCE_ANGLES - 1;

  return index;
}

typedef struct {
  double r;
  double s;
  double t;
  double u;
  double v;
  double w;
  double x;
  double y;
  double z;
} haps_bel_coefficients_t;

/*
 * ITU-R P.2109-2, Table 1.
 */
static const haps_bel_coefficients_t
    bel_traditional_coefficients = {
        .r = 12.64,
        .s = 3.72,
        .t = 0.96,
        .u = 9.6,
        .v = 2.0,
        .w = 9.1,
        .x = -3.0,
        .y = 4.5,
        .z = -2.0
};

static const haps_bel_coefficients_t
    bel_thermally_efficient_coefficients = {
        .r = 28.19,
        .s = -3.00,
        .t = 8.48,
        .u = 13.5,
        .v = 3.8,
        .w = 27.8,
        .x = -2.9,
        .y = 9.4,
        .z = -2.1
};

/*
 * Standart normal dağılımın ters CDF yaklaşımı.
 *
 * Girdi:
 *   0 < probability < 1
 */
static double haps_inverse_normal_cdf(
    double probability)
{
  static const double a[] = {
      -3.969683028665376e+01,
       2.209460984245205e+02,
      -2.759285104469687e+02,
       1.383577518672690e+02,
      -3.066479806614716e+01,
       2.506628277459239e+00
  };

  static const double b[] = {
      -5.447609879822406e+01,
       1.615858368580409e+02,
      -1.556989798598866e+02,
       6.680131188771972e+01,
      -1.328068155288572e+01
  };

  static const double c[] = {
      -7.784894002430293e-03,
      -3.223964580411365e-01,
      -2.400758277161838e+00,
      -2.549732539343734e+00,
       4.374664141464968e+00,
       2.938163982698783e+00
  };

  static const double d[] = {
       7.784695709041462e-03,
       3.224671290700398e-01,
       2.445134137142996e+00,
       3.754408661907416e+00
  };

  const double lower_limit = 0.02425;
  const double upper_limit = 1.0 - lower_limit;

  if (probability <= 0.0 ||
      probability >= 1.0) {
    return NAN;
  }

  if (probability < lower_limit) {
    const double q =
        sqrt(-2.0 * log(probability));

    return
        (((((c[0] * q + c[1]) * q + c[2]) * q +
            c[3]) * q + c[4]) * q + c[5]) /
        ((((d[0] * q + d[1]) * q + d[2]) * q +
           d[3]) * q + 1.0);
  }

  if (probability > upper_limit) {
    const double q =
        sqrt(-2.0 * log(1.0 - probability));

    return -
        (((((c[0] * q + c[1]) * q + c[2]) * q +
            c[3]) * q + c[4]) * q + c[5]) /
        ((((d[0] * q + d[1]) * q + d[2]) * q +
           d[3]) * q + 1.0);
  }

  const double q = probability - 0.5;
  const double r = q * q;

  return
      (((((a[0] * r + a[1]) * r + a[2]) * r +
          a[3]) * r + a[4]) * r + a[5]) * q /
      (((((b[0] * r + b[1]) * r + b[2]) * r +
          b[3]) * r + b[4]) * r + 1.0);
}

static int haps_compute_building_entry_loss_db(
    const haps_config_t *cfg,
    double elevation_deg,
    double *building_entry_loss_db)
{
  if (cfg == NULL ||
      building_entry_loss_db == NULL) {
    return -1;
  }

  *building_entry_loss_db = 0.0;

  /*
   * Outdoor terminal için bina giriş kaybı yoktur.
   */
  if (cfg->terminal_type ==
      HAPS_TERMINAL_OUTDOOR) {
    return 0;
  }

  const double probability =
      cfg->bel_percentile;

  if (probability <= 0.0 ||
      probability >= 1.0) {

    LOG_E(
        HW,
        "HAPS BEL: bel_percentile must satisfy "
        "0 < P < 1, received %.6f\n",
        probability);

    return -2;
  }

  if (probability < 0.01 ||
      probability > 0.99) {

    LOG_W(
        HW,
        "HAPS BEL: P=%.4f is outside the "
        "empirically validated 0.01-0.99 range\n",
        probability);
  }

  const double frequency_ghz =
      cfg->carrier_frequency_hz / 1.0e9;

  if (frequency_ghz <= 0.0)
    return -3;

  const haps_bel_coefficients_t *coefficients =
      cfg->terminal_type ==
              HAPS_TERMINAL_INDOOR_THERMALLY_EFFICIENT
          ? &bel_thermally_efficient_coefficients
          : &bel_traditional_coefficients;

  const double log_frequency =
      log10(frequency_ghz);

  /*
   * P.2109 equations 5-10.
   */
  const double horizontal_median_loss =
      coefficients->r +
      coefficients->s * log_frequency +
      coefficients->t *
          log_frequency * log_frequency;

  const double elevation_correction =
      0.212 * fabs(elevation_deg);

  const double mu_1 =
      horizontal_median_loss +
      elevation_correction;

  const double mu_2 =
      coefficients->w +
      coefficients->x * log_frequency;

  const double sigma_1 =
      coefficients->u +
      coefficients->v * log_frequency;

  const double sigma_2 =
      coefficients->y +
      coefficients->z * log_frequency;

  const double normal_quantile =
      haps_inverse_normal_cdf(probability);

  if (!isfinite(normal_quantile))
    return -4;

  const double component_a =
      normal_quantile * sigma_1 + mu_1;

  const double component_b =
      normal_quantile * sigma_2 + mu_2;

  const double component_c = -3.0;

  *building_entry_loss_db =
      10.0 *
      log10(
          pow(10.0, component_a / 10.0) +
          pow(10.0, component_b / 10.0) +
          pow(10.0, component_c / 10.0));

  LOG_I(
      HW,
      "HAPS BEL: type=%d P=%.3f "
      "frequency=%.4f GHz elevation=%.2f deg "
      "quantile=%.4f BEL=%.2f dB\n",
      (int)cfg->terminal_type,
      probability,
      frequency_ghz,
      elevation_deg,
      normal_quantile,
      *building_entry_loss_db);

  return 0;
}

double haps_fspl_db(
    double distance_m,
    double frequency_hz)
{
  if (distance_m <= 0.0 || frequency_hz <= 0.0)
    return 0.0;

  return 20.0 *
         log10(
             4.0 *
             HAPS_PI *
             distance_m *
             frequency_hz /
             SPEED_OF_LIGHT_MPS);
}

int haps_compute_propagation(
    const haps_config_t *cfg,
    const haps_geometry_result_t *geometry,
    haps_propagation_result_t *result)
{
  if (cfg == NULL || geometry == NULL || result == NULL)
    return -1;

  memset(result, 0, sizeof(*result));

  /*
   * Bu aşamada henüz uygulanmamış bileşenler.
   */
  if ( cfg->enable_cloud ||
      cfg->enable_scintillation) {

    LOG_E(
        HW,
        "HAPS propagation: cloud and scintillation "
        "are not implemented yet\n");

    return -2;
  }

  const haps_large_scale_table_t *table =
      get_large_scale_table(cfg->scenario);

  const int angle_index =
      nearest_reference_angle_index(
          geometry->elevation_deg);

  result->reference_elevation_deg =
      reference_elevation_deg[angle_index];

  result->los_probability =
      table->los_probability[angle_index];

  /*
   * Forced modlarda rastgele LOS çekilişi yapılmaz.
   */
  result->los_uniform_draw = -1.0;

  /*
   * Seed aynı olduğunda sonuç yeniden üretilebilir.
   */
  /*
   * Review fix (E9): xorshift32 must not be seeded with the raw user seed.
   * For seeds below 2^13 the first output is ~ seed * 270369, i.e. the LoS
   * draw is ~ seed * 6.3e-5 and every link is LoS. Mix the seed with a
   * SplitMix32-style finaliser and discard the first outputs.
   */
  uint32_t random_state = haps_seed_mix(cfg->random_seed != 0 ? cfg->random_seed : 1U);
  for (int warm = 0; warm < 8; warm++)
    (void)haps_xorshift32(&random_state);

  switch (cfg->los_mode) {
    case HAPS_LOS_FORCED:
      result->is_los = true;
      break;

    case HAPS_NLOS_FORCED:
      result->is_los = false;
      break;

    case HAPS_LOS_PROBABILISTIC:
      result->los_uniform_draw =
          haps_uniform_open01(&random_state);

      result->is_los =
          result->los_uniform_draw <
          result->los_probability;
      break;

    default:
      result->is_los = true;
      break;
  }

  /*
   * Gerçek frekans ve slant mesafe üzerinden FSPL.
   */
  if (cfg->enable_fspl) {
    result->fspl_db =
        haps_fspl_db(
            geometry->slant_range_m,
            cfg->carrier_frequency_hz);
  }

  /*
   * TR 38.811'e göre LOS durumunda clutter loss 0 dB.
   */
  if (cfg->enable_clutter && !result->is_los) {
    result->clutter_loss_db =
        table->clutter_nlos_db[angle_index];
  }

  /*
   * LOS/NLOS durumuna göre shadow-fading sigma seçimi.
   */
  if (result->is_los) {
    result->shadow_sigma_db =
        table->sigma_los_db[angle_index];
  } else {
    result->shadow_sigma_db =
        table->sigma_nlos_db[angle_index];
  }

  /*
   * Shadow fading:
   *
   * SF ~ N(0, sigma_SF^2)
   *
   * Bu ilk sürümde kanal oluşturulurken bir kez çekilir
   * ve bağlantı boyunca sabit kalır.
   */
  if (cfg->enable_shadow_fading) {
    const double standard_normal =
        haps_standard_normal(&random_state);

    result->shadow_fading_db =
        result->shadow_sigma_db *
        standard_normal;
  }

  if (cfg->enable_gas) {
  haps_gas_result_t gas_result;

  const int gas_rc =
      haps_gas_compute_p676_13(
          cfg->carrier_frequency_hz,
          geometry->elevation_deg,
          cfg->gas_surface_pressure_hpa,
          cfg->gas_surface_temperature_k,
          cfg->
              gas_surface_water_vapour_density_g_m3,
          cfg->
              gas_integrated_water_vapour_kg_m2,
          &gas_result);

  if (gas_rc != 0) {
    LOG_E(
        HW,
        "HAPS GAS: P.676-13 calculation "
        "failed, rc=%d, f=%.6f GHz, "
        "elevation=%.2f deg\n",
        gas_rc,
        cfg->carrier_frequency_hz /
            1.0e9,
        geometry->elevation_deg);

    return -4;
  }

  result->oxygen_gas_loss_db =
      gas_result.oxygen_loss_db;

  result->water_vapour_gas_loss_db =
      gas_result.water_loss_db;

  result->gas_loss_db =
      gas_result.total_loss_db;

  LOG_I(
      HW,
      "HAPS GAS P676-13: "
      "f=%.6f GHz elevation=%.2f deg "
      "P=%.2f hPa T=%.2f K "
      "rho=%.3f g/m3 V=%.3f kg/m2 "
      "gammaO2=%.8f dB/km "
      "hO2=%.6f km "
      "O2=%.6f dB "
      "H2O=%.6f dB "
      "GAS=%.6f dB\n",
      cfg->carrier_frequency_hz /
          1.0e9,
      geometry->elevation_deg,
      cfg->gas_surface_pressure_hpa,
      cfg->gas_surface_temperature_k,
      cfg->
          gas_surface_water_vapour_density_g_m3,
      cfg->
          gas_integrated_water_vapour_kg_m2,
      gas_result.
          oxygen_specific_db_per_km,
      gas_result.
          oxygen_equivalent_height_km,
      gas_result.oxygen_loss_db,
      gas_result.water_loss_db,
      gas_result.total_loss_db);
}

  

  if (cfg->enable_building_entry_loss) {
  const int bel_result =
      haps_compute_building_entry_loss_db(
          cfg,
          geometry->elevation_deg,
          &result->building_entry_loss_db);

  if (bel_result != 0) {
    LOG_E(
        HW,
        "HAPS building-entry-loss "
        "calculation failed: %d\n",
        bel_result);

    return -3;
  }
}

    result->total_loss_db =
    result->fspl_db +
    result->clutter_loss_db +
    result->shadow_fading_db +
    result->building_entry_loss_db +
    result->gas_loss_db +
    result->rain_loss_db +
    result->cloud_loss_db +
    result->scintillation_loss_db;

/*
 * Normalized RFsimulator IQ kalibrasyonu.
 */
result->applied_loss_db =
    result->total_loss_db -
    cfg->link_budget_offset_db;

if (result->applied_loss_db < 0.0)
  result->applied_loss_db = 0.0;

/*
 * Güç kaybını kompleks IQ genlik katsayısına çevir.
 */
result->linear_amplitude_gain =
    pow(
        10.0,
        -result->applied_loss_db / 20.0);

LOG_I(
    HW,
    "HAPS LARGE SCALE: "
    "FSPL=%.6f dB "
    "CL=%.6f dB "
    "SF=%.6f dB "
    "BEL=%.6f dB "
    "GAS=%.6f dB "
    "total=%.6f dB "
    "offset=%.6f dB "
    "applied=%.6f dB "
    "gain=%.9f\n",
    result->fspl_db,
    result->clutter_loss_db,
    result->shadow_fading_db,
    result->building_entry_loss_db,
    result->gas_loss_db,
    result->total_loss_db,
    cfg->link_budget_offset_db,
    result->applied_loss_db,
    result->linear_amplitude_gain);

return 0;
}    

      
