#!/usr/bin/env vpython
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
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
from roll_deps import CalculateChangedDeps, FindAddedDeps, \
  FindRemovedDeps, ChooseCQMode, GenerateCommitMessage, \
  GetMatchingDepsEntries, ParseDepsDict, ParseLocalDepsFile, UpdateDepsFile, \
  ChromiumRevisionUpdate

import mock

TEST_DATA_VARS = {
    'chromium_git': 'https://chromium.googlesource.com',
    'chromium_revision': '1b9c098a08e40114e44b6c1ec33ddf95c40b901d',
}

DEPS_ENTRIES = {
    'src/build': 'https://build.com',
    'src/third_party/depot_tools': 'https://depottools.com',
    'src/testing/gtest': 'https://gtest.com',
    'src/testing/gmock': 'https://gmock.com',
}

BUILD_OLD_REV = '52f7afeca991d96d68cf0507e20dbdd5b845691f'
BUILD_NEW_REV = 'HEAD'
DEPOTTOOLS_OLD_REV = 'b9ae2ca9a55d9b754c313f4c9e9f0f3b804a5e44'
DEPOTTOOLS_NEW_REV = '1206a353e40abb70d8454eb9af53db0ad10b713c'

NO_CHROMIUM_REVISION_UPDATE = ChromiumRevisionUpdate('cafe', 'cafe')


class TestError(Exception):
    pass


class FakeCmd(object):
    def __init__(self):
        self.expectations = []

    def AddExpectation(self, *args, **kwargs):
        returns = kwargs.pop('_returns', None)
        ignores = kwargs.pop('_ignores', [])
        self.expectations.append((args, kwargs, returns, ignores))

    def __call__(self, *args, **kwargs):
        if not self.expectations:
            raise TestError('Got unexpected\n%s\n%s' % (args, kwargs))
        exp_args, exp_kwargs, exp_returns, ignores = self.expectations.pop(0)
        for item in ignores:
            kwargs.pop(item, None)
        if args != exp_args or kwargs != exp_kwargs:
            message = 'Expected:\n  args: %s\n  kwargs: %s\n' % (exp_args,
                                                                 exp_kwargs)
            message += 'Got:\n  args: %s\n  kwargs: %s\n' % (args, kwargs)
            raise TestError(message)
        return exp_returns


class NullCmd(object):
    """No-op mock when calls mustn't be checked. """

    def __call__(self, *args, **kwargs):
        # Empty stdout and stderr.
        return None, None


class TestRollChromiumRevision(unittest.TestCase):
    def setUp(self):
        self._output_dir = tempfile.mkdtemp()
        test_data_dir = os.path.join(SCRIPT_DIR, 'testdata', 'roll_deps')
        for test_file in glob.glob(os.path.join(test_data_dir, '*')):
            shutil.copy(test_file, self._output_dir)
        join = lambda f: os.path.join(self._output_dir, f)
        self._webrtc_depsfile = join('DEPS')
        self._new_cr_depsfile = join('DEPS.chromium.new')
        self._webrtc_depsfile_android = join('DEPS.with_android_deps')
        self._new_cr_depsfile_android = join('DEPS.chromium.with_android_deps')
        self.fake = FakeCmd()

    def tearDown(self):
        shutil.rmtree(self._output_dir, ignore_errors=True)
        self.assertEqual(self.fake.expectations, [])

    def testVarLookup(self):
        local_scope = {'foo': 'wrong', 'vars': {'foo': 'bar'}}
        lookup = roll_deps.VarLookup(local_scope)
        self.assertEquals(lookup('foo'), 'bar')

    def testUpdateDepsFile(self):
        new_rev = 'aaaaabbbbbcccccdddddeeeeefffff0000011111'
        current_rev = TEST_DATA_VARS['chromium_revision']

        with open(self._new_cr_depsfile_android) as deps_file:
            new_cr_contents = deps_file.read()

        UpdateDepsFile(self._webrtc_depsfile,
                       ChromiumRevisionUpdate(current_rev, new_rev), [],
                       new_cr_contents)
        with open(self._webrtc_depsfile) as deps_file:
            deps_contents = deps_file.read()
            self.assertTrue(
                new_rev in deps_contents,
                'Failed to find %s in\n%s' % (new_rev, deps_contents))

    def _UpdateDepsSetup(self):
        with open(self._webrtc_depsfile_android) as deps_file:
            webrtc_contents = deps_file.read()
        with open(self._new_cr_depsfile_android) as deps_file:
            new_cr_contents = deps_file.read()
        webrtc_deps = ParseDepsDict(webrtc_contents)
        new_cr_deps = ParseDepsDict(new_cr_contents)

        changed_deps = CalculateChangedDeps(webrtc_deps, new_cr_deps)
        with mock.patch('roll_deps._RunCommand', NullCmd()):
            UpdateDepsFile(self._webrtc_depsfile_android,
                           NO_CHROMIUM_REVISION_UPDATE, changed_deps,
                           new_cr_contents)

        with open(self._webrtc_depsfile_android) as deps_file:
            updated_contents = deps_file.read()

        return webrtc_contents, updated_contents

    def testUpdateAndroidGeneratedDeps(self):
        _, updated_contents = self._UpdateDepsSetup()

        changed = 'third_party/android_deps/libs/android_arch_core_common'
        changed_version = '1.0.0-cr0'
        self.assertTrue(changed in updated_contents)
        self.assertTrue(changed_version in updated_contents)

    def testAddAndroidGeneratedDeps(self):
        webrtc_contents, updated_contents = self._UpdateDepsSetup()

        added = 'third_party/android_deps/libs/android_arch_lifecycle_common'
        self.assertFalse(added in webrtc_contents)
        self.assertTrue(added in updated_contents)

    def testRemoveAndroidGeneratedDeps(self):
        webrtc_contents, updated_contents = self._UpdateDepsSetup()

        removed = 'third_party/android_deps/libs/android_arch_lifecycle_runtime'
        self.assertTrue(removed in webrtc_contents)
        self.assertFalse(removed in updated_contents)

    def testParseDepsDict(self):
        with open(self._webrtc_depsfile) as deps_file:
            deps_contents = deps_file.read()
        local_scope = ParseDepsDict(deps_contents)
        vars_dict = local_scope['vars']

        def AssertVar(variable_name):
            self.assertEquals(vars_dict[variable_name],
                              TEST_DATA_VARS[variable_name])

        AssertVar('chromium_git')
        AssertVar('chromium_revision')
        self.assertEquals(len(local_scope['deps']), 3)
        self.assertEquals(len(local_scope['deps_os']), 1)

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

    def testCalculateChangedDeps(self):
        webrtc_deps = ParseLocalDepsFile(self._webrtc_depsfile)
        new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile)
        with mock.patch('roll_deps._RunCommand', self.fake):
            _SetupGitLsRemoteCall(
                self.fake,
                'https://chromium.googlesource.com/chromium/src/build',
                BUILD_NEW_REV)
            changed_deps = CalculateChangedDeps(webrtc_deps, new_cr_deps)

        self.assertEquals(len(changed_deps), 3)
        self.assertEquals(changed_deps[0].path, 'src/build')
        self.assertEquals(changed_deps[0].current_rev, BUILD_OLD_REV)
        self.assertEquals(changed_deps[0].new_rev, BUILD_NEW_REV)

        self.assertEquals(changed_deps[1].path, 'src/third_party/depot_tools')
        self.assertEquals(changed_deps[1].current_rev, DEPOTTOOLS_OLD_REV)
        self.assertEquals(changed_deps[1].new_rev, DEPOTTOOLS_NEW_REV)

        self.assertEquals(changed_deps[2].path, 'src/third_party/xstream')
        self.assertEquals(changed_deps[2].package,
                          'chromium/third_party/xstream')
        self.assertEquals(changed_deps[2].current_version, 'version:1.4.8-cr0')
        self.assertEquals(changed_deps[2].new_version, 'version:1.10.0-cr0')

    def testWithDistinctDeps(self):
        """Check CalculateChangedDeps still works when deps are added/removed. """
        webrtc_deps = ParseLocalDepsFile(self._webrtc_depsfile_android)
        new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile_android)
        changed_deps = CalculateChangedDeps(webrtc_deps, new_cr_deps)
        self.assertEquals(len(changed_deps), 1)
        self.assertEquals(
            changed_deps[0].path,
            'src/third_party/android_deps/libs/android_arch_core_common')
        self.assertEquals(
            changed_deps[0].package,
            'chromium/third_party/android_deps/libs/android_arch_core_common')
        self.assertEquals(changed_deps[0].current_version, 'version:0.9.0')
        self.assertEquals(changed_deps[0].new_version, 'version:1.0.0-cr0')

    def testFindAddedDeps(self):
        webrtc_deps = ParseLocalDepsFile(self._webrtc_depsfile_android)
        new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile_android)
        added_android_paths, other_paths = FindAddedDeps(
            webrtc_deps, new_cr_deps)
        self.assertEquals(added_android_paths, [
            'src/third_party/android_deps/libs/android_arch_lifecycle_common'
        ])
        self.assertEquals(other_paths, [])

    def testFindRemovedDeps(self):
        webrtc_deps = ParseLocalDepsFile(self._webrtc_depsfile_android)
        new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile_android)
        removed_android_paths, other_paths = FindRemovedDeps(
            webrtc_deps, new_cr_deps)
        self.assertEquals(removed_android_paths, [
            'src/third_party/android_deps/libs/android_arch_lifecycle_runtime'
        ])
        self.assertEquals(other_paths, [])

    def testMissingDepsIsDetected(self):
        """Check an error is reported when deps cannot be automatically removed."""
        # The situation at test is the following:
        #   * A WebRTC DEPS entry is missing from Chromium.
        #   * The dependency isn't an android_deps (those are supported).
        webrtc_deps = ParseLocalDepsFile(self._webrtc_depsfile)
        new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile_android)
        _, other_paths = FindRemovedDeps(webrtc_deps, new_cr_deps)
        self.assertEquals(
            other_paths,
            ['src/third_party/xstream', 'src/third_party/depot_tools'])

    def testExpectedDepsIsNotReportedMissing(self):
        """Some deps musn't be seen as missing, even if absent from Chromium."""
        webrtc_deps = ParseLocalDepsFile(self._webrtc_depsfile)
        new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile_android)
        removed_android_paths, other_paths = FindRemovedDeps(
            webrtc_deps, new_cr_deps)
        self.assertTrue('src/build' not in removed_android_paths)
        self.assertTrue('src/build' not in other_paths)

    def _CommitMessageSetup(self):
        webrtc_deps = ParseLocalDepsFile(self._webrtc_depsfile_android)
        new_cr_deps = ParseLocalDepsFile(self._new_cr_depsfile_android)

        changed_deps = CalculateChangedDeps(webrtc_deps, new_cr_deps)
        added_paths, _ = FindAddedDeps(webrtc_deps, new_cr_deps)
        removed_paths, _ = FindRemovedDeps(webrtc_deps, new_cr_deps)

        current_commit_pos = 'cafe'
        new_commit_pos = 'f00d'

        with mock.patch('roll_deps._RunCommand', self.fake):
            # We don't really care, but it's needed to construct the message.
            self.fake.AddExpectation(['git', 'config', 'user.email'],
                                     _returns=('nobody@nowhere.no', None),
                                     _ignores=['working_dir'])

            commit_msg = GenerateCommitMessage(NO_CHROMIUM_REVISION_UPDATE,
                                               current_commit_pos,
                                               new_commit_pos, changed_deps,
                                               added_paths, removed_paths)

        return [l.strip() for l in commit_msg.split('\n')]

    def testChangedDepsInCommitMessage(self):
        commit_lines = self._CommitMessageSetup()

        changed = '* src/third_party/android_deps/libs/' \
                  'android_arch_core_common: version:0.9.0..version:1.0.0-cr0'
        self.assertTrue(changed in commit_lines)
        # Check it is in adequate section.
        changed_line = commit_lines.index(changed)
        self.assertTrue('Changed' in commit_lines[changed_line - 1])

    def testAddedDepsInCommitMessage(self):
        commit_lines = self._CommitMessageSetup()

        added = '* src/third_party/android_deps/libs/' \
                'android_arch_lifecycle_common'
        self.assertTrue(added in commit_lines)
        # Check it is in adequate section.
        added_line = commit_lines.index(added)
        self.assertTrue('Added' in commit_lines[added_line - 1])

    def testRemovedDepsInCommitMessage(self):
        commit_lines = self._CommitMessageSetup()

        removed = '* src/third_party/android_deps/libs/' \
                  'android_arch_lifecycle_runtime'
        self.assertTrue(removed in commit_lines)
        # Check it is in adequate section.
        removed_line = commit_lines.index(removed)
        self.assertTrue('Removed' in commit_lines[removed_line - 1])


class TestChooseCQMode(unittest.TestCase):
    def testSkip(self):
        self.assertEquals(ChooseCQMode(True, 99, 500000, 500100), 0)

    def testDryRun(self):
        self.assertEquals(ChooseCQMode(False, 101, 500000, 500100), 1)

    def testSubmit(self):
        self.assertEquals(ChooseCQMode(False, 100, 500000, 500100), 2)


def _SetupGitLsRemoteCall(cmd_fake, url, revision):
    cmd = ['git', 'ls-remote', url, revision]
    cmd_fake.AddExpectation(cmd, _returns=(revision, None))


if __name__ == '__main__':
    unittest.main()
