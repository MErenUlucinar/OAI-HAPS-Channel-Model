#!/usr/bin/env python3
"""Standalone C regression checks for the module's LoS/SF initialization.

Run from any directory: python3 validation/test_rng.py
Requires Python 3 and gcc (or CC pointing to a compiler executable).
No OAI installation is required. This does not run RFsim or measure attach.
"""
import csv
import io
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLOS = {
    "dense_urban": {10: .282, 30: .398, 60: .612, 90: .981},
    "rural": {10: .782, 30: .919, 60: .940, 90: .998},
}
LOG_STUB = r'''#pragma once
#include <stdio.h>
#include <stdlib.h>
#define HW 0
#define LOG_I(c, ...) do { if (getenv("HAPS_VERBOSE")) fprintf(stderr, __VA_ARGS__); } while (0)
#define LOG_W(c, ...) fprintf(stderr, __VA_ARGS__)
#define LOG_E(c, ...) fprintf(stderr, __VA_ARGS__)
#define LOG_D(c, ...) do {} while (0)
'''


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def probe(binary, scenario, elevation, mode="prob", clutter=1, count=100, first=1):
    env = dict(os.environ)
    env.pop("HAPS_VERBOSE", None)
    completed = subprocess.run([
        str(binary), scenario, str(elevation), mode, str(clutter), "1", str(count), str(first),
    ], check=True, text=True, capture_output=True, env=env)
    rows = [{k: float(v) for k, v in row.items()}
            for row in csv.DictReader(io.StringIO(completed.stdout))]
    require(len(rows) == count, "Probe did not return every seed")
    return rows


def main():
    cc = os.environ.get("CC", "gcc")
    require(shutil.which(cc) is not None, f"C compiler not found: {cc}")
    cells = []
    with tempfile.TemporaryDirectory(prefix="haps_rng_") as temporary:
        temporary = Path(temporary)
        header = temporary / "stubs/common/utils/LOG/log.h"
        header.parent.mkdir(parents=True)
        header.write_text(LOG_STUB)
        binary = temporary / "rng_probe"
        subprocess.run([
            cc, "-std=c11", "-O2", "-Wall", "-Wextra",
            "-I", str(ROOT / "src"), "-I", str(temporary / "stubs"),
            str(ROOT / "validation/rng_probe.c"),
            str(ROOT / "src/haps_propagation.c"), str(ROOT / "src/haps_gas.c"),
            "-lm", "-o", str(binary),
        ], check=True)
        small = probe(binary, "dense_urban", 60)
        # Independent regression datum: the package's E9 seeds 1..100 contain
        # 61 LoS and 39 NLoS realizations at dense-urban 60 degrees.
        require(sum(int(r["los"]) for r in small) == 61,
                "Small-seed regression: expected 61 LoS realizations at dense-urban 60 degrees")
        require(small == probe(binary, "dense_urban", 60), "Same-seed replay failed")
        for scenario, angles in PLOS.items():
            for elevation, expected in angles.items():
                rows = probe(binary, scenario, elevation, count=10000)
                observed = sum(int(r["los"]) for r in rows) / len(rows)
                bound = 6 * math.sqrt(expected * (1 - expected) / len(rows)) + 1 / len(rows)
                require(abs(observed - expected) <= bound,
                        f"LoS frequency differs at {scenario}/{elevation}")
                for r in rows:
                    require(abs(r["plos"] - expected) < 1e-12, "Model pLoS changed")
                    require(0 < r["draw"] < 1, "Draw outside open interval")
                    require(bool(r["los"]) == (r["draw"] < r["plos"]), "LoS decision differs")
                standardised = [r["sf"] / r["sigma_sf"] for r in rows]
                require(abs(statistics.mean(standardised)) < .06, "SF mean regression")
                require(.90 < statistics.pvariance(standardised) < 1.10, "SF variance regression")
                cells.append(dict(scenario=scenario, elevation=elevation, n=10000,
                                  expected_plos=expected, observed_plos=observed))
        for seed in (0, 1, 0xFFFFFFFF):
            a = probe(binary, "dense_urban", 60, count=1, first=seed)
            require(a == probe(binary, "dense_urban", 60, count=1, first=seed), "Edge-seed replay failed")
        zero = probe(binary, "dense_urban", 60, count=1, first=0)[0]
        one = probe(binary, "dense_urban", 60, count=1, first=1)[0]
        require(all(zero[k] == one[k] for k in zero if k != "seed"), "Seed-zero alias changed")
        for scenario, expected in (("rural", 16.3), ("dense_urban", 25.5)):
            on = probe(binary, scenario, 90, mode="nlos", clutter=1, count=10)
            off = probe(binary, scenario, 90, mode="nlos", clutter=0, count=10)
            los = probe(binary, scenario, 90, mode="los", clutter=1, count=10)
            for a, b, c in zip(on, off, los):
                require(abs(a["cl"] - expected) < 1e-12 and b["cl"] == c["cl"] == 0,
                        "Clutter flag regression")
                require(a["sf"] == b["sf"], "Clutter flag changed SF")
                require(abs(a["total"] - b["total"] - expected) < 1e-10,
                        "Clutter contribution missing from total loss")
    print(json.dumps(dict(status="PASS", scope="Standalone C module; not RFsim",
                          dense_urban_60_los_first_100=61, cells=cells,
                          replay="PASS", edge_seeds="PASS", clutter="PASS"), indent=2))


if __name__ == "__main__":
    main()
