# Author integration for OAI RFsimulator

The published, self-contained [author integration patch](patches/author_rfsim_9313cbe.patch)
includes the `simulator.cpp` connection and the HAPS source snapshot used in
our E8/E9/SA experiments. It is pinned to this exact OAI revision:

| Item | Value |
|---|---|
| OAI repository | [duranta-project/openairinterface5g](https://github.com/duranta-project/openairinterface5g) |
| OAI base commit | `9313cbea82bc3c6b8ff695dd808a0cde5f431724` |
| Original author checkout | `699a5453a0713b0978944e583357a7b1b6056591` |
| Patch | `patches/author_rfsim_9313cbe.patch` |
| Patch SHA-256 | `c4530b27af37a1f9b7c11f6caceb12c50f4831ad496bcb6861a4bee1e28b3515` |
| Recorded output files | 26; see [manifest.json](patches/manifest.json) |

The `699a5453` value identifies the source checkout from which the author's
integration was exported. Apply the published patch to **`9313cbe`**.
The file is the author's ported integration, not the review-side full-tree
patch. Its bytes and recorded source hashes are preserved from the tested
port package.

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
  "$haps_module_dir/integration/patches/author_rfsim_9313cbe.patch"

git -C "$haps_oai_tree" apply --whitespace=nowarn \
  "$haps_module_dir/integration/patches/author_rfsim_9313cbe.patch"

python3 "$haps_module_dir/integration/verify_snapshot.py" --oai "$haps_oai_tree"
```

The complete patch installs its own `radio/rfsimulator/haps/` files. Use that
embedded snapshot when reproducing the author runs; copying a later `src/`
version over it changes the source identity. The patch preserves historical
formatting, so `--whitespace=nowarn` replays it without reformatting its code.
The verifier checks the patch digest, the pinned OAI HEAD and all 26 recorded
output files. It does not build or execute OAI.

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
can change binary hashes even when source hashes match. This build recipe
is provided for reconstruction; no additional full build was performed for
this publication.

Use the [matched configuration profiles](../examples/README.md) for the
selected elevation and experiment mode. They include the current UE
`pdu_sessions` syntax, consistent PLMN/slice settings, UE ECEF positions,
`do_SRS = "none"`, list-type `rfsimulator` sections and `min_rxtxtime = 6`.
Nominal SA and E9 use different channel flags. Keep the SIB1/SIB19 assistance
for ordinary SA; the profile generator documents the coreless E9 mode.
Open5GS, subscriber provisioning and UE network isolation for traffic
measurements are separate runtime setup steps.

## What the patch contains

| Area | Contents |
|---|---|
| `radio/rfsimulator/simulator.cpp` | HAPS context lifecycle, runtime frequency/sample-rate binding, SISO receive-path processing, delayed history reads and startup zero IQ |
| `radio/rfsimulator/CMakeLists.txt` | The six implemented HAPS C modules in the RFsimulator build |
| `radio/rfsimulator/haps/` | 19 author files, including the RNG mix/eight-output warm-up, a rain probe and empty historical placeholders |
| `openair1/SIMULATION/TOOLS/{sim.h,random_channel.c}` | Channel-model registration for HAPS and the existing BASIC_STATIC/TWO_TAP_RAYLEIGH controls |
| `openair2/LAYER2/NR_MAC_UE/nr_ra_procedures.c` | Common SSB/RO assignment on the four-step CFRA path |
| `openair2/LAYER2/NR_MAC_gNB/nr_radio_config.c` | NTN access indication in SIB1 when NTN configuration is present |
| `openair2/LAYER2/NR_MAC_UE/nr_ue_procedures.c` | L1 SSB measurement logging used by the author runs |

The SISO HAPS branch processes each link once. For integer delay, RFsimulator
reads input history at `timestamp - channel_offset`; insufficient startup
history produces zero IQ. PSS/SSS detection code is inherited from the pinned
OAI base. Only the six implemented HAPS modules are built; empty atmosphere,
cloud and scintillation files do not implement those effects.

Delay quantization uses `ceil(sample_rate * geometric_delay)`: at 20 km and
61.44 Msamples/s this is 4099 samples. The pinned RFsimulator's final
channel-model IQ accumulation uses `lroundf` (nearest integer, half-way cases
away from zero) before storage in its 16-bit sample buffers. The author E8
campaign recorded gNB `TX_AMP = 519` LSB; this is a recorded run setting,
not a portable amplitude guarantee for other configurations.

## Validation and upstream status

Publication checks applied the unchanged patch to a fresh full checkout of
`9313cbe`, verified seven modified upstream pre-image Git blobs, and matched
all 26 post-image SHA-256 values in [the manifest](patches/manifest.json).
Earlier standalone checks exercised the six real HAPS C sources and the
RFsim bridge branch with narrow OAI API stubs. They do not replace a full
modem build or network experiment.

The author machine subsequently built this pinned integration and executed:

- E8: 24 attenuation/noise-probe points, using PHY-test synchronization and
  SSB SINR observations.
- E9: 25 seeds for each of two environments and four elevations (200 real
  CFRA attempts), plus a forced-LoS control.
- SA: ordinary SA/Open5GS access and application measurements at 90, 60, 30
  and 10 degrees, with SIB19/NTN timing before the first PRACH.

These are the author reruns. Their sample sizes and measured values must
remain distinct from the paper's original reference dataset; this patch
publication does not establish numerical identity or edit the manuscript.
No new full modem run or upstream CI result is claimed here.

The two general OAI access fixes were submitted separately upstream:

- [PR #887: CFRA SSB/RO initialization](https://github.com/duranta-project/openairinterface5g/pull/887)
- [PR #888: SIB1 NTN access indication](https://github.com/duranta-project/openairinterface5g/pull/888)

Both fixes are already included in this pinned integration. Their upstream
submission is separate from acceptance/merge. The published snapshot targets
`9313cbe`; it is not a claim that the patch applies to a later `develop`.
