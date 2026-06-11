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
project_name = os.environ['PROJECT_NAME']


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


inputs = []
# Fall back to the Xcode-provided setting name for configurations where we do
# not pass the -fprofile-instr-use flag directly.
if os.environ.get('WK_ENABLE_PGO_USE',
                  os.environ.get('CLANG_USE_OPTIMIZATION_PROFILE')) == 'YES':
    for arch in archs.split():
        src = profiles_folder / f'{arch}/{project_name}.profdata.compressed'
        dst = output_folder / f'{arch}.profdata'
        checked_decompress(src, dst)
        inputs.append(src)

depfile_path.write_text(
    'dependencies: ' + '\\\n    '.join(
        shlex.quote(str(path)) for path in inputs
    )
)
