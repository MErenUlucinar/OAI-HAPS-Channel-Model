#ifndef HAPS_CONFIG_H
#define HAPS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  HAPS_SCENARIO_DENSE_URBAN = 0,
  HAPS_SCENARIO_URBAN,
  HAPS_SCENARIO_SUBURBAN,
  HAPS_SCENARIO_RURAL
} haps_scenario_t;

typedef enum {
  HAPS_LOS_FORCED = 0,
  HAPS_NLOS_FORCED,
  HAPS_LOS_PROBABILISTIC
} haps_los_mode_t;

typedef enum {
  HAPS_TERMINAL_OUTDOOR = 0,
  HAPS_TERMINAL_INDOOR_TRADITIONAL,
  HAPS_TERMINAL_INDOOR_THERMALLY_EFFICIENT
} haps_terminal_type_t;

typedef enum {
  HAPS_NTN_TDL_INVALID = -1,
  HAPS_NTN_TDL_A = 0,
  HAPS_NTN_TDL_B,
  HAPS_NTN_TDL_C,
  HAPS_NTN_TDL_D
} haps_tdl_profile_t;

typedef struct {
  char model_name[64];

  /*
   * Temel RF parametreleri.
   * Bu modülde birimler açıkça Hz olarak tutulmaktadır.
   */
  double carrier_frequency_hz;
  double sample_rate_hz;

  /*
   * WGS-84 koordinatları.
   */
  double haps_latitude_deg;
  double haps_longitude_deg;
  double haps_altitude_m;

  double ue_latitude_deg;
  double ue_longitude_deg;
  double ue_altitude_m;


  
  int rain_mode;

  double rain_rate_mm_per_h;
  double rain_height_km;
  double rain_polarization_tilt_deg;

  double gas_surface_pressure_hpa;
  double gas_surface_temperature_k;
  double gas_surface_water_vapour_density_g_m3;
  double gas_integrated_water_vapour_kg_m2;

  double cloud_liquid_water_kg_m2;
  double scintillation_nwet;
  double scintillation_antenna_diameter_m;
  double scintillation_antenna_efficiency;
  double scintillation_exceedance_percent;

  haps_scenario_t scenario;
  haps_los_mode_t los_mode;
  haps_terminal_type_t terminal_type;
  double bel_percentile;
  haps_tdl_profile_t tdl_profile;

  double delay_spread_s;
  double k_factor_db; /* NAN selects the tabulated profile powers. */
  double tdl_max_doppler_hz;

  /*
   * OAI'nin normalize IQ seviyesi ile fiziksel link bütçesi
   * arasındaki kalibrasyon için kullanılır.
   *
   * applied_loss = total_loss - link_budget_offset_db
   */
  double link_budget_offset_db;

  double fixed_doppler_hz;
  bool enable_fspl;
  bool enable_clutter;
  bool enable_shadow_fading;
  bool enable_building_entry_loss;

  bool enable_gas;
  bool enable_rain;
  bool enable_cloud;
  bool enable_scintillation;

  bool enable_fast_fading;
  bool enable_propagation_delay;
  bool enable_fractional_delay;
  bool enable_doppler;
  bool enable_noise;

  uint32_t random_seed;
  double update_period_s;
} haps_config_t;

void haps_config_set_defaults(haps_config_t *cfg);

int haps_load_config(
    const char *model_name,
    haps_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
