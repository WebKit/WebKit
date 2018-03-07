#!/usr/bin/env python

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# pylint: disable=invalid-name
"""
This script acts as an interface between the Chromium infrastructure and
gtest-parallel, renaming options and translating environment variables into
flags. Developers should execute gtest-parallel directly.

In particular, this translates the GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS
environment variables to the --shard_index and --shard_count flags, and renames
the --isolated-script-test-output flag to --dump_json_test_results.

All flags before '--' will be passed as arguments to gtest-parallel, and
(almost) all flags after '--' will be passed as arguments to the test
executable.
The exception is that --isolated-script-test-output and
--isolated-script-test-chartson-output are expected to be after '--', so they
are processed and removed from there.

If the --store-test-artifacts flag is set, an --output_dir must be also
specified.
The test artifacts will then be stored in a 'test_artifacts' subdirectory of the
output dir, and will be compressed into a zip file once the test finishes
executing.
This is useful when running the tests in swarming, since the output directory
is not known beforehand.

For example:

  gtest-parallel-wrapper.py some_test \
      --some_flag=some_value \
      --another_flag \
      --output_dir=SOME_OUTPUT_DIR
      -- \
      --store-test-artifacts
      --isolated-script-test-output=SOME_DIR \
      --isolated-script-test-perf-output=SOME_OTHER_DIR \
      --foo=bar \
      --baz

Will be converted into:

  python gtest-parallel some_test \
      --shard_count 1 \
      --shard_index 0 \
      --some_flag=some_value \
      --another_flag \
      --output_dir=SOME_OUTPUT_DIR \
      --dump_json_test_results=SOME_DIR \
      -- \
      --test_artifacts_dir=SOME_OUTPUT_DIR/test_artifacts \
      --foo=bar \
      --baz

"""

import argparse
import collections
import os
import shutil
import subprocess
import sys


Args = collections.namedtuple('Args',
                              ['gtest_parallel_args', 'test_env', 'output_dir',
                               'test_artifacts_dir'])


def _CatFiles(file_list, output_file):
  with open(output_file, 'w') as output_file:
    for filename in file_list:
      with open(filename) as input_file:
        output_file.write(input_file.read())
      os.remove(filename)


def _GetOutputDir(gtest_parallel_args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--output_dir', type=str, default=None)
  options, _ = parser.parse_known_args(gtest_parallel_args)
  return options.output_dir


def _ParseArgs():
  if '--' in sys.argv:
    argv_index = sys.argv.index('--')
  else:
    argv_index = len(sys.argv)

  gtest_parallel_args = sys.argv[1:argv_index]
  executable_args = sys.argv[argv_index + 1:]

  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str, default=None)

  # Needed when the test wants to store test artifacts, because it doesn't know
  # what will be the swarming output dir.
  parser.add_argument('--store-test-artifacts', action='store_true',
                      default=False)

  # We don't need to implement this flag, and possibly can't, since it's
  # intended for results of Telemetry tests. See
  # https://chromium.googlesource.com/external/github.com/catapult-project/catapult/+/HEAD/dashboard/docs/data-format.md
  parser.add_argument('--isolated-script-test-perf-output', type=str,
                      default=None)

  # No-sandbox is a Chromium-specific flag, ignore it.
  # TODO(oprypin): Remove (bugs.webrtc.org/8115)
  parser.add_argument('--no-sandbox', action='store_true', default=False)

  # We have to do this, since --isolated-script-test-output is passed as an
  # argument to the executable by the swarming scripts, and we want to pass it
  # to gtest-parallel instead.
  options, executable_args = parser.parse_known_args(executable_args)

  # --isolated-script-test-output is used to upload results to the flakiness
  # dashboard. This translation is made because gtest-parallel expects the flag
  # to be called --dump_json_test_results instead.
  if options.isolated_script_test_output:
    gtest_parallel_args += [
        '--dump_json_test_results',
        options.isolated_script_test_output,
    ]

  output_dir = _GetOutputDir(gtest_parallel_args)
  test_artifacts_dir = None

  if options.store_test_artifacts:
    assert output_dir, (
        '--output_dir must be specified for storing test artifacts.')
    test_artifacts_dir = os.path.join(output_dir, 'test_artifacts')
    if not os.path.isdir(test_artifacts_dir):
      os.makedirs(test_artifacts_dir)

    executable_args += [
        '--test_artifacts_dir',
        test_artifacts_dir,
    ]

  # GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS must be removed from the
  # environment. Otherwise it will be picked up by the binary, causing a bug
  # where only tests in the first shard are executed.
  test_env = os.environ.copy()
  gtest_shard_index = test_env.pop('GTEST_SHARD_INDEX', '0')
  gtest_total_shards = test_env.pop('GTEST_TOTAL_SHARDS', '1')

  gtest_parallel_args += [
      '--shard_count',
      gtest_total_shards,
      '--shard_index',
      gtest_shard_index,
  ] + ['--'] + executable_args

  return Args(gtest_parallel_args, test_env, output_dir, test_artifacts_dir)


def main():
  webrtc_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  gtest_parallel_path = os.path.join(
      webrtc_root, 'third_party', 'gtest-parallel', 'gtest-parallel')

  args = _ParseArgs()

  command = [
      sys.executable,
      gtest_parallel_path,
  ] + args.gtest_parallel_args

  print 'gtest-parallel-wrapper: Executing command %s' % ' '.join(command)
  sys.stdout.flush()

  exit_code = subprocess.call(command, env=args.test_env, cwd=os.getcwd())

  if args.output_dir:
    for test_status in 'passed', 'failed', 'interrupted':
      logs_dir = os.path.join(args.output_dir, 'gtest-parallel-logs',
                              test_status)
      if not os.path.isdir(logs_dir):
        continue
      logs = [os.path.join(logs_dir, log) for log in os.listdir(logs_dir)]
      log_file = os.path.join(args.output_dir, '%s-tests.log' % test_status)
      _CatFiles(logs, log_file)
      os.rmdir(logs_dir)

  if args.test_artifacts_dir:
    shutil.make_archive(args.test_artifacts_dir, 'zip', args.test_artifacts_dir)
    shutil.rmtree(args.test_artifacts_dir)

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
