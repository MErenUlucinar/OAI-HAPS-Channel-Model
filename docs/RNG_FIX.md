# RNG initialization fix and clutter-setting clarification

## Change

The v0.1.1 development sources incorporate the review's
`0002-rng-seed-mix-warmup.patch` into `src/haps_propagation.c`.
The former implementation used the raw user seed as the initial xorshift32
state and immediately used its first output for probabilistic LoS selection.
Small consecutive seeds therefore produced correlated first draws. At
dense-urban 60 degrees, seeds 1..100 yielded 100 LoS realizations even though
the model probability is 0.612.

The fix applies a 32-bit seed finalizer using Murmur3 fmix32 constants and
discards eight xorshift32 outputs before drawing the LoS state and shadow
fading. The seed-zero alias of seed one is preserved, and a nonzero fallback
state is retained. The unsupported-effects error message is also corrected
to refer only to cloud and scintillation, matching its condition.

For a fixed configuration and seed, realizations remain repeatable within
the new implementation. The same numeric seed generally produces a different
realization than in the previous implementation, including in forced modes
when shadow fading is enabled. Do not externally hash the seeds again when
repeating the review's final E9 campaign.

## Standalone validation

From a checkout with Python 3 and gcc installed, run:

```bash
python3 validation/test_rng.py
```

The check compiles the actual propagation and gas sources with a temporary
logging stub; no OAI installation or third-party Python dependency is needed.
It checks 10,000 seeds in each of eight dense-urban/rural elevation cells,
same-seed replay, seed 0/1/UINT32_MAX, standardised shadow fading, and forced
LoS/NLoS clutter switching. The loose six-sigma frequency bound is a regression
check, not a general PRNG certification. Dense-urban 60 degrees now gives
61 LoS and 39 NLoS realizations for seeds 1..100.

Before publication, the prepared-source validation also compared all 800
LoS/draw/SF/clutter realizations with the supplied ICC2027 review package's
E9 extracts; they agree within the precision printed in those extracts.
That comparison does not repeat the 800 RFsim runs or validate attach.

On 26 October 2026, the author reported a successful rebuild of
`librfsimulator.so` in the existing OAI working tree. The corrected propagation
source matches that working source byte for byte, with SHA-256:

```text
1d2211d67b81c16e8e41b2900e9cd6221fa7680edf9405216b4211450fb3afbd
```

The author-reported built-library SHA-256 is:

```text
041511c281bef80d8ac934e2f59af17abb3b4a780333e62b17750a40ac6b929f
```

The library hash identifies that host build, not a portable binary release.
It is not a substitute for publishing the complete integration and OAI base
commit. No new RFsim E8/E9/SA campaign is claimed by this update.

## E2/E3 clutter setting

The author confirmed on 26 October 2026 that `enable_clutter = 1` was used
for the NLoS cases of E2 and E3 and that the configuration was changed between
experimental cases. This clarification resolves the remaining clutter-setting
question in reviewer item M5. It is based on author confirmation, not an
independent reconstruction of the original run logs.

The general example file is a baseline configuration and does not represent
every experiment. Its flags have not been silently changed by this RNG fix.
LoS clutter is zero regardless of the flag; NLoS clutter is added only when
enabled. No setting for other cases or every DL/UL record is inferred from
the NLoS E2/E3 clarification.

## Integration and versioning

After updating the module checkout, update the source copy actually compiled
by the OAI integration, rebuild `librfsimulator.so`, and restart the relevant
gNB/UE processes. Merely editing the module checkout does not change an
already loaded RFsim library. Preserve the OAI integration patch, base commit,
compiler/build information and per-run configuration with the experiment.

Version the corrected sources with a new tag; do not move an existing version
tag. The README identifies development version v0.1.1. A GitHub release is a
separate publication step and should target the corrected source commit.
