# Delay quantization in v0.1.2

## Adopted review fix

Version v0.1.2 adopts the delay part of review patch
`0001-review-fixes-delay-rounding-configs.patch`. The supplied reference patch
has SHA-256 `2aae5a5c18a5be5b966aacdd79c5deb2990d1ad3617774b631bf2cca1b6ac121`.
Only its `src/haps_channel.c` rounding hunk is applied here; its older
configuration changes are not reapplied over the already corrected profiles.
The explanatory comment avoids assuming a zero-mean error for every fixed
geometry and states the actual bounded error and tie convention.

For non-negative geometric delay `tau`, the default is:

```math
N=\lfloor\tau F_s+\tfrac12\rfloor
```

Half-sample ties round upward. This matches the nearest rule in manuscript
Eq. (8). The maximum absolute one-way quantization error is half a sample,
8.138 ns at 61.44 Msps. Negative quantization errors are valid and mean the
integer delay is slightly shorter than the continuous geometric delay.

Define `HAPS_DELAY_ROUNDING_CEIL` when compiling the HAPS C sources to select
the previous `ceil(tau*Fs)` behavior. This switch is for comparison; the
v0.1.2 default requires no extra build definition. I/Q value quantization
using the pinned RFsimulator's `lroundf` is a separate operation and is unchanged.

## Four elevation profiles

These are standalone outputs of the actual C channel/geometry/propagation
sources at 20 km HAPS altitude, the released UE coordinates, and 61.44 Msps.

| Elevation | Exact samples | v0.1.1 ceil | v0.1.2 nearest | Nearest applied delay (us) | One-way error (ns) | Two-leg error (samples) |
|---:|---:|---:|---:|---:|---:|---:|
| 90 deg | 4098.835602 | 4099 | 4099 | 66.715495 | +2.676 | +0.328796 |
| 60 deg | 4730.467840 | 4731 | 4730 | 76.985677 | -7.615 | -0.935681 |
| 30 deg | 8159.641354 | 8160 | 8160 | 132.812500 | +5.837 | +0.717292 |
| 10 deg | 22525.095525 | 22526 | 22525 | 366.617839 | -1.555 | -0.191050 |

The two-leg column is `2*(N-tau*Fs)` for identical UL and DL geometry. Its
magnitude is below one sample in these profiles. It is the **channel-delay
quantization contribution**, not an independently measured complete UE TA
residual. Position encoding, timing assistance and other receiver timing
terms must not be inferred from this column alone.

## Module and integration must agree

Both `src/haps_channel.c` and the embedded source in the new full integration
patch use the nearest default and the optional legacy macro. The default
manifest points to
[`author_rfsim_v0.1.2_9313cbe.patch`](../integration/patches/author_rfsim_v0.1.2_9313cbe.patch).
The OAI base remains `9313cbea82bc3c6b8ff695dd808a0cde5f431724`.
Of the 20 nonempty embedded files, only `radio/rfsimulator/haps/haps_channel.c`
changes relative to the ceil comparison. Six zero-byte file creations have
been removed from both full patches as a subsequent packaging cleanup. The full patch's SHA-256 and
all source hashes are in [the current manifest](../integration/patches/manifest.json).

For an already matching author integration, the
[incremental rounding patch](../integration/patches/author_rfsim_delay_nearest.patch)
changes only that block. The [integration guide](../integration/README.md)
includes verification and rebuild instructions. Rebuild `librfsimulator.so`
and restart the modems; updating repository text does not replace an already
loaded shared library on the author's computer.

## Preserve the original experiment record

The completed author E8/E9/SA reruns used **ceil**. Their raw logs, binary
hashes and measured KPI records are not changed or relabeled by this release.
The current [ceil comparison patch](../integration/patches/author_rfsim_9313cbe.patch)
retains all 20 original nonempty file hashes and omits the six zero-byte files.
Its [manifest](../integration/patches/manifest.author-reruns-ceil.json) verifies
that cleaned tree. The exact original 26-file patch remains in
[immutable commit history](https://github.com/MErenUlucinar/OAI-HAPS-Channel-Model/blob/ee1cea5a4afed9ef250a622ba43b1749019d5afa/integration/patches/author_rfsim_9313cbe.patch),
with SHA-256 `c4530b27af37a1f9b7c11f6caceb12c50f4831ad496bcb6861a4bee1e28b3515`.
The [integration guide](../integration/README.md) links the original manifest
and explains the removal-only update for older worktrees. Published version
tags have not been moved.
The original `validation/reference_results.csv` also stays unchanged and is
explicitly historical ceil data.

The new rule changes each affected leg by one sample (16.276 ns at this
sample rate). FSPL, propagation losses, RNG code and IQ gain do not change.
This makes no claim that a new v0.1.2 E8/E9/SA modem campaign has been run or
that application KPIs were independently remeasured with this version.

## Validation

```bash
python3 validation/test_delay.py --sanitize
python3 validation/test_rng.py
python3 validation/check_configurations.py
```

`test_delay.py` compiles the actual C implementation with temporary OAI
type/logging stubs. It checks nine binary-exact boundary cases in each mode,
including half ties, sub-sample delay and delay-off, and the four released
geometry profiles. It checks the error bound and verifies that switching
rounding mode leaves loss/gain and block IQ amplitude unchanged. It can also
test the actual embedded integration sources with `--source-dir`.

Address and undefined-behavior sanitizer instrumentation passed; leak
detection is disabled because the hosted runner cannot inspect the required
`/proc` task information. The module results are recorded in
[`delay_rounding_results.json`](../validation/delay_rounding_results.json).
Full-patch checks apply the patch to a fresh pinned OAI checkout and verify
all 20 nonempty output hashes and the absence of the six excluded paths. These checks are not a full OAI build or modem run.
