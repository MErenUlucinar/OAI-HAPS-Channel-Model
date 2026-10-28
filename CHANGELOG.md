# Changelog

## Unreleased — packaging cleanup after v0.1.2

- Remove six zero-byte atmosphere/cloud/scintillation `.c`/`.h` entries from
  both full integration patches; retain all 20 nonempty files unchanged.
- Update manifests, checksums and integration documentation; require the six
  excluded paths to be absent when verifying a cleaned installation.
- Provide a removal-only patch for existing integrations. Published tags and
  original experiment artifacts remain available in immutable Git history.

## v0.1.2 — 2026-10-28

- Adopt review patch 0001's delay rule: `floor(tau*Fs + 0.5)` by default,
  with non-negative half ties upward. Keep `HAPS_DELAY_ROUNDING_CEIL` for comparisons.
- Apply the same change to the embedded author integration targeting OAI
  `9313cbea82bc3c6b8ff695dd808a0cde5f431724`; publish full and incremental patches.
- Preserve the frozen ceil integration, its manifest and original experimental
  records. The historical `reference_results.csv` is not rewritten.
- Add real C regression tests for both rounding modes, tie boundaries, the four
  geometry profiles and unchanged IQ gain; update documentation and profile checks.
- Retain the v0.1.1 RNG initialization fix. No new RFsim E8/E9/SA campaign is claimed.

See [delay quantization and validation](docs/DELAY_QUANTIZATION.md).
