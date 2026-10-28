#!/usr/bin/env python3
"""Verify the pinned integration patch and its 26 recorded source files."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--oai', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, help='Optional manifest, e.g. the frozen author-reruns-ceil manifest')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    manifest_path = args.manifest or here / 'patches/manifest.json'
    manifest = json.loads(manifest_path.read_text())
    oai = args.oai.expanduser().resolve()
    try:
        if sha256(here / 'patches' / manifest['patch']) != manifest['patch_sha256']:
            raise RuntimeError('Published patch checksum mismatch')
        head = subprocess.check_output(['git', '-C', str(oai), 'rev-parse', 'HEAD'], text=True).strip()
        if head != manifest['oai_commit']:
            raise RuntimeError('Expected OAI HEAD ' + manifest['oai_commit'] + ', found ' + head)
        for relative, expected in manifest['files_sha256'].items():
            path = oai / relative
            if path.is_symlink() or not path.is_file() or sha256(path) != expected:
                raise RuntimeError('Source snapshot mismatch: ' + relative)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print('Snapshot verification failed:', error, file=sys.stderr)
        return 1
    print('OAI base:', head)
    print('Patch SHA-256:', manifest['patch_sha256'])
    print('Verified', len(manifest['files_sha256']), 'recorded source files.')
    print('This checks source identity; it does not build or run the modems.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
