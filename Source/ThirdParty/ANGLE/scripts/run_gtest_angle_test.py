#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs an isolated non-Telemetry ANGLE test.

The main contract is that the caller passes the arguments:

  --isolated-script-test-output=[FILENAME]
json is written to that file in the format:
https://chromium.googlesource.com/chromium/src/+/master/docs/testing/json_test_results_format.md

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
import shutil
import sys
import tempfile
import traceback

# Add //src/testing into sys.path for importing xvfb and test_env, and
# //src/testing/scripts for importing common.
d = os.path.dirname
THIS_DIR = d(os.path.abspath(__file__))
CHROMIUM_SRC_DIR = d(d(d(THIS_DIR)))
sys.path.insert(0, os.path.join(CHROMIUM_SRC_DIR, 'testing'))
sys.path.insert(0, os.path.join(CHROMIUM_SRC_DIR, 'testing', 'scripts'))

import common
import xvfb
import test_env

# Unfortunately we need to copy these variables from
# //src/testing/scripts/test_env.py. Importing it and using its
# get_sandbox_env breaks test runs on Linux (it seems to unset DISPLAY).
CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'


def IsWindows():
    return sys.platform == 'cygwin' or sys.platform.startswith('win')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', help='Test executable.')
    parser.add_argument('--isolated-script-test-output', type=str, required=True)
    parser.add_argument('--isolated-script-test-perf-output', type=str, required=False)
    parser.add_argument('--isolated-script-test-filter', type=str, required=False)
    parser.add_argument('--xvfb', help='Start xvfb.', action='store_true')

    args, extra_flags = parser.parse_known_args()

    env = os.environ.copy()

    # total_shards = None
    # shard_index = None
    if 'GTEST_TOTAL_SHARDS' in env:
        extra_flags += ['--shard-count=%d' % env['GTEST_TOTAL_SHARDS']]
    if 'GTEST_SHARD_INDEX' in env:
        extra_flags += ['--shard-index=%d' % env['GTEST_SHARD_INDEX']]

    # Assume we want to set up the sandbox environment variables all the
    # time; doing so is harmless on non-Linux platforms and is needed
    # all the time on Linux.
    env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH

    rc = 0
    try:
        # Consider adding stdio control flags.
        if args.isolated_script_test_output:
            extra_flags.append('--results-file=%s' % args.isolated_script_test_output)

        if args.isolated_script_test_perf_output:
            extra_flags.append('--histogram-json-file=%s' % args.isolated_script_test_perf_output)

        if args.isolated_script_test_filter:
            filter_list = common.extract_filter_list(args.isolated_script_test_filter)
            extra_flags.append('--gtest_filter=' + ':'.join(filter_list))

        if IsWindows():
            args.executable = '.\\%s.exe' % args.executable
        else:
            args.executable = './%s' % args.executable
        with common.temporary_file() as tempfile_path:
            env['CHROME_HEADLESS'] = '1'
            cmd = [args.executable] + extra_flags

            if args.xvfb:
                rc = xvfb.run_executable(cmd, env, stdoutfile=tempfile_path)
            else:
                rc = test_env.run_command_with_output(cmd, env=env, stdoutfile=tempfile_path)

    except Exception:
        traceback.print_exc()
        rc = 1

    return rc


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
    json.dump([], args.output)


if __name__ == '__main__':
    # Conform minimally to the protocol defined by ScriptTest.
    if 'compile_targets' in sys.argv:
        funcs = {
            'run': None,
            'compile_targets': main_compile_targets,
        }
        sys.exit(common.run_script(sys.argv[1:], funcs))
    sys.exit(main())
