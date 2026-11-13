# Author integration for OAI RFsimulator

The self-contained [v0.2.0 RFsim integration patch](patches/author_rfsim_v0.2.0_9313cbe.patch)
installs the HAPS channel with optional cloud attenuation, tropospheric
scintillation, and SISO NTN-TDL fast fading.

| Item | Value |
|---|---|
| OAI repository | [duranta-project/openairinterface5g](https://github.com/duranta-project/openairinterface5g) |
| OAI base commit | `9313cbea82bc3c6b8ff695dd808a0cde5f431724` |
| Original author checkout | `699a5453a0713b0978944e583357a7b1b6056591` |
| Patch and source checksums | [v0.2.0 manifest](patches/manifest.v0.2.0.json) |

## Apply to a separate worktree

Use an existing OAI Git checkout and a new destination directory. Run this
example from the root of the HAPS module repository; adjust the paths first.
The destination below is separate from an existing experiment worktree.

```bash
haps_module_dir="$(pwd)"
haps_oai_source="$HOME/openairinterface5g"
haps_oai_tree="$HOME/openairinterface5g_haps_reproduce_9313cbe"

git -C "$haps_oai_source" fetch --no-tags \
  https://github.com/duranta-project/openairinterface5g.git \
  9313cbea82bc3c6b8ff695dd808a0cde5f431724

git -C "$haps_oai_source" worktree add --detach "$haps_oai_tree" \
  9313cbea82bc3c6b8ff695dd808a0cde5f431724

git -C "$haps_oai_tree" apply --check --whitespace=nowarn \
  "$haps_module_dir/integration/patches/author_rfsim_v0.2.0_9313cbe.patch"

git -C "$haps_oai_tree" apply --whitespace=nowarn \
  "$haps_module_dir/integration/patches/author_rfsim_v0.2.0_9313cbe.patch"

python3 "$haps_module_dir/integration/verify_snapshot.py" --oai "$haps_oai_tree" \
  --manifest "$haps_module_dir/integration/patches/manifest.v0.2.0.json"
```

The patch installs the HAPS sources and changes to the pinned OAI tree.
The verifier checks the patch digest, pinned OAI HEAD, 25 recorded files
and four excluded zero-byte placeholders. See the historical section below
for the earlier snapshots.

## Build and configure

Install the dependencies required by the pinned OAI revision first. A direct
CMake/Ninja build for the RFsimulator, gNB and UE is:

```bash
cmake -S "$haps_oai_tree" \
  -B "$haps_oai_tree/cmake_targets/ran_build/build" -GNinja \
  -DOAI_SIMU=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TESTS=OFF

cmake --build "$haps_oai_tree/cmake_targets/ran_build/build" \
  --target rfsimulator nr-softmodem nr-uesoftmodem --parallel 2
```

The three outputs are `librfsimulator.so`, `nr-softmodem` and
`nr-uesoftmodem` in that build directory. Build flags, compiler and dependencies
can change binary hashes even when source hashes match. This build recipe reconstructs the full modem binaries.

Use the [matched configuration profiles](../examples/README.md) for the
selected elevation and experiment mode. They include the current UE
`pdu_sessions` syntax, consistent PLMN/slice settings, UE ECEF positions,
`do_SRS = "none"`, list-type `rfsimulator` sections and `min_rxtxtime = 6`.
Nominal SA and E9 use different channel flags. Keep the SIB1/SIB19 assistance
for ordinary SA; the profile generator documents the coreless E9 mode.
Open5GS, subscriber provisioning and UE network isolation for traffic
measurements are separate runtime setup steps.

Use the [atmosphere and TDL guide](../docs/ATMOSPHERE_AND_TDL.md) to enable the new effects.

## What the patch contains

| Area | Contents |
|---|---|
| `radio/rfsimulator/simulator.cpp` | HAPS context lifecycle, runtime frequency/sample-rate binding, SISO receive-path processing, geometric delay, TDL packet history and startup zero padding |
| `radio/rfsimulator/CMakeLists.txt` | The eight implemented HAPS C modules in the RFsimulator build |
| `radio/rfsimulator/haps/` | 18 files including cloud/scintillation, NTN-TDL, the shared RNG and the rain probe |
| `openair1/SIMULATION/TOOLS/{sim.h,random_channel.c}` | Channel-model registration for HAPS and the existing BASIC_STATIC/TWO_TAP_RAYLEIGH controls |
| `openair2/LAYER2/NR_MAC_UE/nr_ra_procedures.c` | Common SSB/RO assignment on the four-step CFRA path |
| `openair2/LAYER2/NR_MAC_gNB/nr_radio_config.c` | NTN access indication in SIB1 when NTN configuration is present |
| `openair2/LAYER2/NR_MAC_UE/nr_ue_procedures.c` | L1 SSB measurement logging used by the author runs |

The SISO HAPS branch reads from `timestamp - channel_offset` and retains
the additional samples needed by the longest TDL tap. The receive window is
zero-padded where the requested source timestamp precedes sample zero.
PSS/SSS detection comes from the pinned OAI base.

Default delay quantization uses `floor(sample_rate * geometric_delay + 0.5)`
with non-negative half ties upward: 4099/4730/8160/22525 samples at
90/60/30/10 degrees and 61.44 Msamples/s. `HAPS_DELAY_ROUNDING_CEIL` restores
the former 4099/4731/8160/22526 values. See the [delay guide](../docs/DELAY_QUANTIZATION.md). The pinned RFsimulator's final
channel-model IQ accumulation uses `lroundf` (nearest integer, half-way cases
away from zero) before storage in its 16-bit sample buffers. The author E8
campaign recorded gNB `TX_AMP = 519` LSB; this is a recorded run setting,
not a portable amplitude guarantee for other configurations.

## Validation and upstream status

The release checks covered patch application to `9313cbe`, all manifest
checksums, C/C++ compilation of the RFsim source files and standalone
regressions for attenuation, TDL and delay. Subsequent gNB–UE RFsim runs
exercised cloud attenuation, tropospheric scintillation and NTN-TDL-D in
coreless CFRA mode. See the [run configuration and measurements](../validation/README.md#gnbue-rfsim-cfra-measurements).

The author machine previously built the **original ceil integration** and executed:

- E8: 24 attenuation/noise-probe points, using PHY-test synchronization and
  SSB SINR observations.
- E9: 25 seeds for each of two environments and four elevations (200 real
  CFRA attempts), plus a forced-LoS control.
- SA: ordinary SA/Open5GS access and application measurements at 90, 60, 30
  and 10 degrees, with SIB19/NTN timing before the first PRACH.

These are the author reruns. Their sample sizes and measured values must
remain distinct from the paper's original reference dataset; this patch
publication does not establish numerical identity or edit the manuscript.
No new v0.1.2 full modem run or upstream CI result is claimed here. The prior
measurements remain identified with the ceil sources and their original hashes.

The two general OAI access fixes were submitted separately upstream:

- [PR #887: CFRA SSB/RO initialization](https://github.com/duranta-project/openairinterface5g/pull/887)
- [PR #888: SIB1 NTN access indication](https://github.com/duranta-project/openairinterface5g/pull/888)

Both fixes are already included in this pinned integration. Their upstream
submission is separate from acceptance/merge. The published snapshot targets
`9313cbe`; it is not a claim that the patch applies to a later `develop`.

## Historical integration snapshots

The [v0.1.2 nearest-delay patch](patches/author_rfsim_v0.1.2_9313cbe.patch)
and [v0.1.2 manifest](patches/manifest.json) reconstruct the 20-file baseline.
Run the verifier with that manifest for v0.1.2.

### Ceil comparison and exact historical artifacts

The [cleaned ceil comparison patch](patches/author_rfsim_9313cbe.patch),
SHA-256 `56f502bc3502cbfe0e89ea3f23521ab183e750c28dd07725b8d5dd5cd8d8b8e0`,
contains all 20 original nonempty source files, with their hashes unchanged.
Its [manifest](patches/manifest.author-reruns-ceil.json) additionally requires
the six excluded paths to be absent. Select this patch in the separate-worktree
example and verify with:

```bash
python3 "$haps_module_dir/integration/verify_snapshot.py" \
  --oai "$haps_oai_tree" \
  --manifest "$haps_module_dir/integration/patches/manifest.author-reruns-ceil.json"
```

For the **exact original 26-file artifact**, use the immutable
[original patch](https://github.com/MErenUlucinar/OAI-HAPS-Channel-Model/blob/ee1cea5a4afed9ef250a622ba43b1749019d5afa/integration/patches/author_rfsim_9313cbe.patch)
and [original manifest](https://github.com/MErenUlucinar/OAI-HAPS-Channel-Model/blob/ee1cea5a4afed9ef250a622ba43b1749019d5afa/integration/patches/manifest.json).
That patch's SHA-256 remains
`c4530b27af37a1f9b7c11f6caceb12c50f4831ad496bcb6861a4bee1e28b3515`.
The published v0.1.2 tag also remains unchanged; this main-branch packaging
cleanup does not move tags or rewrite prior experiment records. The v0.1.1
tag predates publication of the full integration.

## Upgrade to v0.2.0

Apply the full v0.2.0 patch in a fresh worktree at the pinned OAI commit,
then rebuild RFsim and use the desired `hapsmod` flags. This preserves the
experiment tree and its configuration for comparison.

### Maintenance patches for older integrations

If an older integration has the six zero-byte files, the
[removal-only patch](patches/remove_empty_placeholders.patch) deletes them:

```bash
git -C "$haps_oai_tree" apply --check \
  "$haps_module_dir/integration/patches/remove_empty_placeholders.patch"
git -C "$haps_oai_tree" apply \
  "$haps_module_dir/integration/patches/remove_empty_placeholders.patch"
```

Apply it only to the older tree where those files exist and are empty. It
rejects nonempty files. A fresh cleaned installation already omits them and
does not need this patch. Removing only these uncompiled files requires no
library rebuild.

For a ceil integration, separately apply the
[rounding update](patches/author_rfsim_delay_nearest.patch) to adopt the nearest
default. This changes real C code and therefore requires a rebuild/restart:

```bash
git -C "$haps_oai_tree" apply --check \
  "$haps_module_dir/integration/patches/author_rfsim_delay_nearest.patch"
git -C "$haps_oai_tree" apply \
  "$haps_module_dir/integration/patches/author_rfsim_delay_nearest.patch"
python3 "$haps_module_dir/integration/verify_snapshot.py" --oai "$haps_oai_tree"
cmake --build "$haps_oai_tree/cmake_targets/ran_build/build" \
  --target rfsimulator --parallel 2
```

Stop the modems before rebuilding, then restart both so they load the new
library. An existing nearest-mode tree needs only the removal patch and
verification; do not apply the rounding patch twice.
