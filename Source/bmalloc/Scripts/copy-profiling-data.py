#!/usr/bin/python3

import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

profiles_folder = Path(os.environ['SCRIPT_INPUT_FILE_0'])
empty_profdata = Path(os.environ['SCRIPT_INPUT_FILE_1'])
output_folder = Path('{BUILT_PRODUCTS_DIR}/DerivedSources/{PROJECT_NAME}/Profiling'.format_map(os.environ))
depfile_path = Path('{DERIVED_FILES_DIR}/copy-profiling-data.d'.format_map(os.environ))

# Some 26.x versions of Xcode do not expose ARCHS_BASE, but can safely fall
# back to ARCHS.
archs = os.environ.get('ARCHS_BASE', os.environ['ARCHS'])
# When a target is statically linked into another project's dylib (e.g. WTF
# into JavaScriptCore.framework), its counters land in that dylib's profdata.
# WK_PGO_SOURCE_PROJECT_NAME lets the consuming target name the source
# profile, while still writing into its own DerivedSources output.
source_project_name = os.environ.get('WK_PGO_SOURCE_PROJECT_NAME') or os.environ['PROJECT_NAME']

# When an upstream-in-build-order target has already decompressed the same
# profile, symlink to its output instead of decompressing again. Falls back
# to a fresh decompression if the upstream output is missing (clean build
# before the upstream's phase has run, or production where the upstream
# project's byproducts are not available).
upstream_target = os.environ.get('WK_PGO_REUSE_FROM_TARGET')


def checked_decompress(src, dst):
    if src.stat().st_size < 1024:
        if os.environ['CONFIGURATION'] == 'Production':
            raise SystemExit(
                f'error: {src} is <1KB, is it a Git LFS stub? '
                'Ensure this file was checked out on a machine with git-lfs installed.'
            )
        else:
            print(
                f'warning: {src} is <1KB, is it a Git LFS stub? '
                'To build with production optimizations, ensure this file was '
                'checked out on a machine with git-lfs installed. Falling '
                'back to stub profile data.',
                file=sys.stderr
            )
            shutil.copy(src, dst)
    else:
        subprocess.check_call(
            ('compression_tool', '-v', '-decode', '-i', src, '-o', dst)
        )


def upstream_profdata(arch):
    if not upstream_target:
        return None
    candidate = Path(
        '{BUILT_PRODUCTS_DIR}/DerivedSources/'.format_map(os.environ)
    ) / upstream_target / 'Profiling' / f'{arch}.profdata'
    return candidate if candidate.exists() else None


inputs = []
# Fall back to the Xcode-provided setting name for configurations where we do
# not pass the -fprofile-instr-use flag directly.
if os.environ.get('WK_ENABLE_PGO_USE',
                  os.environ.get('CLANG_USE_OPTIMIZATION_PROFILE')) == 'YES':
    for arch in archs.split():
        dst = output_folder / f'{arch}.profdata'
        upstream = upstream_profdata(arch)
        if upstream is not None:
            try:
                os.unlink(dst)
            except FileNotFoundError:
                pass
            os.symlink(upstream, dst)
            inputs.append(upstream)
            continue

        src = profiles_folder / f'{arch}/{source_project_name}.profdata.compressed'
        checked_decompress(src, dst)
        inputs.append(src)

depfile_path.write_text(
    'dependencies: ' + '\\\n    '.join(
        shlex.quote(str(path)) for path in inputs
    )
)
