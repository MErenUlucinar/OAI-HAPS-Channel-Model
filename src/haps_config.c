#include "haps_config.h"
#include "haps_rain.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"

#define HAPS_CONFIG_LIST_NAME "hapsmod"

static haps_scenario_t parse_scenario(const char *value)
{
  if (value == NULL)
    return HAPS_SCENARIO_RURAL;

  if (strcasecmp(value, "dense_urban") == 0)
    return HAPS_SCENARIO_DENSE_URBAN;

  if (strcasecmp(value, "urban") == 0)
    return HAPS_SCENARIO_URBAN;

  if (strcasecmp(value, "suburban") == 0)
    return HAPS_SCENARIO_SUBURBAN;

  return HAPS_SCENARIO_RURAL;
}

static haps_los_mode_t parse_los_mode(const char *value)
{
  if (value == NULL)
    return HAPS_LOS_FORCED;

  if (strcasecmp(value, "forced_nlos") == 0)
    return HAPS_NLOS_FORCED;

  if (strcasecmp(value, "probabilistic") == 0)
    return HAPS_LOS_PROBABILISTIC;

  return HAPS_LOS_FORCED;
}

static haps_terminal_type_t parse_terminal_type(const char *value)
{
  if (value == NULL)
    return HAPS_TERMINAL_OUTDOOR;

  if (strcasecmp(value, "indoor_traditional") == 0)
    return HAPS_TERMINAL_INDOOR_TRADITIONAL;

  if (strcasecmp(value, "indoor_thermally_efficient") == 0)
    return HAPS_TERMINAL_INDOOR_THERMALLY_EFFICIENT;

  return HAPS_TERMINAL_OUTDOOR;
}

static haps_tdl_profile_t parse_tdl_profile(const char *value)
{
  if (value == NULL)
    return HAPS_NTN_TDL_D;

  if (strcasecmp(value, "NTN-TDL-A") == 0)
    return HAPS_NTN_TDL_A;

  if (strcasecmp(value, "NTN-TDL-B") == 0)
    return HAPS_NTN_TDL_B;

  if (strcasecmp(value, "NTN-TDL-C") == 0)
    return HAPS_NTN_TDL_C;

  if (strcasecmp(value, "NTN-TDL-D") == 0)
    return HAPS_NTN_TDL_D;

  return HAPS_NTN_TDL_INVALID;
}

void haps_config_set_defaults(haps_config_t *cfg)
{
  if (cfg == NULL)
    return;

  memset(cfg, 0, sizeof(*cfg));

  cfg->carrier_frequency_hz = 3619200000.0;
  cfg->sample_rate_hz = 61440000.0;

  cfg->haps_latitude_deg = 39.9334;
  cfg->haps_longitude_deg = 32.8597;
  cfg->haps_altitude_m = 20000.0;

  cfg->ue_latitude_deg = 39.9334;
  cfg->ue_longitude_deg = 32.8597;
  cfg->ue_altitude_m = 0.0;

  cfg->scenario = HAPS_SCENARIO_RURAL;
  cfg->los_mode = HAPS_LOS_FORCED;
  cfg->terminal_type = HAPS_TERMINAL_OUTDOOR;
  cfg->bel_percentile = 0.50;
  cfg->tdl_profile = HAPS_NTN_TDL_D;

  cfg->delay_spread_s = 100.0e-9;
  cfg->k_factor_db = NAN;
  cfg->tdl_max_doppler_hz = 30.0;

  cfg->cloud_liquid_water_kg_m2 = 0.0;
  cfg->scintillation_nwet = 50.0;
  cfg->scintillation_antenna_diameter_m = 0.3;
  cfg->scintillation_antenna_efficiency = 0.5;
  cfg->scintillation_exceedance_percent = 1.0;

  /*
   * İlk test identity kanal olsun.
   */
  cfg->link_budget_offset_db = 0.0;
  cfg->fixed_doppler_hz = 0.0;

  cfg->gas_surface_pressure_hpa = 1013.25;
  cfg->gas_surface_temperature_k = 288.15;
  cfg->gas_surface_water_vapour_density_g_m3 = 7.5;
  cfg->gas_integrated_water_vapour_kg_m2 = 15.0;
  
  
  cfg->enable_rain = false;
  cfg->rain_mode =
      HAPS_RAIN_DISABLED;

  cfg->rain_rate_mm_per_h = 0.0;
  cfg->rain_height_km = 5.0;
  cfg->rain_polarization_tilt_deg = 45.0;


  cfg->enable_fspl = false;
  cfg->enable_clutter = false;
  cfg->enable_shadow_fading = false;
  cfg->enable_building_entry_loss = false;

  cfg->enable_gas = false;
  cfg->enable_rain = false;
  cfg->enable_cloud = false;
  cfg->enable_scintillation = false;

  cfg->enable_fast_fading = false;
  cfg->enable_propagation_delay = false;
  cfg->enable_fractional_delay = false;
  cfg->enable_doppler = false;
  cfg->enable_noise = false;

  cfg->random_seed = 12345;
  cfg->update_period_s = 0.001;
}

int haps_load_config(
    const char *requested_model_name,
    haps_config_t *cfg)
{
  if (requested_model_name == NULL || cfg == NULL)
    return -1;

  haps_config_set_defaults(cfg);

  paramdef_t params[] = {
      {"model_name", "RFsimulator channel model name", 0,
       .strptr = NULL, .defstrval = "", TYPE_STRING, 0},

      {"carrier_frequency_hz", "Carrier frequency in Hz", 0,
       .dblptr = NULL, .defdblval = 3619200000.0, TYPE_DOUBLE, 0},

      {"sample_rate_hz", "Sample rate in samples per second", 0,
       .dblptr = NULL, .defdblval = 61440000.0, TYPE_DOUBLE, 0},

      {"haps_latitude_deg", "HAPS latitude", 0,
       .dblptr = NULL, .defdblval = 39.9334, TYPE_DOUBLE, 0},

      {"haps_longitude_deg", "HAPS longitude", 0,
       .dblptr = NULL, .defdblval = 32.8597, TYPE_DOUBLE, 0},

      {"haps_altitude_m", "HAPS altitude", 0,
       .dblptr = NULL, .defdblval = 20000.0, TYPE_DOUBLE, 0},

      {"ue_latitude_deg", "UE latitude", 0,
       .dblptr = NULL, .defdblval = 39.9334, TYPE_DOUBLE, 0},

      {"ue_longitude_deg", "UE longitude", 0,
       .dblptr = NULL, .defdblval = 32.8597, TYPE_DOUBLE, 0},

      {"ue_altitude_m", "UE altitude", 0,
       .dblptr = NULL, .defdblval = 0.0, TYPE_DOUBLE, 0},

      {"scenario", "3GPP scenario", 0,
       .strptr = NULL, .defstrval = "rural", TYPE_STRING, 0},

      {"los_mode", "LOS mode", 0,
       .strptr = NULL, .defstrval = "forced_los", TYPE_STRING, 0},

      {"terminal_type", "Terminal type", 0,
       .strptr = NULL, .defstrval = "outdoor", TYPE_STRING, 0},

      {"bel_percentile",
      "ITU-R P.2109 loss not-exceeded probability",
       0,
       .dblptr = NULL,
       .defdblval = 0.50,
        TYPE_DOUBLE,
       0},

      {"tdl_profile", "NTN TDL profile", 0,
       .strptr = NULL, .defstrval = "NTN-TDL-D", TYPE_STRING, 0},

      {"delay_spread_s", "Delay spread in seconds", 0,
       .dblptr = NULL, .defdblval = 100.0e-9, TYPE_DOUBLE, 0},

      {"k_factor_db", "Specular to total diffuse power ratio in dB; omit for table values", 0,
       .dblptr = NULL, .defdblval = NAN, TYPE_DOUBLE, 0},

      {"tdl_max_doppler_hz", "Maximum local scattering Doppler in Hz", 0,
       .dblptr = NULL, .defdblval = 30.0, TYPE_DOUBLE, 0},

      {"cloud_liquid_water_kg_m2", "Vertical cloud liquid water column in kg/m2", 0,
       .dblptr = NULL, .defdblval = 0.0, TYPE_DOUBLE, 0},
      {"scintillation_nwet", "Wet surface refractivity in N-units", 0,
       .dblptr = NULL, .defdblval = 50.0, TYPE_DOUBLE, 0},
      {"scintillation_antenna_diameter_m", "Ground antenna diameter in m", 0,
       .dblptr = NULL, .defdblval = 0.3, TYPE_DOUBLE, 0},
      {"scintillation_antenna_efficiency", "Ground antenna aperture efficiency", 0,
       .dblptr = NULL, .defdblval = 0.5, TYPE_DOUBLE, 0},
      {"scintillation_exceedance_percent", "Scintillation exceedance time percentage", 0,
       .dblptr = NULL, .defdblval = 1.0, TYPE_DOUBLE, 0},

      {"link_budget_offset_db", "RFsim IQ calibration offset", 0,
       .dblptr = NULL, .defdblval = 0.0, TYPE_DOUBLE, 0},
      
      {"fixed_doppler_hz",
       "Fixed Doppler frequency shift in Hz",
        0,
        .dblptr = NULL,
        .defdblval = 0.0,
        TYPE_DOUBLE,
       0},

     {"rain_mode",
       "Rain model: 0 disabled, 1 fixed layer, 2 ITU statistical",
       0,
       .iptr = NULL,
       .defintval = HAPS_RAIN_DISABLED,
       TYPE_INT,
       0},

      {"rain_rate_mm_per_h",
       "Rain rate in millimetres per hour",
       0,
       .dblptr = NULL,
       .defdblval = 0.0,
       TYPE_DOUBLE,
       0},

      {"rain_height_km",
       "Upper height of rain layer in kilometres",
       0,
       .dblptr = NULL,
       .defdblval = 5.0,
       TYPE_DOUBLE,
       0},

      {"rain_polarization_tilt_deg",
       "Polarization tilt angle for ITU-R P.838-3",
       0,
       .dblptr = NULL,
       .defdblval = 45.0,
       TYPE_DOUBLE,
       0},

      {"gas_surface_pressure_hpa",
       "ITU-R P.676 surface pressure in hPa",
       0,
       .dblptr = NULL,
       .defdblval = 1013.25,
       TYPE_DOUBLE,
       0},

{"gas_surface_temperature_k",
 "ITU-R P.676 surface temperature in K",
 0,
 .dblptr = NULL,
 .defdblval = 288.15,
 TYPE_DOUBLE,
 0},

{"gas_surface_water_vapour_density_g_m3",
 "ITU-R P.676 surface water vapour density in g/m3",
 0,
 .dblptr = NULL,
 .defdblval = 7.5,
 TYPE_DOUBLE,
 0},

{"gas_integrated_water_vapour_kg_m2",
 "ITU-R P.676 integrated water vapour in kg/m2",
 0,
 .dblptr = NULL,
 .defdblval = 15.0,
 TYPE_DOUBLE,
 0},

      {"enable_fspl", "Enable free-space path loss", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_clutter", "Enable clutter loss", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_shadow_fading", "Enable shadow fading", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_building_entry_loss", "Enable building entry loss", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_gas", "Enable gaseous attenuation", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_rain", "Enable rain attenuation", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_cloud", "Enable cloud attenuation", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_scintillation", "Enable scintillation", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_fast_fading", "Enable NTN TDL fading", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_propagation_delay", "Enable absolute delay", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_fractional_delay", "Enable fractional delay", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_doppler", "Enable Doppler", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"enable_noise", "Enable HAPS internal noise", 0,
       .iptr = NULL, .defintval = 0, TYPE_INT, 0},

      {"random_seed", "Random seed", 0,
       .uptr = NULL, .defintval = 12345, TYPE_UINT, 0},

      {"update_period_s", "Channel update period", 0,
       .dblptr = NULL, .defdblval = 0.001, TYPE_DOUBLE, 0},
  };

  const int num_params = sizeof(params) / sizeof(params[0]);

  paramlist_def_t haps_list;
  memset(&haps_list, 0, sizeof(haps_list));

  snprintf(
      haps_list.listname,
      sizeof(haps_list.listname),
      "%s",
      HAPS_CONFIG_LIST_NAME);

  /*
   * hapsmod top-level bir config listesidir.
   */
  config_getlist(
      config_get_if(),
      &haps_list,
      params,
      num_params,
      NULL);

  if (haps_list.numelt <= 0) {
    LOG_E(HW, "HAPS config list '%s' not found\n",
          HAPS_CONFIG_LIST_NAME);
    return -1;
  }

#define PARAM_INDEX(name) \
  config_paramidx_fromname(params, num_params, name)

  const int idx_model = PARAM_INDEX("model_name");
  const int idx_fc = PARAM_INDEX("carrier_frequency_hz");
  const int idx_fs = PARAM_INDEX("sample_rate_hz");

  const int idx_haps_lat = PARAM_INDEX("haps_latitude_deg");
  const int idx_haps_lon = PARAM_INDEX("haps_longitude_deg");
  const int idx_haps_alt = PARAM_INDEX("haps_altitude_m");

  const int idx_ue_lat = PARAM_INDEX("ue_latitude_deg");
  const int idx_ue_lon = PARAM_INDEX("ue_longitude_deg");
  const int idx_ue_alt = PARAM_INDEX("ue_altitude_m");

  const int idx_scenario = PARAM_INDEX("scenario");
  const int idx_los = PARAM_INDEX("los_mode");
  const int idx_terminal = PARAM_INDEX("terminal_type");
  const int idx_bel_percentile = PARAM_INDEX("bel_percentile");
  const int idx_tdl = PARAM_INDEX("tdl_profile");

  const int idx_ds = PARAM_INDEX("delay_spread_s");
  const int idx_k = PARAM_INDEX("k_factor_db");
  const int idx_tdl_max_doppler = PARAM_INDEX("tdl_max_doppler_hz");
  const int idx_cloud_water = PARAM_INDEX("cloud_liquid_water_kg_m2");
  const int idx_scint_nwet = PARAM_INDEX("scintillation_nwet");
  const int idx_scint_diameter = PARAM_INDEX("scintillation_antenna_diameter_m");
  const int idx_scint_efficiency = PARAM_INDEX("scintillation_antenna_efficiency");
  const int idx_scint_exceedance = PARAM_INDEX("scintillation_exceedance_percent");
  const int idx_cal = PARAM_INDEX("link_budget_offset_db");
  const int idx_fixed_doppler = PARAM_INDEX("fixed_doppler_hz");

    const int idx_rain_mode =
      PARAM_INDEX("rain_mode");

  const int idx_rain_rate =
      PARAM_INDEX("rain_rate_mm_per_h");

  const int idx_rain_height =
      PARAM_INDEX("rain_height_km");

  const int idx_rain_polarization =
      PARAM_INDEX("rain_polarization_tilt_deg");

  const int idx_fspl = PARAM_INDEX("enable_fspl");
  const int idx_clutter = PARAM_INDEX("enable_clutter");
  const int idx_shadow = PARAM_INDEX("enable_shadow_fading");
  const int idx_bel = PARAM_INDEX("enable_building_entry_loss");

  const int idx_gas = PARAM_INDEX("enable_gas");
  const int idx_rain = PARAM_INDEX("enable_rain");
  const int idx_cloud = PARAM_INDEX("enable_cloud");
  const int idx_scint = PARAM_INDEX("enable_scintillation");

  const int idx_gas_pressure =
    PARAM_INDEX("gas_surface_pressure_hpa");

const int idx_gas_temperature =
    PARAM_INDEX("gas_surface_temperature_k");

const int idx_gas_rho =
    PARAM_INDEX(
        "gas_surface_water_vapour_density_g_m3");

const int idx_gas_integrated_water =
    PARAM_INDEX(
        "gas_integrated_water_vapour_kg_m2");

  const int idx_fast = PARAM_INDEX("enable_fast_fading");
  const int idx_delay = PARAM_INDEX("enable_propagation_delay");
  const int idx_fractional = PARAM_INDEX("enable_fractional_delay");
  const int idx_doppler = PARAM_INDEX("enable_doppler");
  const int idx_noise = PARAM_INDEX("enable_noise");

  const int idx_seed = PARAM_INDEX("random_seed");
  const int idx_update = PARAM_INDEX("update_period_s");

#undef PARAM_INDEX

  for (int i = 0; i < haps_list.numelt; ++i) {
    const char *model_name =
        *(haps_list.paramarray[i][idx_model].strptr);

    if (model_name == NULL ||
        strcmp(model_name, requested_model_name) != 0) {
      continue;
    }

    snprintf(
        cfg->model_name,
        sizeof(cfg->model_name),
        "%s",
        model_name);

    cfg->carrier_frequency_hz =
        *(haps_list.paramarray[i][idx_fc].dblptr);

    cfg->sample_rate_hz =
        *(haps_list.paramarray[i][idx_fs].dblptr);

    cfg->haps_latitude_deg =
        *(haps_list.paramarray[i][idx_haps_lat].dblptr);

    cfg->haps_longitude_deg =
        *(haps_list.paramarray[i][idx_haps_lon].dblptr);

    cfg->haps_altitude_m =
        *(haps_list.paramarray[i][idx_haps_alt].dblptr);

    cfg->ue_latitude_deg =
        *(haps_list.paramarray[i][idx_ue_lat].dblptr);

    cfg->ue_longitude_deg =
        *(haps_list.paramarray[i][idx_ue_lon].dblptr);

    cfg->ue_altitude_m =
        *(haps_list.paramarray[i][idx_ue_alt].dblptr);

    cfg->scenario =
        parse_scenario(
            *(haps_list.paramarray[i][idx_scenario].strptr));

    cfg->los_mode =
        parse_los_mode(
            *(haps_list.paramarray[i][idx_los].strptr));

    cfg->terminal_type =
        parse_terminal_type(
            *(haps_list.paramarray[i][idx_terminal].strptr));
    
    cfg->bel_percentile =
    *(haps_list.paramarray[i][idx_bel_percentile].dblptr);

    cfg->tdl_profile =
        parse_tdl_profile(
            *(haps_list.paramarray[i][idx_tdl].strptr));

    cfg->delay_spread_s =
        *(haps_list.paramarray[i][idx_ds].dblptr);

    cfg->k_factor_db =
        *(haps_list.paramarray[i][idx_k].dblptr);
    cfg->tdl_max_doppler_hz = *haps_list.paramarray[i][idx_tdl_max_doppler].dblptr;
    cfg->cloud_liquid_water_kg_m2 = *haps_list.paramarray[i][idx_cloud_water].dblptr;
    cfg->scintillation_nwet = *haps_list.paramarray[i][idx_scint_nwet].dblptr;
    cfg->scintillation_antenna_diameter_m = *haps_list.paramarray[i][idx_scint_diameter].dblptr;
    cfg->scintillation_antenna_efficiency = *haps_list.paramarray[i][idx_scint_efficiency].dblptr;
    cfg->scintillation_exceedance_percent = *haps_list.paramarray[i][idx_scint_exceedance].dblptr;

    cfg->link_budget_offset_db =
        *(haps_list.paramarray[i][idx_cal].dblptr);


   cfg->gas_surface_pressure_hpa =
    *(haps_list.paramarray[i]
          [idx_gas_pressure].dblptr);

cfg->gas_surface_temperature_k =
    *(haps_list.paramarray[i]
          [idx_gas_temperature].dblptr);

cfg->gas_surface_water_vapour_density_g_m3 =
    *(haps_list.paramarray[i]
          [idx_gas_rho].dblptr);

cfg->gas_integrated_water_vapour_kg_m2 =
    *(haps_list.paramarray[i]
          [idx_gas_integrated_water].dblptr);

   
   cfg->fixed_doppler_hz =
    *(haps_list.paramarray[i][idx_fixed_doppler].dblptr);
 
    cfg->rain_mode =
        *(haps_list.paramarray[i]
              [idx_rain_mode].iptr);

    cfg->rain_rate_mm_per_h =
        *(haps_list.paramarray[i]
              [idx_rain_rate].dblptr);

    cfg->rain_height_km =
        *(haps_list.paramarray[i]
              [idx_rain_height].dblptr);

    cfg->rain_polarization_tilt_deg =
        *(haps_list.paramarray[i]
              [idx_rain_polarization].dblptr);

   cfg->enable_fspl =
        *(haps_list.paramarray[i][idx_fspl].iptr) != 0;

    cfg->enable_clutter =
        *(haps_list.paramarray[i][idx_clutter].iptr) != 0;

    cfg->enable_shadow_fading =
        *(haps_list.paramarray[i][idx_shadow].iptr) != 0;

    cfg->enable_building_entry_loss =
        *(haps_list.paramarray[i][idx_bel].iptr) != 0;

    cfg->enable_gas =
        *(haps_list.paramarray[i][idx_gas].iptr) != 0;

    cfg->enable_rain =
        *(haps_list.paramarray[i][idx_rain].iptr) != 0;

    cfg->enable_cloud =
        *(haps_list.paramarray[i][idx_cloud].iptr) != 0;

    cfg->enable_scintillation =
        *(haps_list.paramarray[i][idx_scint].iptr) != 0;

    cfg->enable_fast_fading =
        *(haps_list.paramarray[i][idx_fast].iptr) != 0;

    cfg->enable_propagation_delay =
        *(haps_list.paramarray[i][idx_delay].iptr) != 0;

    cfg->enable_fractional_delay =
        *(haps_list.paramarray[i][idx_fractional].iptr) != 0;

    cfg->enable_doppler =
        *(haps_list.paramarray[i][idx_doppler].iptr) != 0;

    cfg->enable_noise =
        *(haps_list.paramarray[i][idx_noise].iptr) != 0;

    cfg->random_seed =
        *(haps_list.paramarray[i][idx_seed].uptr);

    cfg->update_period_s =
        *(haps_list.paramarray[i][idx_update].dblptr);
 
    
    if (cfg->enable_rain) {

      if (cfg->rain_mode !=
              HAPS_RAIN_FIXED_UNIFORM_LAYER &&
          cfg->rain_mode !=
              HAPS_RAIN_ITU_STATISTICAL) {

        LOG_E(
            HW,
            "Invalid HAPS rain mode: %d\n",
            cfg->rain_mode);

        return -1;
      }

      if (!isfinite(
              cfg->rain_rate_mm_per_h) ||
          cfg->rain_rate_mm_per_h < 0.0) {

        LOG_E(
            HW,
            "Invalid HAPS rain rate: %.6f mm/h\n",
            cfg->rain_rate_mm_per_h);

        return -1;
      }

      if (!isfinite(
              cfg->rain_height_km) ||
          cfg->rain_height_km < 0.0) {

        LOG_E(
            HW,
            "Invalid HAPS rain height: %.6f km\n",
            cfg->rain_height_km);

        return -1;
      }

      if (!isfinite(
              cfg->rain_polarization_tilt_deg) ||
          cfg->rain_polarization_tilt_deg <
              0.0 ||
          cfg->rain_polarization_tilt_deg >
              90.0) {

        LOG_E(
            HW,
            "Invalid HAPS rain polarization "
            "tilt: %.6f deg\n",
            cfg->rain_polarization_tilt_deg);

        return -1;
      }

      if (cfg->rain_mode ==
          HAPS_RAIN_ITU_STATISTICAL) {

        LOG_E(
            HW,
            "HAPS ITU statistical rain mode "
            "is not implemented yet\n");

        return -1;
      }
    }


    LOG_I(
    HW,
    "HAPS config loaded: model=%s, fc=%.3f GHz, "
    "fs=%.2f Msps, altitude=%.1f m, "
    "FSPL=%d Doppler=%d fixedDoppler=%.2f Hz\n",
    cfg->model_name,
    cfg->carrier_frequency_hz / 1.0e9,
    cfg->sample_rate_hz / 1.0e6,
    cfg->haps_altitude_m,
    cfg->enable_fspl,
    cfg->enable_doppler,
    cfg->fixed_doppler_hz);


    LOG_I(
        HW,
        "HAPS GAS CONFIG: "
        "model=%s enable_gas=%d "
        "P=%.2f hPa T=%.2f K "
        "rho=%.3f g/m3 V=%.3f kg/m2\n",
        cfg->model_name,
        cfg->enable_gas,
        cfg->gas_surface_pressure_hpa,
        cfg->gas_surface_temperature_k,
        cfg->gas_surface_water_vapour_density_g_m3,
        cfg->gas_integrated_water_vapour_kg_m2);


    LOG_I(
        HW,
        "HAPS RAIN CONFIG: "
        "model=%s enable_rain=%d "
        "mode=%d R=%.3f mm/h "
        "rain_height=%.3f km "
        "polarization_tilt=%.2f deg\n",
        cfg->model_name,
        cfg->enable_rain,
        cfg->rain_mode,
        cfg->rain_rate_mm_per_h,
        cfg->rain_height_km,
        cfg->rain_polarization_tilt_deg);

  return 0;
  }

  LOG_E(
      HW,
      "No HAPS config entry found for model %s\n",
      requested_model_name);

  return -1;
}
