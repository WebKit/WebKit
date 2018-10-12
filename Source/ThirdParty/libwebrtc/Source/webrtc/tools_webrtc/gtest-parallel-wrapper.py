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
environment variables to the --shard_index and --shard_count flags, renames
the --isolated-script-test-output flag to --dump_json_test_results,
and interprets e.g. --workers=2x as 2 workers per core.

Flags before '--' will be attempted to be understood as arguments to
gtest-parallel. If gtest-parallel doesn't recognize the flag or the flag is
after '--', the flag will be passed on to the test executable.

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
      --output_dir=SOME_OUTPUT_DIR \
      --store-test-artifacts
      --isolated-script-test-output=SOME_DIR \
      --isolated-script-test-perf-output=SOME_OTHER_DIR \
      -- \
      --foo=bar \
      --baz

Will be converted into:

  python gtest-parallel \
      --shard_index 0 \
      --shard_count 1 \
      --output_dir=SOME_OUTPUT_DIR \
      --dump_json_test_results=SOME_DIR \
      some_test \
      -- \
      --test_artifacts_dir=SOME_OUTPUT_DIR/test_artifacts \
      --some_flag=some_value \
      --another_flag \
      --isolated-script-test-perf-output=SOME_OTHER_DIR \
      --foo=bar \
      --baz

"""

import argparse
import collections
import multiprocessing
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

def _ParseWorkersOption(workers):
  """Interpret Nx syntax as N * cpu_count. Int value is left as is."""
  base = float(workers.rstrip('x'))
  if workers.endswith('x'):
    result = int(base * multiprocessing.cpu_count())
  else:
    result = int(base)
  return max(result, 1)  # Sanitize when using e.g. '0.5x'.


class ReconstructibleArgumentGroup(object):
  """An argument group that can be converted back into a command line.

  This acts like ArgumentParser.add_argument_group, but names of arguments added
  to it are also kept in a list, so that parsed options from
  ArgumentParser.parse_args can be reconstructed back into a command line (list
  of args) based on the list of wanted keys."""
  def __init__(self, parser, *args, **kwargs):
    self._group = parser.add_argument_group(*args, **kwargs)
    self._keys = []

  def AddArgument(self, *args, **kwargs):
    arg = self._group.add_argument(*args, **kwargs)
    self._keys.append(arg.dest)

  def RemakeCommandLine(self, options):
    result = []
    for key in self._keys:
      value = getattr(options, key)
      if value is True:
        result.append('--%s' % key)
      elif value is not None:
        result.append('--%s=%s' % (key, value))
    return result


def ParseArgs(argv=None):
  parser = argparse.ArgumentParser(argv)

  gtest_group = ReconstructibleArgumentGroup(parser,
                                             'Arguments to gtest-parallel')
  # These options will be passed unchanged to gtest-parallel.
  gtest_group.AddArgument('-d', '--output_dir')
  gtest_group.AddArgument('-r', '--repeat')
  gtest_group.AddArgument('--retry_failed')
  gtest_group.AddArgument('--gtest_color')
  gtest_group.AddArgument('--gtest_filter')
  gtest_group.AddArgument('--gtest_also_run_disabled_tests',
                          action='store_true', default=None)
  gtest_group.AddArgument('--timeout')

  # Syntax 'Nx' will be interpreted as N * number of cpu cores.
  gtest_group.AddArgument('-w', '--workers', type=_ParseWorkersOption)

  # --isolated-script-test-output is used to upload results to the flakiness
  # dashboard. This translation is made because gtest-parallel expects the flag
  # to be called --dump_json_test_results instead.
  gtest_group.AddArgument('--isolated-script-test-output',
                          dest='dump_json_test_results')

  # Needed when the test wants to store test artifacts, because it doesn't know
  # what will be the swarming output dir.
  parser.add_argument('--store-test-artifacts', action='store_true')

  # No-sandbox is a Chromium-specific flag, ignore it.
  # TODO(oprypin): Remove (bugs.webrtc.org/8115)
  parser.add_argument('--no-sandbox', action='store_true',
                      help=argparse.SUPPRESS)

  parser.add_argument('executable')
  parser.add_argument('executable_args', nargs='*')

  options, unrecognized_args = parser.parse_known_args(argv)

  executable_args = options.executable_args + unrecognized_args

  if options.store_test_artifacts:
    assert options.output_dir, (
        '--output_dir must be specified for storing test artifacts.')
    test_artifacts_dir = os.path.join(options.output_dir, 'test_artifacts')

    executable_args.insert(0, '--test_artifacts_dir=%s' % test_artifacts_dir)
  else:
    test_artifacts_dir = None

  gtest_parallel_args = gtest_group.RemakeCommandLine(options)

  # GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS must be removed from the
  # environment. Otherwise it will be picked up by the binary, causing a bug
  # where only tests in the first shard are executed.
  test_env = os.environ.copy()
  gtest_shard_index = test_env.pop('GTEST_SHARD_INDEX', '0')
  gtest_total_shards = test_env.pop('GTEST_TOTAL_SHARDS', '1')

  gtest_parallel_args.insert(0, '--shard_index=%s' % gtest_shard_index)
  gtest_parallel_args.insert(1, '--shard_count=%s' % gtest_total_shards)

  gtest_parallel_args.append(options.executable)
  if executable_args:
    gtest_parallel_args += ['--'] + executable_args

  return Args(gtest_parallel_args, test_env, options.output_dir,
              test_artifacts_dir)


def main():
  webrtc_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  gtest_parallel_path = os.path.join(
      webrtc_root, 'third_party', 'gtest-parallel', 'gtest-parallel')

  gtest_parallel_args, test_env, output_dir, test_artifacts_dir = ParseArgs()

  command = [
      sys.executable,
      gtest_parallel_path,
  ] + gtest_parallel_args

  if output_dir and not os.path.isdir(output_dir):
    os.makedirs(output_dir)
  if test_artifacts_dir and not os.path.isdir(test_artifacts_dir):
    os.makedirs(test_artifacts_dir)

  print 'gtest-parallel-wrapper: Executing command %s' % ' '.join(command)
  sys.stdout.flush()

  exit_code = subprocess.call(command, env=test_env, cwd=os.getcwd())

  if output_dir:
    for test_status in 'passed', 'failed', 'interrupted':
      logs_dir = os.path.join(output_dir, 'gtest-parallel-logs', test_status)
      if not os.path.isdir(logs_dir):
        continue
      logs = [os.path.join(logs_dir, log) for log in os.listdir(logs_dir)]
      log_file = os.path.join(output_dir, '%s-tests.log' % test_status)
      _CatFiles(logs, log_file)
      os.rmdir(logs_dir)

  if test_artifacts_dir:
    shutil.make_archive(test_artifacts_dir, 'zip', test_artifacts_dir)
    shutil.rmtree(test_artifacts_dir)

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
