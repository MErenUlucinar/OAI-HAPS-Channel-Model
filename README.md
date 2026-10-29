# OAI RFsimulator HAPS Channel Model: 3GPP and ITU-R-Based HAPS Channel Emulation for OAI RFsimulator

An experimental High-Altitude Platform Station (HAPS) channel model for the
OpenAirInterface (OAI) RFsimulator. The implementation combines a static
WGS-84-based HAPS-to-UE geometry with large-scale propagation effects based on
3GPP TR 38.811 and selected ITU-R recommendations.

> **Current version:** v0.1.2 — Experimental (nearest-sample delay quantization; RNG fix retained)

> This project is a research prototype. It is not an official 3GPP or ITU
> conformance implementation.

## Documentation

- [User Guide](docs/USER_GUIDE.md)
- [OAI Integration Guide](integration/README.md)
- [v0.1.2 author RFsim integration patch (OAI 9313cbe)](integration/patches/author_rfsim_v0.1.2_9313cbe.patch)
- [Delay quantization, legacy reproduction and tests](docs/DELAY_QUANTIZATION.md)
- [Source Module Guide](src/README.md)
- [Runnable configuration profiles (OAI 9313cbe)](examples/README.md)
- [Configuration review fixes C1--C7](docs/CONFIGURATION_FIXES.md)
- [Example HAPS Configuration](examples/channelmod_haps_3gpp_itu.conf)
- [Validation Guide](validation/README.md)
- [RNG fix, validation, and E2/E3 clutter setting](docs/RNG_FIX.md)
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



## Intended Applications

This project can be used for:

- Reproducible HAPS-to-UE link experiments in OAI RFsimulator
- Evaluation of elevation-dependent slant range, propagation delay, and FSPL
- LOS/NLOS, clutter, shadow-fading, and building-entry-loss analysis
- Atmospheric gas and fixed rain-layer attenuation studies
- IQ-level evaluation of amplitude attenuation, integer-sample delay, and fixed Doppler
- Baseline development for future dynamic geometry, fractional delay, NTN-TDL, and MIMO extensions

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

The published [author integration](integration/README.md) applies to OAI commit
`9313cbea82bc3c6b8ff695dd808a0cde5f431724`. Its self-contained patch includes
the v0.1.2 HAPS source snapshot, `simulator.cpp` receive-path integration,
model registration, build changes, and the CFRA/SIB1 access fixes.

Follow the integration guide to create a separate OAI worktree, apply the
patch and verify its source checksums. Then use the matched
[configuration profiles](examples/README.md) for the selected elevation and
experiment mode. The patch already installs its HAPS sources.

For configuration, log interpretation, calculations, verification, and basic
troubleshooting, see the [User Guide](docs/USER_GUIDE.md). The integration guide
records the pinned OAI revision, application/build instructions and validation
scope.

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

The default example enables integer propagation delay. The nominal SA
profile keeps clutter and shadow fading off; E9 uses the separate
`channelmod_haps_3gpp_itu.e9-delay-clutter-on.conf` profile with both enabled.
Use matched UE/gNB/channel files per elevation; see [examples](examples/README.md).
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
| 60° | 23.082 km | 76.993292 µs | 4730 | 130.89 dB |
| 30° | 39.814 km | 132.806663 µs | 8160 | 135.62 dB |
| 10° | 109.910 km | 366.619393 µs | 22525 | 144.44 dB |

These values are intended as reference calculations for geometry, FSPL, and
integer-sample delay verification with v0.1.2's `floor(tau*Fs + 0.5)` rule
(half-sample ties upward). Current standalone C results are in
[`validation/delay_rounding_results.json`](validation/delay_rounding_results.json).
The historical [`validation/reference_results.csv`](validation/reference_results.csv)
is preserved unchanged and contains the former `ceil` values. The original
E8/E9/SA author runs also used that frozen `ceil` integration; they have not
been relabeled as v0.1.2 runs.


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

The original HAPS module source code and author-developed integration changes
are licensed under the Collaborative Standards Software License v1.0 (CSSL),
unless a file explicitly states otherwise. See [LICENSE](LICENSE) for the full
terms and [NOTICE](NOTICE) for scope and upstream attribution.

The license grant also explicitly covers the original HAPS code in the already
released **v0.1.2** snapshot at commit
`b9960cced8833f8601c9a6d2a7e51c39241f5681`; see the
[version-specific licensing declaration](docs/LICENSING-v0.1.2.md).
The existing v0.1.2 tag and source archive remain unchanged.

OpenAirInterface-derived code retains its applicable upstream license and
attribution notices. The integration patches target OAI commit
`9313cbea82bc3c6b8ff695dd808a0cde5f431724`; the seven modified upstream files
carry `SPDX-License-Identifier: LicenseRef-CSSL-1.0`. A verbatim copy of the
upstream NOTICE is included at [LICENSES/OAI-NOTICE.txt](LICENSES/OAI-NOTICE.txt).

CSSL includes separate patent provisions for research and other uses. Existing
third-party notices remain applicable; this statement does not relicense
third-party material or the referenced 3GPP/ITU-R standards publications.

## Citation

Citation metadata will be added before the public v0.1.0 release. If this model
is used in academic work, please cite the software release and the associated
technical publication when available.
