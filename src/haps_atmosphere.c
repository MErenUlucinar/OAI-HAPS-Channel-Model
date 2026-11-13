#include "haps_atmosphere.h"

#include <math.h>

#define HAPS_DEG_TO_RAD 0.01745329251994329577

int haps_cloud_loss_db(const haps_config_t *cfg, double elevation_deg, double *loss_db)
{
  const double f = cfg->carrier_frequency_hz / 1e9;
  const double water = cfg->cloud_liquid_water_kg_m2;
  if (!(f >= 1 && f <= 200) || !(elevation_deg > 0 && elevation_deg <= 90) ||
      !isfinite(water) || water < 0)
    return -1;

  /* ITU-R P.840-9, section 3.1: instantaneous column with the frequency correction. */
  const double theta = 300.0 / 273.75 - 1;
  const double eps0 = 77.66 + 103.3 * theta;
  const double eps1 = 0.0671 * eps0;
  const double eps2 = 3.52;
  const double fp = 20.20 - 146 * theta + 316 * theta * theta;
  const double rp = f / fp;
  const double rs = rp / 39.8;
  const double primary = (eps0 - eps1) / (1 + rp * rp);
  const double secondary = (eps1 - eps2) / (1 + rs * rs);
  const double real = primary + secondary + eps2;
  const double imag = rp * primary + rs * secondary;
  const double eta = (2 + real) / imag;
  const double kl = 0.819 * f / (imag * (1 + eta * eta));
  const double delta1 = f + 23.9589;
  const double delta2 = f - 219.2096;
  const double correction = 0.1522 * exp(-delta1 * delta1 / 3299.1)
                          + 11.51 * exp(-delta2 * delta2 / 2759500.0) - 10.4912;
  *loss_db = kl * correction * water / sin(elevation_deg * HAPS_DEG_TO_RAD);
  return isfinite(*loss_db) ? 0 : -1;
}

int haps_scintillation_loss_db(const haps_config_t *cfg, double elevation_deg, double *loss_db)
{
  const double f = cfg->carrier_frequency_hz / 1e9;
  const double nwet = cfg->scintillation_nwet;
  const double diameter = cfg->scintillation_antenna_diameter_m;
  const double efficiency = cfg->scintillation_antenna_efficiency;
  const double p = cfg->scintillation_exceedance_percent;
  if (!(f > 0 && f <= 55) || !(elevation_deg > 0 && elevation_deg <= 90) ||
      !isfinite(nwet) || nwet < 0 || !isfinite(diameter) || diameter <= 0 ||
      !(efficiency > 0 && efficiency <= 1) || !(p > 0.01 && p <= 50))
    return -1;

  *loss_db = 0;
  /* ITU-R P.619-5 treats tropospheric scintillation below 4 GHz as negligible. */
  if (f < 4)
    return 0;
  if (elevation_deg < 5)
    return -1;

  /* ITU-R P.618-14, section 2.4.1: fade depth for a fixed exceedance percentage. */
  const double sine = sin(elevation_deg * HAPS_DEG_TO_RAD);
  const double length_m = 2000 / (sqrt(sine * sine + 2.35e-4) + sine);
  const double x = 1.22 * efficiency * diameter * diameter * f / length_m;
  if (x >= 7)
    return 0;
  const double averaging = sqrt(fmax(0, 3.86 * pow(x * x + 1, 11.0 / 12)
                                        * sin((11.0 / 6) * atan2(1, x)) - 7.08 * pow(x, 5.0 / 6)));
  const double sigma_ref = 3.6e-3 + 1e-4 * nwet;
  const double sigma = sigma_ref * pow(f, 7.0 / 12) * averaging / pow(sine, 1.2);
  const double log_p = log10(p);
  const double time_factor = ((-0.061 * log_p + 0.072) * log_p - 1.71) * log_p + 3;
  *loss_db = time_factor * sigma;
  return isfinite(*loss_db) ? 0 : -1;
}
