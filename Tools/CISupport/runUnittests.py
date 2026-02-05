#!/usr/bin/python3

# Copyright (C) 2018-2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import os
import shlex
import sys
from pathlib import Path


def main():
    print('runUnittests.py is deprecated!', file=sys.stderr)

    parser = argparse.ArgumentParser(
        description='Run Python unit tests for our various build masters',
    )
    parser.add_argument(
        'path',
        help='Relative path of directory to run Python unit tests in',
        nargs=1,
        type=Path,
    )
    parser.add_argument(
        '--autoinstall',
        help='Opt in to using autoinstall for buildbot packages',
        action='store_true',
    )

    args = parser.parse_args()
    self_path = Path(__file__).parent.resolve(strict=True)

    run_tests_args = []
    for path in args.path:
        if not path.is_dir():
            print(f'{path} is not a directory. Please specify a valid directory')
            sys.exit(1)

        run_tests_args.append(
            '.'.join(path.resolve(strict=True).relative_to(self_path).parts)
        )

    extra_env = {}
    if not args.autoinstall:
        extra_env['DISABLE_WEBKITCOREPY_AUTOINSTALLER'] = '1'

    new_args = [sys.executable, str(self_path / 'run-tests'), *run_tests_args]

    repro_args = [*(f'{k}={v}' for k, v in extra_env.items()), *(new_args)]

    print(f'Run instead: {shlex.join(repro_args)}\n\n', file=sys.stderr)
    os.execve(sys.executable, new_args, os.environ | extra_env)


if __name__ == '__main__':
    sys.exit(main())
