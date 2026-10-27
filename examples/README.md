# Matched HAPS configurations for OAI 9313cbe

These files target the tested OAI `develop` commit
`9313cbea82bc3c6b8ff695dd808a0cde5f431724`, not an arbitrary future `develop`.
The author's E8/E9/SA build includes the HAPS RFsim integration, RNG fix,
CFRA common-occasion assignment and the band-78 SIB1 NTN-access condition.
The last two are source prerequisites; configuration files cannot replace them.

## PLMN, subscriber and slice

The example gNB now broadcasts **PLMN 208/99**, matching the demo virtual-USIM
IMSI prefix. The UE uses the current syntax:

```text
pdu_sessions = ({ dnn = "oai"; nssai_sst = 1; });
```

The slice is SST 1 **without SD**. The AMF must serve the same PLMN/TAC/slice;
the core subscriber must have the matching virtual-USIM authentication values,
SST 1 and DNN `oai`. The distributed keys are public lab examples. Use only a
locally provisioned test subscriber; do not commit operational credentials.

Default local-core addresses match the author's validated SA setup:
AMF `127.0.0.5`, gNB NGAP/NGU `127.0.0.1`, GTP-U port 2152. Adapt these
addresses together for a remote core. RFsim uses port 4043 and a list-type
`rfsimulator` section in both gNB and UE. Keep `options = ("chanmod")`:
the channel-model records select `HAPS_3GPP_ITU`; the RFsim fallback field
`modelname = "AWGN"` is retained from the tested configuration.

## Profiles

| Profile | Delay | Clutter | Shadow fading | LoS | Offset |
|---|---:|---:|---:|---|---:|
| Nominal SA / `channelmod_haps_3gpp_itu.conf` | on | off | off | forced | 129.64 dB |
| Delay-off control / `channelmod_haps_3gpp_itu.delay-off.conf` | off | off | off | forced | 129.64 dB |
| E9 / `channelmod_haps_3gpp_itu.e9-delay-clutter-on.conf` | on | on | on | probabilistic | 112.08 dB |

FSPL is on, gas is on for DL only, and internal noise, Doppler, fast fading and
fractional delay are off in these profiles. Enabling clutter does not create
a nonzero clutter loss on forced-LoS links. The E9 file is the **default E9
channel input**, at zenith with seed 1; generate a matched triplet for each
scenario/elevation/seed using the command below. A file labelled delay-on now
actually enables propagation delay in both directions.

Matched nominal SA triplets:

| Elevation | gNB suffix | UE | Channel | UE longitude |
|---:|---|---|---|---:|
| 90 | `...rfsim.e90.conf` | `ue.haps.e90.conf` | `channelmod_haps_3gpp_itu.e90.conf` | 32.859700000 |
| 60 | `...rfsim.e60.conf` | `ue.haps.e60.conf` | `channelmod_haps_3gpp_itu.e60.conf` | 32.994297811 |
| 30 | `...rfsim.e30.conf` | `ue.haps.e30.conf` | `channelmod_haps_3gpp_itu.e30.conf` | 33.261831217 |
| 10 | `...rfsim.e10.conf` | `ue.haps.e10.conf` | `channelmod_haps_3gpp_itu.e10.conf` | 34.122126213 |

The full gNB prefix is `gnb.sa.band78.fr1.106PRB.haps.`. UE positions are
WGS-84 ECEF in metres, calculated from latitude 39.9334 degrees, the listed
longitude, and altitude 0 m. HAPS position is fixed at latitude 39.9334,
longitude 32.8597, altitude 20 km; SIB19 stores its ECEF components in 1.3 m
units. Do not combine an e60 UE with an e90 gNB/channel: both UL and DL channel
geometry must match the UE position used for NTN pre-compensation.

The existing `...rfsim.delay-on.conf` and `ue.haps.conf` names are matching
zenith aliases. Pair `...rfsim.delay-off.conf` with **`ue.haps.delay-off.conf`**;
both include the delay-off channel. No manual toggle of a shared file is needed.

## Generate private configurations

From the module repository root, for nominal SA at 60 degrees:

```bash
python3 examples/make_profile.py --profile sa --elevation 60 \
  --output /tmp/haps-sa-e60
```

For E9 at rural 10 degrees with a given seed:

```bash
python3 examples/make_profile.py --profile e9 --scenario rural --elevation 10 \
  --seed 25 --server-ip 10.201.1.100 --output /tmp/haps-e9-rural-e10-s25
```

The server IP must be the host address actually reachable from the UE's
namespace; 10.201.1.100 is an example, not an address the generator creates.
Each output must be a **new** directory. It contains `gnb.conf`, `ue.conf`,
`chan.conf` and non-secret profile metadata; original examples stay unchanged.
For a campaign, use seeds 1..S for each of two environments and four angles.
Seed mixing and eight warm-up outputs happen inside the module; do not add
external hashing. The generator does not launch modems or run a campaign.

## Radio modes and system information

Normal **SA** retains `cu_sibs = (2)` and `du_sibs = [19]`, needs a core,
and uses neither `--phy-test` nor `--do-ra`. Verify SIB19 and NTN timing before
first PRACH, normal CBRA, Registration Accept, PDU Session Accept and UE IP.

The generated **E9** profile is for `--do-ra` (CFRA without core). On the
pinned OAI revision this mode has no broadcast SIB1, so the generated gNB copy
uses `cu_sibs = ()`, `du_sibs = []`; NTN timing is supplied in the fresh
gNB-generated configuration. Do not carry this SI-list workaround into SA.
Do not preload old `reconfig.raw`/`rbconfig.raw` from an earlier mode or point.

`do_SRS = "none"` and `min_rxtxtime = 6` are set in every gNB profile. The
comment for `cellSpecificKoffset_r17 = 10` describes a scheduling offset rather
than the physical HAPS round-trip delay. At 30 kHz, OAI converts 10 reference
slots into 20 NR slots. The pinned L1 logger prints that shifted slot count
with an `ms` suffix; do not use the printed number as a propagation-delay
measurement or infer an exact additional IP RTT from it.

## Example launches

After configuring a matching core and preparing an SA triplet, use two
terminals from a fresh runtime directory containing the built libraries:

```bash
sudo ./nr-softmodem -O /tmp/haps-sa-e60/gnb.conf \
  --rfsim --gNBs.[0].min_rxtxtime 6
```

```bash
sudo ./nr-uesoftmodem -O /tmp/haps-sa-e60/ue.conf \
  --rfsim -r 106 --numerology 1 --band 78 -C 3619200000 --ssb 516
```

Those co-located commands are for attachment checks. For user-plane KPIs the
UE must run in an isolated namespace and ping/iperf must bind to its TUN/IP,
with the UPF target routed through that TUN. The raw commands above do not
constitute a throughput experiment. Adapt RFsim `serveraddr` to the host's
veth address for a namespaced UE. Use the validated campaign harness for
owned-PID/network cleanup and complete KPI collection.

For a generated E9 triplet, add `--do-ra` to **both** modem commands and omit
the core requirement. Count UE `RA procedure succeeded` (CFRA), not merely
PSS/PBCH synchronization. The reference observation is up to 26 s after UE
launch, following an 8 s gNB lead.

## Local checks

The optional checker requires Python `libconf` and exercises all static
profiles plus both environments and multiple generated seeds. Use a virtual
environment if needed:

```bash
python3 -m venv /tmp/haps-config-check
/tmp/haps-config-check/bin/pip install libconf==2.0.1
/tmp/haps-config-check/bin/python validation/check_configurations.py
```

Parsing and geometry checks do not replace a modem run. The author's separate
integration completed E8, E9 with 25 seeds/condition and SA at all four angles;
these release files are checked against the effective configurations of
those runs where applicable. This update does not claim another campaign
was executed after packaging, or change the manuscript's numeric results.
