#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs an isolated non-Telemetry ANGLE test.

The main contract is that the caller passes the arguments:

  --isolated-script-test-output=[FILENAME]
json is written to that file in the format:
https://chromium.googlesource.com/chromium/src/+/main/docs/testing/json_test_results_format.md

Optional argument:

  --isolated-script-test-filter=[TEST_NAMES]

is a double-colon-separated ("::") list of test names, to run just that subset
of tests. This list is parsed by this harness and sent down via the
--gtest_filter argument.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent non-python executable. For a similar script see
run_performance_test.py.
"""

import argparse
import json
import os
import pathlib
import shutil
import sys
import tempfile
import traceback


PY_UTILS = str(pathlib.Path(__file__).resolve().parents[1] / 'src' / 'tests' / 'py_utils')
if PY_UTILS not in sys.path:
    os.stat(PY_UTILS) and sys.path.insert(0, PY_UTILS)
import angle_path_util
import angle_test_util

angle_path_util.AddDepsDirToPath('testing/scripts')
import common
import xvfb
import test_env

# Unfortunately we need to copy these variables from
# //src/testing/scripts/test_env.py. Importing it and using its
# get_sandbox_env breaks test runs on Linux (it seems to unset DISPLAY).
CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', help='Test executable.')
    parser.add_argument('--isolated-script-test-output', type=str)
    parser.add_argument('--isolated-script-test-filter', type=str)
    parser.add_argument('--xvfb', help='Start xvfb.', action='store_true')

    # Kept for compatiblity.
    # TODO(jmadill): Remove when removed from the recipes. http://crbug.com/954415
    parser.add_argument('--isolated-script-test-perf-output', type=str)

    args, extra_flags = parser.parse_known_args()

    env = os.environ.copy()

    if angle_test_util.HasGtestShardsAndIndex(env):
        extra_flags += ('--shard-count=%d --shard-index=%d' %
                        angle_test_util.PopGtestShardsAndIndex(env)).split()

    if 'ISOLATED_OUTDIR' in env:
        extra_flags += ['--isolated-outdir=' + env['ISOLATED_OUTDIR']]
        env.pop('ISOLATED_OUTDIR')

    # Assume we want to set up the sandbox environment variables all the
    # time; doing so is harmless on non-Linux platforms and is needed
    # all the time on Linux.
    env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH

    rc = 0
    try:
        # Consider adding stdio control flags.
        if args.isolated_script_test_output:
            extra_flags.append('--isolated-script-test-output=%s' %
                               args.isolated_script_test_output)

        if args.isolated_script_test_filter:
            filter_list = common.extract_filter_list(args.isolated_script_test_filter)
            extra_flags.append('--gtest_filter=' + ':'.join(filter_list))

        with common.temporary_file() as tempfile_path:
            env['CHROME_HEADLESS'] = '1'
            cmd = [angle_test_util.ExecutablePathInCurrentDir(args.executable)] + extra_flags

            if args.xvfb:
                rc = xvfb.run_executable(cmd, env, stdoutfile=tempfile_path)
            else:
                rc = test_env.run_command_with_output(cmd, env=env, stdoutfile=tempfile_path)

    except Exception:
        traceback.print_exc()
        rc = 1

    return rc


if __name__ == '__main__':
    sys.exit(main())
