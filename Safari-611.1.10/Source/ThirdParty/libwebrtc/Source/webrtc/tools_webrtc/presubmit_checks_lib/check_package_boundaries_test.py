#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import ast
import os
import unittest

#pylint: disable=relative-import
from check_package_boundaries import CheckPackageBoundaries


MSG_FORMAT = 'ERROR:check_package_boundaries.py: Unexpected %s.'
TESTDATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            'testdata')


def ReadPylFile(file_path):
  with open(file_path) as f:
    return ast.literal_eval(f.read())


class UnitTest(unittest.TestCase):
  def _RunTest(self, test_dir, check_all_build_files=False):
    build_files = [os.path.join(test_dir, 'BUILD.gn')]
    if check_all_build_files:
      build_files = None

    messages = []
    for violation in CheckPackageBoundaries(test_dir, build_files):
      build_file_path = os.path.relpath(violation.build_file_path, test_dir)
      build_file_path = build_file_path.replace(os.path.sep, '/')
      messages.append(violation._replace(build_file_path=build_file_path))

    expected_messages = ReadPylFile(os.path.join(test_dir, 'expected.pyl'))
    self.assertListEqual(sorted(expected_messages), sorted(messages))

  def testNoErrors(self):
    self._RunTest(os.path.join(TESTDATA_DIR, 'no_errors'))

  def testMultipleErrorsSingleTarget(self):
    self._RunTest(os.path.join(TESTDATA_DIR, 'multiple_errors_single_target'))

  def testMultipleErrorsMultipleTargets(self):
    self._RunTest(os.path.join(TESTDATA_DIR,
                               'multiple_errors_multiple_targets'))

  def testCommonPrefix(self):
    self._RunTest(os.path.join(TESTDATA_DIR, 'common_prefix'))

  def testAllBuildFiles(self):
    self._RunTest(os.path.join(TESTDATA_DIR, 'all_build_files'), True)

  def testSanitizeFilename(self):
    # The `dangerous_filename` test case contains a directory with '++' in its
    # name. If it's not properly escaped, a regex error would be raised.
    self._RunTest(os.path.join(TESTDATA_DIR, 'dangerous_filename'), True)

  def testRelativeFilename(self):
    test_dir = os.path.join(TESTDATA_DIR, 'all_build_files')
    with self.assertRaises(AssertionError):
      CheckPackageBoundaries(test_dir, ["BUILD.gn"])


if __name__ == '__main__':
  unittest.main()
