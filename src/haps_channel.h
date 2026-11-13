#ifndef HAPS_CHANNEL_H
#define HAPS_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "openair1/SIMULATION/TOOLS/sim.h"
#include "haps_config.h"
#include "haps_geometry.h"
#include "haps_propagation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct haps_channel_ctx_s haps_channel_ctx_t;

typedef struct {
  bool enabled;

  double one_way_delay_s;
  double exact_samples;

  uint64_t floor_samples;
  double fractional_samples;

  uint64_t applied_integer_samples;
  double applied_delay_s;
  double quantization_error_s;
} haps_delay_info_t;



haps_channel_ctx_t *haps_channel_create(
    const haps_config_t *cfg,
    int nb_tx,
    int nb_rx,
    bool is_uplink,
    double runtime_sample_rate_hz,
    double runtime_center_frequency_hz);

int haps_channel_get_delay_info(
    const haps_channel_ctx_t *ctx,
    haps_delay_info_t *info);

int haps_channel_update(
    haps_channel_ctx_t *ctx,
    uint64_t timestamp,
    int nb_samples);

/* With NTN-TDL enabled, prepend this many older IQ samples to each input block.
 * The prefix excludes the geometric delay, which RFsim applies separately.
 */
unsigned haps_channel_get_history_samples(const haps_channel_ctx_t *ctx);

int haps_channel_process(
    haps_channel_ctx_t *ctx,
    c16_t **input,
    int nb_tx,
    cf_t **output,
    int nb_rx,
    int nb_samples,
    uint64_t timestamp);

int haps_channel_get_snapshot(
    const haps_channel_ctx_t *ctx,
    haps_geometry_result_t *geometry,
    haps_propagation_result_t *propagation);

void haps_channel_destroy(
    haps_channel_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
