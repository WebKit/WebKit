#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import shutil
import sys
import tempfile
import unittest


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.join(SCRIPT_DIR, os.pardir)
sys.path.append(PARENT_DIR)
from roll_chromium_revision import ParseDepsDict, UpdateDeps, \
    GetMatchingDepsEntries

TEST_DATA_VARS = {
  'extra_gyp_flag': '-Dextra_gyp_flag=0',
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '1b9c098a08e40114e44b6c1ec33ddf95c40b901d',
}

DEPS_ENTRIES = {
  'src/build': 'https://build.com',
  'src/buildtools': 'https://buildtools.com',
  'src/testing/gtest': 'https://gtest.com',
  'src/testing/gmock': 'https://gmock.com',
}


class TestRollChromiumRevision(unittest.TestCase):
  def setUp(self):
    self._output_dir = tempfile.mkdtemp()
    shutil.copy(os.path.join(SCRIPT_DIR, 'DEPS'), self._output_dir)
    self._deps_filename = os.path.join(self._output_dir, 'DEPS')

  def tearDown(self):
    shutil.rmtree(self._output_dir, ignore_errors=True)

  def testUpdateDeps(self):
    new_rev = 'aaaaabbbbbcccccdddddeeeeefffff0000011111'

    current_rev = TEST_DATA_VARS['chromium_revision']
    UpdateDeps(self._deps_filename, current_rev, new_rev)
    with open(self._deps_filename) as deps_file:
      deps_contents = deps_file.read()
      self.assertTrue(new_rev in deps_contents,
                      'Failed to find %s in\n%s' % (new_rev, deps_contents))

  def testParseDepsDict(self):
    with open(self._deps_filename) as deps_file:
      deps_contents = deps_file.read()
    local_scope = ParseDepsDict(deps_contents)
    vars_dict = local_scope['vars']

    def assertVar(variable_name):
      self.assertEquals(vars_dict[variable_name], TEST_DATA_VARS[variable_name])
    assertVar('extra_gyp_flag')
    assertVar('chromium_git')
    assertVar('chromium_revision')

  def testGetMatchingDepsEntriesReturnsPathInSimpleCase(self):
    entries = GetMatchingDepsEntries(DEPS_ENTRIES, 'src/testing/gtest')
    self.assertEquals(len(entries), 1)
    self.assertEquals(entries[0], DEPS_ENTRIES['src/testing/gtest'])

  def testGetMatchingDepsEntriesHandlesSimilarStartingPaths(self):
    entries = GetMatchingDepsEntries(DEPS_ENTRIES, 'src/testing')
    self.assertEquals(len(entries), 2)

  def testGetMatchingDepsEntriesHandlesTwoPathsWithIdenticalFirstParts(self):
    entries = GetMatchingDepsEntries(DEPS_ENTRIES, 'src/build')
    self.assertEquals(len(entries), 1)
    self.assertEquals(entries[0], DEPS_ENTRIES['src/build'])

if __name__ == '__main__':
  unittest.main()
