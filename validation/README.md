# Validation Results

This directory contains reference results used to check the numerical behavior
and RFsimulator integration of the HAPS channel model.

The dataset is intended for regression testing and result comparison. It is not
a formal 3GPP or ITU-R conformance test suite.

## Files

| File | Purpose |
|---|---|
| [`reference_results.csv`](reference_results.csv) | Reference geometry, delay, propagation-loss, and link-status observations |
| [`rfsim_cfra_v0.2.0_results.csv`](rfsim_cfra_v0.2.0_results.csv) | UL and DL measurements from the v0.2.0 gNB–UE CFRA runs |

## Reference Setup

Unless a row states otherwise, the reference cases use:

| Parameter | Value |
|---|---:|
| HAPS altitude | 20 km |
| Carrier frequency | 3.6192 GHz |
| Sample rate | 61.44 Msps |
| Scenario | Rural |
| Terminal | Outdoor |

The baseline geometry cases cover elevation angles of 90°, 60°, 30°, and 10°.

## Validation Coverage

The `test_group` column identifies the purpose of each row:

| Test group | Validation target |
|---|---|
| `geometry_delay` | Slant range, one-way delay, integer-sample delay, quantization error, and FSPL |
| `los_probability` | OAI LOS probability against the 3GPP reference value |
| `probabilistic_los` | LOS/NLOS realization from the random draw and LOS probability |
| `shadow_fading_los` | LOS shadow-fading standard deviation |
| `shadow_fading_clutter_nlos` | NLOS shadow-fading standard deviation and clutter loss |
| `shadow_fading_seed` | Repeatability of seeded shadow-fading realizations |
| `bel_elevation` | Building entry loss across elevation angles |
| `bel_terminal_type` | Outdoor and indoor terminal-type comparison |
| `bel_percentile` | Building entry loss at selected percentiles |
| `nlos_scenario` | Rural, urban, and dense-urban NLOS comparison |
| `rain_attenuation` | Rain-path length and rain attenuation |
| `gas_attenuation` | Atmospheric gas attenuation |

## Delay Validation

The v0.1.2 default follows the manuscript's nearest rule for non-negative delays:

```math
N = \left\lfloor \tau F_s + \tfrac12 \right\rfloor, \qquad
\varepsilon_q = N/F_s - \tau
```

Half-sample ties round upward. The absolute one-way error is at most half a
sample period: 8.138 ns at 61.44 Msps. `HAPS_DELAY_ROUNDING_CEIL` retains the
former upward mode for comparisons. This is distinct from the final I/Q
`lroundf` quantization, which has not changed.

Run `python3 validation/test_delay.py --sanitize` for real C-source checks of
both modes, binary-exact tie boundaries, delay-off behavior and unchanged
I/Q gain. It needs Python 3 and a C compiler, uses temporary OAI API stubs,
and does not run RFsim. Leak detection is disabled under the hosted runner;
address and undefined-behavior instrumentation remain enabled. The checked
results are in [delay_rounding_results.json](delay_rounding_results.json).

| Elevation | Exact samples | v0.1.2 nearest | Legacy ceil | Nearest one-way error (ns) | RTT quantization error (samples) |
|---:|---:|---:|---:|---:|---:|
| 90° | 4098.835602 | 4099 | 4099 | +2.676 | +0.328796 |
| 60° | 4730.467840 | 4730 | 4731 | -7.615 | -0.935681 |
| 30° | 8159.641354 | 8160 | 8160 | +5.837 | +0.717292 |
| 10° | 22525.095525 | 22525 | 22526 | -1.555 | -0.191050 |

The RTT column is `2*(N - tau*Fs)`: the component due to quantizing two
identical channel legs, not a measurement of the UE's complete TA residual.
Its magnitude is below one sample for these four geometries.

The existing `reference_results.csv` is historical data using **ceil** and
remains unchanged. The original author E8/E9/SA reruns also used ceil. Do not
relabel those measurements as v0.1.2 or silently change their sample counts.
See [the delay guide](../docs/DELAY_QUANTIZATION.md) for source provenance and
full/incremental integration patches.

## How to Read the Dataset

- `record_id` is the unique identifier for a validation observation.
- `test_group` identifies the validation family.
- `reference_standard` identifies the main model or standard used for the
  comparison.
- Columns ending in `_oai` contain model output, while matching `_reference`
  columns contain the expected value.
- `channel_mode` records forced or probabilistic LOS/NLOS operation.
- `realized_state` records the LOS/NLOS state selected for that observation.
- `source_sheet` and `source_row` preserve the origin of manually consolidated
  results.
- Empty cells mean that a field is not applicable or was not recorded for that
  test.
- `link_status` records the observed end-to-end connection result. For
  `no_connection` rows, `snr_db` is intentionally left empty.

## Comparison Rules

Use the following checks when repeating a validation case:

1. Keep the carrier frequency, sample rate, geometry, scenario, LOS mode,
   terminal type, and random seed equal to the reference row.
2. Compare OAI output with the corresponding reference fields at the precision
   stored in the CSV.
3. For probabilistic LOS, verify that the realized state follows
   `random_draw < plos_oai` for LOS and otherwise NLOS.
4. For v0.1.2 default delay, verify `floor(tau*Fs + 0.5)` and an absolute error
   at most half a sample. Historical CSV rows instead retain their documented
   `ceil` rule and a non-negative error below one sample.
5. Treat `link_status` as an integration observation, not as proof that an
   individual propagation formula is correct.

## Adding New Results

When adding a validation row:

1. Use a unique, descriptive `record_id`.
2. Reuse an existing `test_group`, or document a new group in this file.
3. Store numeric values without unit text; units are defined in column names.
4. Leave non-applicable fields empty instead of inserting zero.
5. Record the standard, scenario, channel mode, realized state, and input
   conditions required to reproduce the result.
6. Use `connected` or `no_connection` in `link_status`.
7. Add a short note when the row needs interpretation or has a special test
   condition.

## Atmosphere and NTN-TDL validation

Run `python3 validation/test_delay.py --atmosphere-tdl --sanitize` to check the
v0.2.0 C module with OAI type and log stubs. The probe checks cloud and
scintillation reference values, n78 scintillation behavior, invalid inputs,
loss-to-IQ gain, all four TDL delay profiles, K-factor scaling, seeded replay,
UL/DL stream separation and the timestamped IQ impulse response across blocks.
Use `--source-dir /path/to/oai/radio/rfsimulator/haps` to check the integrated
source snapshot.

## gNB–UE RFsim CFRA measurements

The [v0.2.0 CFRA CSV](rfsim_cfra_v0.2.0_results.csv) contains 30 link records
from 15 completed runs on 13 November 2026. Each run has one UL row from
the gNB receiver and one DL row from the UE receiver. Both modems used OAI
`9313cbea82bc3c6b8ff695dd808a0cde5f431724` and the RFsim HAPS sources from
v0.2.0 (`b8d489e`).

The modems ran as separate processes on TCP loopback in coreless `--do-ra`
mode. All cases used SISO, 106 PRBs, 30 kHz subcarrier spacing, 61.44 Msps,
a 20 km HAPS at 90° elevation, and HAPS seed 1. The enabled configurations
used 0.5 kg/m² liquid water for cloud attenuation and a 100 ns RMS delay
spread with 30 Hz maximum local Doppler for NTN-TDL-D.

| Carrier | Channel effects | Completed CFRA |
|---|---|---:|
| n78, 3.6192 GHz | Baseline | 3/3 |
| n78, 3.6192 GHz | Cloud, scintillation and NTN-TDL-D | 3/3 |
| n79, 4.42002 GHz | Baseline | 3/3 |
| n79, 4.42002 GHz | Cloud, scintillation and NTN-TDL-D | 3/3 |
| n79, 4.42002 GHz | Cloud only | 1/1 |
| n79, 4.42002 GHz | Scintillation only | 1/1 |
| n79, 4.42002 GHz | NTN-TDL-D only | 1/1 |

The UE synchronized, received the random access response, and reported CFRA
completion in each included run. The gNB reported CFRA completion and
successful PUSCH reception. The first n78 baseline launch stopped during
host TUN setup before RFsim. A local UE build skipped that setup for the
coreless CFRA runs, which is why the n78 baseline records begin at trial 2.
The test covers radio access at a fixed seed. SA registration, PDU sessions,
IP traffic and BLER were outside its scope.
