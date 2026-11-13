#ifndef HAPS_ATMOSPHERE_H
#define HAPS_ATMOSPHERE_H

#include "haps_config.h"

/* Fixed atmospheric inputs evaluated at the link's actual elevation. */
int haps_cloud_loss_db(const haps_config_t *cfg, double elevation_deg, double *loss_db);
int haps_scintillation_loss_db(const haps_config_t *cfg, double elevation_deg, double *loss_db);

#endif
