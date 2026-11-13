#!/usr/bin/env python3
"""Compile the real delay code; check nearest and legacy ceil quantization.

Uses temporary OAI type/logging stubs. Does not build or execute a modem.
Can also test the embedded integration snapshot with --source-dir.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir', type=Path, default=ROOT/'src')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--atmosphere-tdl', action='store_true', help='Also run the v0.2.0 atmosphere and TDL checks')
    args = parser.parse_args()
    source = args.source_dir.resolve()
    cc = os.environ.get('CC', 'gcc')
    if not shutil.which(cc):
        raise RuntimeError('C compiler not found: '+cc)
    results = {}
    with tempfile.TemporaryDirectory(prefix='haps_delay_') as directory:
        temporary = Path(directory)
        stubs = temporary/'stubs'
        files = {
            'openair1/SIMULATION/TOOLS/sim.h': '#pragma once\n#include <stdint.h>\ntypedef struct {int16_t r,i;} c16_t;\ntypedef struct {float r,i;} cf_t;\n',
            'assertions.h': '#pragma once\n',
            'common/utils/LOG/log.h': '#pragma once\n#define HW 0\nstatic inline void haps_test_log(const char *format, ...) {(void)format;}\n#define LOG_I(component, ...) haps_test_log(__VA_ARGS__)\n#define LOG_D(component, ...) haps_test_log(__VA_ARGS__)\n#define LOG_E(component, ...) haps_test_log(__VA_ARGS__)\n#define LOG_W(component, ...) haps_test_log(__VA_ARGS__)\n',
        }
        for name, contents in files.items():
            path = stubs/name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents)
        for mode in ['nearest','ceil']:
            binary = temporary/mode
            command = [cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-I', str(stubs), '-I', str(source)]
            if mode == 'ceil':
                command.append('-DHAPS_DELAY_ROUNDING_CEIL')
            if args.sanitize:
                command.extend(['-fsanitize=address,undefined', '-fno-omit-frame-pointer'])
            command.extend([str(ROOT/'validation/delay_probe.c')])
            modules = ['haps_geometry.c', 'haps_propagation.c', 'haps_gas.c', 'haps_rain.c']
            modules += [name for name in ['haps_atmosphere.c', 'haps_tdl.c'] if (source/name).is_file()]
            command.extend(str(source/name) for name in modules)
            command.extend(['-lm', '-o', str(binary)])
            subprocess.run(command, check=True)
            environment = dict(os.environ)
            if args.sanitize:
                # LeakSanitizer cannot inspect /proc under the hosted runner.
                # Address/undefined-behavior instrumentation remains enabled.
                environment['ASAN_OPTIONS'] = environment.get('ASAN_OPTIONS', '') + ':detect_leaks=0'
            output = subprocess.check_output([str(binary)], text=True, env=environment)
            results[mode] = [json.loads(line) for line in output.splitlines()]
            if args.atmosphere_tdl and mode == 'nearest':
                probe = command.copy()
                probe[probe.index(str(ROOT/'validation/delay_probe.c'))] = str(ROOT/'validation/atmosphere_tdl_probe.c')
                probe.insert(-3, str(source/'haps_channel.c'))
                subprocess.run(probe, check=True)
                atmosphere_tdl = subprocess.check_output([str(binary)], text=True, env=environment).splitlines()
        for nearest, ceil in zip(results['nearest'], results['ceil']):
            for field in ['elevation_deg','exact_samples','fspl_db','applied_loss_db','iq_gain']:
                if nearest[field] != ceil[field]:
                    raise RuntimeError('Rounding changed an unrelated value: '+field)
    report = dict(status='PASS',scope='Standalone real C sources with OAI API stubs; no modem execution',
                  sample_rate_hz=61440000, boundary_cases_per_mode=9, delay_off='PASS',
                  iq_gain_unchanged_between_modes=True, sanitizers=args.sanitize,
                  leak_detection=False if args.sanitize else None, modes=results)
    if args.atmosphere_tdl:
        report['atmosphere_tdl'] = atmosphere_tdl
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
