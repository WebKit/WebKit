#!/usr/bin/env python
# Copyright (c) 2012 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Runs various libyuv tests through valgrind_test.py.

This script inherits the chrome_tests.py in Chrome, but allows running any test
instead of only the hard-coded ones. It uses the -t cmdline flag to do this, and
only supports specifying a single test for each run.

Suppression files:
The Chrome valgrind directory we use as a DEPS dependency contains the following
suppression files:
  valgrind/memcheck/suppressions.txt
  valgrind/memcheck/suppressions_mac.txt
  valgrind/tsan/suppressions.txt
  valgrind/tsan/suppressions_mac.txt
  valgrind/tsan/suppressions_win32.txt
Since they're referenced from the chrome_tests.py script, we have similar files
below the directory of this script. When executing, this script will setup both
Chrome's suppression files and our own, so we can easily maintain libyuv
specific suppressions in our own files.
"""

import logging
import optparse
import os
import sys

import logging_utils
import path_utils

import chrome_tests


class LibyuvTest(chrome_tests.ChromeTests):
  """Class that handles setup of suppressions for libyuv.

  Everything else is inherited from chrome_tests.ChromeTests.
  """

  def _DefaultCommand(self, tool, exe=None, valgrind_test_args=None):
    """Override command-building method so we can add more suppressions."""
    cmd = chrome_tests.ChromeTests._DefaultCommand(self, tool, exe,
                                                   valgrind_test_args)
    # When ChromeTests._DefaultCommand has executed, it has setup suppression
    # files based on what's found in the memcheck/ or tsan/ subdirectories of
    # this script's location. If Mac or Windows is executing, additional
    # platform specific files have also been added.
    # Since only the ones located below this directory is added, we must also
    # add the ones maintained by Chrome, located in ../../tools/valgrind.

    # The idea is to look for --suppression arguments in the cmd list and add a
    # modified copy of each suppression file, for the corresponding file in
    # ../../tools/valgrind.
    script_dir = path_utils.ScriptDir()
    old_base, _ = os.path.split(script_dir)

    checkout_src = os.path.abspath(os.path.join(script_dir, os.pardir,
                                                os.pardir))
    new_dir = os.path.join(checkout_src, 'tools', 'valgrind')
    add_suppressions = []
    for token in cmd:
      if '--suppressions' in token:
        add_suppressions.append(token.replace(script_dir, new_dir))
    return add_suppressions + cmd


def main(_):
  parser = optparse.OptionParser('usage: %prog -b <dir> -t <test> <test args>')
  parser.disable_interspersed_args()
  parser.add_option('-b', '--build-dir',
                    help=('Location of the compiler output. Can only be used '
                          'when the test argument does not contain this path.'))
  parser.add_option("--target", help="Debug or Release")
  parser.add_option('-t', '--test', help='Test to run.')
  parser.add_option('', '--baseline', action='store_true', default=False,
                    help='Generate baseline data instead of validating')
  parser.add_option('', '--gtest_filter',
                    help='Additional arguments to --gtest_filter')
  parser.add_option('', '--gtest_repeat',
                    help='Argument for --gtest_repeat')
  parser.add_option("--gtest_shuffle", action="store_true", default=False,
                    help="Randomize tests' orders on every iteration.")
  parser.add_option("--gtest_break_on_failure", action="store_true",
                    default=False,
                    help="Drop in to debugger on assertion failure. Also "
                         "useful for forcing tests to exit with a stack dump "
                         "on the first assertion failure when running with "
                         "--gtest_repeat=-1")
  parser.add_option('-v', '--verbose', action='store_true', default=False,
                    help='Verbose output - enable debug log messages')
  parser.add_option('', '--tool', dest='valgrind_tool', default='memcheck',
                    help='Specify a valgrind tool to run the tests under')
  parser.add_option('', '--tool_flags', dest='valgrind_tool_flags', default='',
                    help='Specify custom flags for the selected valgrind tool')
  parser.add_option('', '--keep_logs', action='store_true', default=False,
                    help=('Store memory tool logs in the <tool>.logs directory '
                          'instead of /tmp.\nThis can be useful for tool '
                          'developers/maintainers.\nPlease note that the <tool>'
                          '.logs directory will be clobbered on tool startup.'))
  parser.add_option("--test-launcher-bot-mode", action="store_true",
                    help="run the tests with --test-launcher-bot-mode")
  parser.add_option("--test-launcher-total-shards", type=int,
                    help="run the tests with --test-launcher-total-shards")
  parser.add_option("--test-launcher-shard-index", type=int,
                    help="run the tests with --test-launcher-shard-index")
  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if not options.test:
    parser.error('--test not specified')

  # Support build dir both with and without the target.
  if (options.target and options.build_dir and
      not options.build_dir.endswith(options.target)):
    options.build_dir = os.path.join(options.build_dir, options.target)

  # If --build_dir is provided, prepend it to the test executable if needed.
  test_executable = options.test
  if options.build_dir and not test_executable.startswith(options.build_dir):
    test_executable = os.path.join(options.build_dir, test_executable)
  args = [test_executable] + args

  test = LibyuvTest(options, args, 'cmdline')
  return test.Run()

if __name__ == '__main__':
  return_code = main(sys.argv)
  sys.exit(return_code)
