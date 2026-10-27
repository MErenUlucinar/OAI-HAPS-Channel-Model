# OAI Integration Guide

This document describes where the HAPS channel model connects to the
OpenAirInterface (OAI) RFsimulator source tree.

> The integration points below describe the current `v0.1.0` prototype. Exact
> line numbers and patch contents depend on the selected OAI commit or tag.

## Tested configuration compatibility

The configuration examples target OAI `develop` commit
`9313cbea82bc3c6b8ff695dd808a0cde5f431724`. See the
[profile instructions](../examples/README.md). They use `do_SRS = "none"`, a
list-type `rfsimulator` section and `min_rxtxtime = 6`.

The OAI integration also needs the CFRA common-occasion assignment and the gNB
SIB1 NTN-access condition for band 78 whenever `ntn_Config_r17` is configured.
Changing configuration syntax alone does not implement these source fixes.
The author's pinned E8/E9/SA build contains them. The existing source and
integration-patch publication is a separate change; this configuration update
does not replace a complete OAI integration patch.

## 1. Integration Principle

The repository separates the HAPS implementation from the OAI core:

- `src/` contains the HAPS-owned source and header files.
- `integration/` documents the changes required in existing OAI files.
- `examples/` contains example RFsimulator and HAPS configurations.
- OAI core files are not copied into this repository as independent source
  files.

This separation makes it clear which code belongs to the HAPS model and which
changes only connect the model to OAI.

## 2. HAPS Source Destination

Copy the HAPS modules from this repository into the compatible OAI source tree:

```text
oai-haps-channel-model/src/haps_*.c
oai-haps-channel-model/src/haps_*.h
                    ↓
openairinterface5g/radio/rfsimulator/haps/
```

Example:

```bash
mkdir -p ~/openairinterface5g/radio/rfsimulator/haps
cp -v src/haps_*.c ~/openairinterface5g/radio/rfsimulator/haps/
cp -v src/haps_*.h ~/openairinterface5g/radio/rfsimulator/haps/
```

Run these commands from the root of the `oai-haps-channel-model` repository.

## 3. OAI Integration Points

| OAI file or area | Required HAPS connection |
|---|---|
| `openair1/SIMULATION/TOOLS/sim.h` | Register the `HAPS_3GPP_ITU` channel type and the required channel state connection |
| `openair1/SIMULATION/TOOLS/random_channel.c` | Create the HAPS channel descriptor through OAI's channel-model creation path |
| `radio/rfsimulator/simulator.cpp` | Create and destroy the HAPS context, apply IQ processing, and transfer integer delay to `channel_offset` |
| RFsimulator CMake target | Compile the HAPS `.c` modules with RFsimulator |
| Channel configuration | Select `HAPS_3GPP_ITU` and associate UL/DL `model_name` values with `hapsmod` records |

Depending on the OAI revision, the channel name mapping and build target may be
located in additional files. Confirm each location against the tested OAI
commit before producing a patch.

## 4. Recommended Integration Order

### Step 1 — Copy the HAPS modules

Place all `haps_*.c` and `haps_*.h` files under:

```text
radio/rfsimulator/haps/
```

### Step 2 — Add the channel type

Register `HAPS_3GPP_ITU` in OAI's channel-model enumeration and name mapping.
This allows the configuration file to select the HAPS model.

### Step 3 — Add the creation path

Update `random_channel.c` so that selection of `HAPS_3GPP_ITU` creates the
single-tap identity/placeholder descriptor used by the current HAPS model.

The physical geometry and propagation losses are calculated by the HAPS
modules, not by this placeholder descriptor.

### Step 4 — Add the build sources

Add the following implementation files to the RFsimulator build target:

```text
haps/haps_channel.c
haps/haps_config.c
haps/haps_geometry.c
haps/haps_propagation.c
haps/haps_gas.c
haps/haps_rain.c
```

### Step 5 — Connect `simulator.cpp`

The RFsimulator receive path must perform the following operations for a HAPS
channel:

1. Create a separate `haps_channel_ctx_t` for each UL or DL link.
2. Pass the runtime carrier frequency and sample rate to the HAPS context.
3. Obtain the calculated delay through `haps_channel_get_delay_info()`.
4. Transfer the integer delay to OAI's `channel_offset` mechanism.
5. Apply IQ attenuation and optional fixed Doppler through
   `haps_channel_process()`.
6. Destroy the HAPS context when the RFsimulator link is closed.

The propagation delay is not applied inside the per-sample IQ multiplication
loop. The current implementation prepares an integer sample offset that is
handled by RFsimulator.

### Step 6 — Add the configuration

Select the HAPS channel type in the RFsimulator channel records and define the
matching entries in the `hapsmod` list.

The link names must match:

```text
rfsimu_channel_enB0   # Downlink
rfsimu_channel_ue0    # Uplink
```

If `model_name` and the corresponding `hapsmod` record do not match,
`haps_load_config()` cannot select the intended parameter set.

## 5. Integration Verification

After building OAI, verify the integration in this order:

1. Confirm that the build includes every HAPS implementation file.
2. Confirm that `HAPS_3GPP_ITU` is accepted by the channel configuration.
3. Confirm that separate UL and DL HAPS contexts are created.
4. Check for `HAPS ACTIVE` messages in the RFsimulator logs.
5. Check for `HAPS DELAY ACTIVE` when propagation delay is enabled.
6. Verify that the logged runtime frequency and sample rate are correct.
7. Compare the logged slant range, delay, FSPL, and integer sample count with
   independent calculations.

Useful source checks include:

```bash
rg -n "HAPS_3GPP_ITU|haps_channel" \
  ~/openairinterface5g/radio/rfsimulator \
  ~/openairinterface5g/openair1/SIMULATION/TOOLS
```

## 6. Patch Files for a Public Release

The first public release should pin one tested OAI commit and provide patches
generated against that exact revision. A suitable future structure is:

```text
integration/
├── README.md
└── patches/
    ├── 0001-register-haps-channel-type.patch
    ├── 0002-add-haps-rfsimulator-processing.patch
    └── 0003-add-haps-build-sources.patch
```

Do not publish patches generated from an unspecified or changing OAI branch.
Without a pinned base commit, the patch may fail or modify the wrong code.

## 7. Files That Belong Elsewhere

- HAPS implementation files belong in `src/`.
- Example `.conf` files belong in `examples/`.
- Reference measurements and validation outputs belong in `validation/`.
- User-facing explanations belong in `docs/`.
- OAI core modifications belong in revision-specific patch files under
  `integration/patches/`.
