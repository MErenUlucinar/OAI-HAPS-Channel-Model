# HAPS Source Modules

This directory contains the HAPS-specific source and header files developed for
the OpenAirInterface RFsimulator channel extension.

These files implement the HAPS configuration, geometry, large-scale
propagation, atmospheric attenuation, propagation delay, and IQ processing
functions. Modifications to existing OAI core files are documented separately
in [`../integration/README.md`](../integration/README.md).

## Module Overview

| Module | Responsibility |
|---|---|
| `haps_config.c/.h` | Reads and validates the `hapsmod` configuration records |
| `haps_geometry.c/.h` | Converts WGS-84 coordinates to ECEF and calculates slant range, elevation, and one-way delay |
| `haps_propagation.c/.h` | Calculates FSPL, LOS/NLOS state, clutter, shadow fading, BEL, gas loss, and total channel loss |
| `haps_gas.c/.h` | Calculates oxygen and water-vapour attenuation using ITU-R P.676-13 |
| `haps_rain.c/.h` | Calculates fixed uniform rain-layer attenuation using ITU-R P.838-3 |
| `haps_atmosphere.c/.h` | Computes ITU-R cloud attenuation and tropospheric scintillation fade depth |
| `haps_tdl.c/.h` | Scales 3GPP NTN-TDL profiles and generates time-varying complex taps |
| `haps_channel.c/.h` | Creates the HAPS context and applies delay information, IQ attenuation, and fixed Doppler |

## Processing Flow

```text
haps_config
    ↓
haps_geometry
    ↓
haps_propagation
    ├── haps_gas
    ├── haps_atmosphere
    └── building entry, clutter and shadow fading
    ↓
haps_channel
    ├── haps_rain
    ├── haps_tdl
    ├── propagation-delay information
    └── IQ attenuation and fixed Doppler
```
