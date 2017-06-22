#!/usr/bin/env python
# Copyright 2017 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import glob
import os
import shutil
import sys
import tempfile
import unittest


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.join(SCRIPT_DIR, os.pardir)
sys.path.append(PARENT_DIR)
import roll_deps
from roll_deps import CalculateChangedDeps, GetMatchingDepsEntries, \
  ParseDepsDict, ParseLocalDepsFile, UpdateDepsFile


TEST_DATA_VARS = {
  'chromium_git': 'https://chromium.googlesource.com',
  'chromium_revision': '1b9c098a08e40114e44b6c1ec33ddf95c40b901d',
}

DEPS_ENTRIES = {
  'src/build': 'https://build.com',
  'src/buildtools': 'https://buildtools.com',
  'src/testing/gtest': 'https://gtest.com',
  'src/testing/gmock': 'https://gmock.com',
}

BUILD_OLD_REV = '52f7afeca991d96d68cf0507e20dbdd5b845691f'
BUILD_NEW_REV = 'HEAD'
BUILDTOOLS_OLD_REV = '64e38f0cebdde27aa0cfb405f330063582f9ac76'
BUILDTOOLS_NEW_REV = '55ad626b08ef971fd82a62b7abb325359542952b'


class TestError(Exception):
  pass


class FakeCmd(object):
  def __init__(self):
    self.expectations = []

  def add_expectation(self, *args, **kwargs):
    returns = kwargs.pop('_returns', None)
    self.expectations.append((args, kwargs, returns))

  def __call__(self, *args, **kwargs):
    if not self.expectations:
      raise TestError('Got unexpected\n%s\n%s' % (args, kwargs))
    exp_args, exp_kwargs, exp_returns = self.expectations.pop(0)
    if args != exp_args or kwargs != exp_kwargs:
      message = 'Expected:\n  args: %s\n  kwargs: %s\n' % (exp_args, exp_kwargs)
      message += 'Got:\n  args: %s\n  kwargs: %s\n' % (args, kwargs)
      raise TestError(message)
    return exp_returns


class TestRollChromiumRevision(unittest.TestCase):
  def setUp(self):
    self._output_dir = tempfile.mkdtemp()
    for test_file in glob.glob(os.path.join(SCRIPT_DIR, 'testdata', '*')):
      shutil.copy(test_file, self._output_dir)
    self._libyuv_depsfile = os.path.join(self._output_dir, 'DEPS')
    self._old_cr_depsfile = os.path.join(self._output_dir, 'DEPS.chromium.old')
    self._new_cr_depsfile = os.path.join(self._output_dir, 'DEPS.chromium.new')

    self.fake = FakeCmd()
    self.old_RunCommand = getattr(roll_deps, '_RunCommand')
    setattr(roll_deps, '_RunCommand', self.fake)

  def tearDown(self):
    shutil.rmtree(self._output_dir, ignore_errors=True)
    self.assertEqual(self.fake.expectations, [])
    setattr(roll_deps, '_RunCommand', self.old_RunCommand)

  def testUpdateDepsFile(self):
    new_rev = 'aaaaabbbbbcccccdddddeeeeefffff0000011111'

    current_rev = TEST_DATA_VARS['chromium_revision']
    UpdateDepsFile(self._libyuv_depsfile, current_rev, new_rev, [])
    with open(self._libyuv_depsfile) as deps_file:
      deps_contents = deps_file.read()
      self.assertTrue(new_rev in deps_contents,
                      'Failed to find %s in\n%s' % (new_rev, deps_contents))

  def testParseDepsDict(self):
    with open(self._libyuv_depsfile) as deps_file:
      deps_contents = deps_file.read()
    local_scope = ParseDepsDict(deps_contents)
    vars_dict = local_scope['vars']

    def assertVar(variable_name):
      self.assertEquals(vars_dict[variable_name], TEST_DATA_VARS[variable_name])
    assertVar('chromium_git')
    assertVar('chromium_revision')
    self.assertEquals(len(local_scope['deps']), 3)

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

  def testCalculateChangedDeps(self):
    _SetupGitLsRemoteCall(self.fake,
        'https://chromium.googlesource.com/chromium/src/build', BUILD_NEW_REV)
    libyuv_deps = ParseLocalDepsFile(self._libyuv_depsfile)
    new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile)
    changed_deps = CalculateChangedDeps(libyuv_deps, new_cr_deps)
    self.assertEquals(len(changed_deps), 2)
    self.assertEquals(changed_deps[0].path, 'src/build')
    self.assertEquals(changed_deps[0].current_rev, BUILD_OLD_REV)
    self.assertEquals(changed_deps[0].new_rev, BUILD_NEW_REV)

    self.assertEquals(changed_deps[1].path, 'src/buildtools')
    self.assertEquals(changed_deps[1].current_rev, BUILDTOOLS_OLD_REV)
    self.assertEquals(changed_deps[1].new_rev, BUILDTOOLS_NEW_REV)


def _SetupGitLsRemoteCall(cmd_fake, url, revision):
  cmd = ['git', 'ls-remote', url, revision]
  cmd_fake.add_expectation(cmd, _returns=(revision, None))


if __name__ == '__main__':
  unittest.main()
