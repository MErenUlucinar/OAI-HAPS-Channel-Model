# Configuration review fixes C1--C7

This change implements the configuration part of the supplied
`fixes/PR_DESCRIPTION.md`, against module repository main commit
`991a9587cda1049e9f707f89ab99c690b8338842`. OAI compatibility is pinned to
`9313cbea82bc3c6b8ff695dd808a0cde5f431724`.

| Review item | Implementation |
|---|---|
| C1 PLMN | All gNB examples use MCC 208, MNC 99, MNC length 2, matching the demo IMSI prefix. AMF/subscriber provisioning must match. |
| C2 current UE syntax | `uicc0.pdu_sessions = ({ dnn = "oai"; nssai_sst = 1; });` in every UE file. Legacy top-level DNN/slice assignments removed. |
| C3 slice SD | SST-only service, no `nssai_sd`; the gNB and example UE use SST 1. |
| C4 per-elevation position | Matched UE ECEF, UL/DL longitude and gNB includes for 90/60/30/10 degrees; profile generator selects all three together. |
| C5 delay/clutter | Nominal SA defaults to propagation delay on. Separate delay-off pair and dedicated E9 delay/clutter/shadow-fading-on profile; the E9 generator selects it automatically. |
| C6 K_offset | Comment now separates scheduling assistance from the 133--733 us geometric RTT. Value 10 retained, with `min_rxtxtime = 6`. No unsupported fixed IP-latency claim. |
| C7 OAI schema | String `do_SRS = "none"`, list-type `rfsimulator` for gNB and UE, sibling-relative includes. |

The E9-only generator removes extra broadcast SI lists from its private
coreless `--do-ra` copy, matching the pinned mode's SIB1 lifecycle. Static SA
examples retain SIB19 and require the existing gNB SIB1 NTN-access code fix.
The RNG and CFRA code fixes were already in the author's validated integration;
this change touches no module/OAI source or compiled binary.

Existing E8/E9/SA runtime templates and manifests on the author's machine are
not overwritten by this repository patch. Generate a separate profile for
new checks. Published results remain associated with their recorded sources,
binaries and effective run configurations.

Validation: libconfig-compatible parsing of all examples and generated
profiles, PLMN/slice/current-UE syntax, expected UL/DL flags, matched geometry,
propagation delays and relative include resolution. The generated nominal SA
profiles are compared against the uploaded four-angle SA campaign metadata.
No additional modem experiment is claimed by this configuration-only change.

See [examples/README.md](../examples/README.md) for commands and the explicit
SA versus E9 flag matrix. Publishing the author's full OAI integration patch,
updating manuscript measurements, and tagging a release are separate tasks.
