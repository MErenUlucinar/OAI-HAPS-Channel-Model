# OAI RFsimulator HAPS Channel Model: 3GPP and ITU-R-Based HAPS Channel Emulation for OAI RFsimulator

An experimental High-Altitude Platform Station (HAPS) channel model for the
OpenAirInterface (OAI) RFsimulator. The implementation combines a static
WGS-84-based HAPS-to-UE geometry with large-scale propagation effects based on
3GPP TR 38.811 and selected ITU-R recommendations.

> **Current version:** v0.1.0 — Experimental

> This project is a research prototype. It is not an official 3GPP or ITU
> conformance implementation.

## Documentation

- [User Guide](docs/USER_GUIDE.md)
- [OAI Integration Guide](integration/README.md)
- [Source Module Guide](src/README.md)
- [Example HAPS Configuration](examples/channelmod_haps_3gpp_itu.conf)
- [Validation Guide](validation/README.md)
- [Reference Validation Results](validation/reference_results.csv)


## Overview

The model calculates the HAPS-to-UE link geometry and large-scale propagation
losses, then applies the resulting amplitude gain, propagation delay, and
optional fixed Doppler shift to the received IQ samples in OAI RFsimulator.

```text
OAI configuration
        ↓
HAPS–UE geometry
        ↓
Large-scale propagation model
        ↓
Delay, IQ gain, and fixed Doppler
        ↓
OAI RFsimulator receive path
```

## Supported Features

- OAI configuration and RFsimulator integration
- Separate uplink and downlink channel contexts
- WGS-84-based static HAPS–UE geometry
- Slant range, elevation angle, and one-way propagation delay
- Integer-sample delay through the RFsimulator `channel_offset` mechanism
- Free-space path loss (FSPL)
- Forced LOS, forced NLOS, and probabilistic LOS/NLOS operation
- 3GPP-based clutter loss and shadow fading
- ITU-R-based building entry, atmospheric gas, and rain losses
- IQ amplitude attenuation
- Optional fixed Doppler with phase continuity between IQ blocks
- Repeatable random realizations using a configurable seed

## Model Components

| Component | Model or source |
|---|---|
| HAPS–UE geometry | WGS-84 |
| Free-space path loss | Friis equation |
| LOS probability, clutter, and shadow fading | 3GPP TR 38.811 |
| Building entry loss | ITU-R P.2109-2 |
| Atmospheric gas attenuation | ITU-R P.676-13 |
| Rain attenuation | ITU-R P.838-3 |
| IQ processing and sample delay | OAI RFsimulator integration |

Only the methods explicitly implemented in the source code are covered. The
project does not claim complete implementation of the referenced standards.

## Source Modules

| Module | Responsibility |
|---|---|
| `haps_config.c/.h` | Reads and validates HAPS configuration parameters |
| `haps_geometry.c/.h` | Computes ECEF coordinates, slant range, elevation, and delay |
| `haps_propagation.c/.h` | Computes LOS/NLOS state and large-scale propagation losses |
| `haps_gas.c/.h` | Computes oxygen and water-vapour attenuation |
| `haps_rain.c/.h` | Computes fixed rain-layer attenuation |
| `haps_channel.c/.h` | Connects the model to the RFsimulator IQ processing path |

The general data flow is:

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

## Core Equations

The one-way propagation delay is calculated as:

```math
\tau = \frac{d_{\mathrm{slant}}}{c}
```

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

The applied loss is converted into a linear IQ amplitude gain:

```math
G_{\mathrm{IQ}} = 10^{-L_{\mathrm{applied}}/20}
```

## Basic Usage

The current model is integrated into the OAI RFsimulator source tree. A basic
setup consists of the following steps:

1. Add the HAPS source and header files to the RFsimulator source tree.
2. Register the `HAPS_3GPP_ITU` channel type.
3. Add the HAPS sources to the OAI build configuration.
4. Connect HAPS channel processing to `simulator.cpp`.
5. Copy and adapt the example
   [`channelmod_haps_3gpp_itu.conf`](examples/channelmod_haps_3gpp_itu.conf).
6. Build and run the RFsimulator, gNB, and UE.

For configuration, log interpretation, calculations, verification, and basic
troubleshooting, see the [User Guide](docs/USER_GUIDE.md). Exact build and run
commands must be published together with the tested OAI revision.

## Configuration

The HAPS parameters are read from the `hapsmod` list. Separate records can be
used for downlink and uplink:

```text
rfsimu_channel_enB0   # Downlink record
rfsimu_channel_ue0    # Uplink record
```

The initial reference configuration uses:

```text
Scenario       : rural
LOS mode       : forced_los
Terminal       : outdoor
HAPS altitude  : 20 km
Carrier        : 3.6192 GHz
Sample rate    : 61.44 Msps
Fixed Doppler  : 0 Hz
```

Additional propagation effects can be enabled or disabled through the HAPS
configuration flags.

## Expected Output

A successful initialization produces log messages containing the link
direction, geometry, propagation losses, Doppler, and delay.

```text
HAPS ACTIVE: link=rfsimu_channel_enB0 direction=DL
slant=20.000 km elevation=90.00 deg LOS=1
FSPL=129.64 dB CL=0.00 dB SF=0.00 dB BEL=0.00 dB

HAPS DELAY ACTIVE: one_way_delay=66.712819 us
integer_samples=4099
```

The exact output depends on the selected geometry, enabled loss components,
configuration values, and random seed.

## Reference Results

The following geometry results use a 20 km HAPS altitude and a 61.44 Msps
sample rate:

| Elevation | Slant range | One-way delay | Integer samples | FSPL at 3.6192 GHz |
|---:|---:|---:|---:|---:|
| 90° | 20.000 km | 66.712819 µs | 4099 | 129.64 dB |
| 60° | 23.082 km | 76.993292 µs | 4731 | 130.89 dB |
| 30° | 39.814 km | 132.806663 µs | 8160 | 135.62 dB |
| 10° | 109.910 km | 366.619393 µs | 22526 | 144.44 dB |

These values are intended as reference calculations for geometry, FSPL, and
integer-sample delay verification. The same dataset, including exact sample
counts, applied delays, and quantization errors, is available in
[`validation/reference_results.csv`](validation/reference_results.csv).

## What This Project Adds to OAI

This project extends OAI RFsimulator with:

- WGS-84-based HAPS-to-UE geometry
- Elevation-dependent 3GPP TR 38.811 LOS, clutter, and shadow fading
- ITU-R-based building entry, atmospheric gas, and rain losses
- HAPS-specific UL and DL channel contexts
- IQ attenuation, integer-sample propagation delay, and fixed Doppler
- Reproducible validation cases and reference results

## Current Limitations

- `1 × 1` SISO operation only
- Static HAPS and UE geometry
- Fixed Doppler supplied through configuration
- Integer-sample propagation delay only
- No fractional-delay filtering
- No NTN-TDL fast fading
- No delay-spread or Rice K-factor processing
- No MIMO channel processing
- No cloud or scintillation attenuation
- No HAPS-specific SIB19 signalling


## References

- 3GPP TR 38.811, *Study on New Radio (NR) to support non-terrestrial networks*
- ITU-R P.2109-2, *Prediction of building entry loss*
- ITU-R P.676-13, *Attenuation by atmospheric gases and related effects*
- ITU-R P.838-3, *Specific attenuation model for rain for use in prediction methods*
- OpenAirInterface RFsimulator documentation and source code

## License

Licensing information will be provided in the repository `LICENSE` file. Any
distribution containing or modifying OpenAirInterface source code must remain
compatible with the applicable OAI licensing terms.

## Citation

Citation metadata will be added before the public v0.1.0 release. If this model
is used in academic work, please cite the software release and the associated
technical publication when available.
