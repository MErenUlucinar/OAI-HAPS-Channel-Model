# OAI RFsimulator HAPS Channel Model — User Guide

This guide describes how to configure, run, and verify version `v0.1.0` of the
experimental HAPS channel model for OpenAirInterface (OAI) RFsimulator.

> This software is a research prototype. It is not an official 3GPP or ITU
> conformance implementation.

## 1. Scope of v0.1.0

The current version models a static HAPS-to-UE radio link and applies the
resulting large-scale channel effects to RFsimulator IQ samples.

The implemented processing path is:

```text
OAI configuration
        ↓
HAPS–UE geometry
        ↓
Large-scale propagation losses
        ↓
Integer-sample delay, IQ attenuation, and fixed Doppler
        ↓
RFsimulator receive path
```

The release supports:

- Static WGS-84 HAPS and UE coordinates
- Slant range, elevation angle, and one-way propagation delay
- Free-space path loss (FSPL)
- Forced LOS, forced NLOS, and probabilistic LOS/NLOS selection
- 3GPP-based clutter loss and shadow fading
- ITU-R-based building entry, atmospheric gas, and rain losses
- Integer-sample propagation delay
- IQ amplitude attenuation
- Optional fixed Doppler with continuous phase
- Separate uplink and downlink channel contexts

For the current configuration set, use the [pinned OAI profiles](../examples/README.md)
and the [C1--C7 configuration fixes](CONFIGURATION_FIXES.md). The profiles target
OAI `9313cbea82bc3c6b8ff695dd808a0cde5f431724`; an arbitrary moving `develop`
branch is not assumed compatible. The default is the zenith SA profile with
propagation delay enabled, PLMN 208/99, SST 1 without SD, and DNN `oai`.

## 2. Prerequisites

Before integrating the model, prepare:

- A Linux development environment supported by OAI
- A working OAI RFsimulator build
- The HAPS source and header files
- A HAPS channel configuration file
- A compatible OAI source revision

The public release should identify the exact tested OAI commit or tag. OAI file
paths and build commands may change between revisions, so use the revision
specified by the release rather than an arbitrary current branch.

## 3. Source Modules

| Module | Responsibility |
|---|---|
| `haps_config.c/.h` | Reads, validates, and stores HAPS parameters |
| `haps_geometry.c/.h` | Computes ECEF coordinates, slant range, elevation, and delay |
| `haps_propagation.c/.h` | Selects LOS/NLOS state and computes large-scale losses |
| `haps_gas.c/.h` | Computes oxygen and water-vapour attenuation |
| `haps_rain.c/.h` | Computes fixed rain-layer attenuation |
| `haps_channel.c/.h` | Connects the model to RFsimulator IQ processing |

The configuration and data flow is:

```text
channelmod_haps_3gpp_itu.conf
        ↓
haps_config_t
        ↓
haps_geometry
        ↓
haps_propagation / haps_gas / haps_rain
        ↓
haps_channel
        ↓
simulator.cpp
```

## 4. OAI Integration Overview

At a high level, integration requires the following changes in the compatible
OAI source tree:

1. Add the HAPS source and header files to the RFsimulator implementation.
2. Register the `HAPS_3GPP_ITU` channel type in OAI's channel model mapping.
3. Add the HAPS sources to the relevant CMake target.
4. Create a HAPS channel context for each selected UL or DL link.
5. Connect `haps_channel_process()` to the RFsimulator receive path.
6. Transfer the calculated integer delay to OAI's `channel_offset` mechanism.
7. Add the `hapsmod` configuration records.

This release reuses OAI's channel selection, link management, receive path, and
integer-sample offset mechanisms. The HAPS-specific geometry and propagation
calculations are implemented in separate modules.

For the file-by-file connection points and recommended integration order, see
the [OAI Integration Guide](../integration/README.md).

## 5. Configuration

HAPS parameters are read from the `hapsmod` list in
`channelmod_haps_3gpp_itu.conf`. A separate record can be used for each link.
The repository provides a documented baseline in
[`examples/channelmod_haps_3gpp_itu.conf`](../examples/channelmod_haps_3gpp_itu.conf)
with the following link names:

```text
rfsimu_channel_enB0   # Downlink channel record
rfsimu_channel_ue0    # Uplink channel record
```

The `model_name` requested by RFsimulator must match the corresponding
`hapsmod` record. If the names do not match, the HAPS configuration cannot be
associated with that link.

### 5.1 Core Parameters

The main configuration groups are:

| Group | Examples |
|---|---|
| Radio | Centre frequency and sample rate |
| Geometry | HAPS and UE latitude, longitude, and altitude |
| Environment | `scenario`, LOS mode, and terminal type |
| Randomization | `random_seed` |
| Calibration | `link_budget_offset_db` |
| Optional effects | Clutter, shadow fading, BEL, gas, rain, delay, and Doppler flags |
| Atmospheric inputs | Pressure, temperature, water-vapour density, and integrated water vapour |
| Rain inputs | Rain rate, rain height, and polarization angle |

Supported LOS operating modes are:

- `forced_los`: always use the LOS state
- `forced_nlos`: always use the NLOS state
- `probabilistic`: compare a seeded random draw with the 3GPP LOS probability

### 5.2 Recommended Initial Configuration

Start with a simple deterministic baseline:

```text
Scenario       : rural
LOS mode       : forced_los
Terminal       : outdoor
HAPS altitude  : 20 km
Carrier        : 3.6192 GHz
Sample rate    : 61.44 Msps
Fixed Doppler  : 0 Hz
```

Enable additional loss components one at a time. This makes it easier to
verify which component changes the final channel loss.

### 5.3 Configuration Behavior

- Outdoor terminals produce zero building entry loss.
- LOS links produce zero clutter loss in the current implementation.
- Shadow fading is generated only when its enable flag is active.
- A fixed `random_seed` makes probabilistic LOS and shadow-fading realizations
  repeatable.
- The RFsimulator runtime frequency and sample rate take precedence when they
  are supplied to the HAPS channel context.
- `link_budget_offset_db` is a simulation calibration value; it is not a
  physical propagation loss.

## 6. Running the Model

Use the normal startup procedure of the compatible OAI RFsimulator setup:

1. Select `HAPS_3GPP_ITU` for the required UL and DL channel records.
2. Load the configuration containing the corresponding `hapsmod` entries.
3. Build OAI with the HAPS source files included in the RFsimulator target.
4. Start RFsimulator and the gNB using the tested OAI procedure.
5. Start the UE.
6. Check the console for `HAPS ACTIVE` and `HAPS DELAY ACTIVE` messages.

Exact build and launch commands must be documented together with the tested OAI
commit because command-line options and source paths can differ between OAI
revisions.

## 7. Understanding the Logs

A typical channel log contains fields similar to:

```text
HAPS ACTIVE: link=rfsimu_channel_enB0 direction=DL
slant=20.000 km elevation=90.00 deg LOS=1 pLOS=0.998
FSPL=129.64 dB CL=0.00 dB sigmaSF=0.72 dB SF=0.00 dB
BEL=0.00 dB GAS=0.04 dB RAIN=0.00 dB
total_loss=129.68 dB applied_loss=0.04 dB
```

| Field | Meaning |
|---|---|
| `link` | RFsimulator channel record |
| `direction` | Uplink or downlink |
| `slant` | HAPS–UE line-of-sight distance |
| `elevation` | UE-to-HAPS elevation angle |
| `LOS` | Selected LOS (`1`) or NLOS (`0`) state |
| `pLOS` | LOS probability from the selected 3GPP table |
| `FSPL` | Free-space path loss |
| `CL` | Clutter loss |
| `sigmaSF` | Shadow-fading standard deviation |
| `SF` | Generated shadow-fading realization |
| `BEL` | Building entry loss |
| `GAS` | Atmospheric gas attenuation |
| `RAIN` | Rain attenuation |
| `total_loss` | Sum of enabled physical loss components |
| `applied_loss` | Loss applied after simulation calibration |

The delay log contains:

```text
HAPS DELAY ACTIVE: one_way_delay=66.712819 us
exact_samples=4098.835602 integer_samples=4099
fractional_residual=0.835602 applied_delay=66.715495 us
quant_error=+2.676 ns
```

| Field | Meaning |
|---|---|
| `one_way_delay` | Physical one-way delay calculated from slant range |
| `exact_samples` | Delay expressed as a non-integer number of samples |
| `integer_samples` | Delay rounded to nearest sample for `channel_offset` (half ties upward; v0.1.2 default) |
| `fractional_residual` | Fractional part of the exact sample delay |
| `applied_delay` | Delay represented by the integer sample count |
| `quant_error` | Difference between applied and physical delay |

## 8. Core Calculations

The one-way propagation delay is:

```math
\tau = \frac{d_{\mathrm{slant}}}{c}
```

The exact delay in samples is:

```math
N_{\mathrm{exact}} = \tau F_s
```

Version `v0.1.2` rounds non-negative delays to the nearest sample, with half-sample ties upward:

```math
N_{\mathrm{integer}} = \left\lfloor N_{\mathrm{exact}} + \tfrac12 \right\rfloor
```

The applied delay and quantization error are:

```math
\tau_{\mathrm{applied}} = \frac{N_{\mathrm{integer}}}{F_s}
```

```math
e_q = \tau_{\mathrm{applied}} - \tau
```

The quantization error depends on the fractional part of
`N_exact`; therefore, it does not increase linearly with distance or decrease
linearly with elevation. With nearest quantization its magnitude is at most
half a sample period (8.138 ns at 61.44 Msps). Versions v0.1.0/v0.1.1 used
`ceil`; define `HAPS_DELAY_ROUNDING_CEIL` when compiling HAPS C sources to
restore that mode, or use the frozen integration documented in the
[delay guide](DELAY_QUANTIZATION.md).

The free-space path loss is:

```math
L_{\mathrm{FSPL}} = 20\log_{10}\left(\frac{4\pi d_{\mathrm{slant}}f_c}{c}\right)
```

When all supported loss components are enabled:

```math
L_{\mathrm{total}} = L_{\mathrm{FSPL}} + L_{\mathrm{clutter}}
+ L_{\mathrm{shadow}} + L_{\mathrm{BEL}} + L_{\mathrm{gas}}
+ L_{\mathrm{rain}}
```

The calibrated loss and IQ amplitude gain are:

```math
L_{\mathrm{applied}} = \max\left(0,
L_{\mathrm{total}}-L_{\mathrm{offset}}\right)
```

```math
G_{\mathrm{IQ}} = 10^{-L_{\mathrm{applied}}/20}
```

If fixed Doppler is enabled, the received samples are processed as:

```math
y[n] = G_{\mathrm{IQ}}x[n]e^{j\phi[n]}
```

## 9. Reference Results

The following results use a 20 km HAPS altitude, a 3.6192 GHz carrier, and a
61.44 Msps sample rate:

| Elevation | Slant range | One-way delay | Exact samples | Integer samples | Quantization error | FSPL |
|---:|---:|---:|---:|---:|---:|---:|
| 90° | 20.000 km | 66.712819 µs | 4098.835602 | 4099 | +2.676 ns | 129.64 dB |
| 60° | 23.082 km | 76.993292 µs | 4730.467840 | 4730 | -7.615 ns | 130.89 dB |
| 30° | 39.814 km | 132.806663 µs | 8159.641354 | 8160 | +5.837 ns | 135.62 dB |
| 10° | 109.910 km | 366.619393 µs | 22525.095525 | 22525 | -1.555 ns | 144.44 dB |

As elevation decreases, the slant range, physical propagation delay, and FSPL
increase. This trend is physically consistent. The quantization errors do not
follow the same monotonic trend because each value is determined by a different
fractional sample residual.

The current standalone C results are in
[`validation/delay_rounding_results.json`](../validation/delay_rounding_results.json).
The historical [`validation/reference_results.csv`](../validation/reference_results.csv)
remains unchanged and records the former `ceil` mode; it should not be
interpreted as a new v0.1.2 measurement campaign.

## 10. Verification Procedure

Use the following sequence for an initial verification:

1. Select `forced_los`, an outdoor terminal, zero fixed Doppler, and a fixed
   random seed.
2. Disable optional loss components.
3. Verify slant range and elevation from the configured coordinates.
4. Recalculate one-way delay using `d_slant / c`.
5. Recalculate FSPL using the logged distance and runtime carrier frequency.
6. Verify that `integer_samples = floor(exact_samples + 0.5)` for the v0.1.2 default.
7. Verify that the absolute quantization error is at most half a sample period.
8. Enable clutter, shadow fading, BEL, gas, and rain individually.
9. Confirm that each enabled component appears in `total_loss`.
10. Repeat the run with the same seed and verify reproducible random results.

## 11. Basic Troubleshooting

| Observation | Check |
|---|---|
| No `HAPS ACTIVE` message | Confirm that `HAPS_3GPP_ITU` is registered and selected |
| HAPS configuration is not found | Check that RFsimulator `model_name` matches the `hapsmod` record |
| Delay is not reported | Check the delay enable flag and `channel_offset` integration |
| An optional loss remains zero | Check its enable flag and whether the selected link state permits that loss |
| BEL remains zero | Confirm that the terminal is configured as indoor |
| Clutter remains zero | Confirm that the selected link is NLOS |
| Results change between runs | Use the same configuration and `random_seed` |
| Applied loss looks unexpectedly small | Check `link_budget_offset_db` |
| Gas calculation is rejected | Check the implemented frequency and elevation limits |

## 12. Current Limitations

- `1 × 1` SISO operation only
- Static HAPS and UE geometry
- Fixed Doppler supplied through configuration
- Integer-sample propagation delay only
- No fractional-delay filtering
- No NTN-TDL fast fading
- No delay-spread or Rice K-factor processing
- No MIMO channel processing
- No internal channel noise model
- No cloud or scintillation attenuation
- No HAPS-specific SIB19 signalling


## 13. Before a Public Release

Before publishing `v0.1.0`, the repository should also contain:

- The exact tested OAI commit or tag
- Revision-specific integration instructions or a patch
- A working example configuration
- Verified build and launch commands
- The correct license for the selected OAI base revision
- A release tag and short changelog

## 14. References

- 3GPP TR 38.811, *Study on New Radio (NR) to support non-terrestrial networks*
- ITU-R P.2109-2, *Prediction of building entry loss*
- ITU-R P.676-13, *Attenuation by atmospheric gases and related effects*
- ITU-R P.838-3, *Specific attenuation model for rain for use in prediction methods*
- OpenAirInterface RFsimulator documentation and source code
