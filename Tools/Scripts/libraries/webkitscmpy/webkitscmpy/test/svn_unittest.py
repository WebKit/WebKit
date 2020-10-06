# Copyright (C) 2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import shutil
import tempfile
import unittest

from webkitcorepy import OutputCapture
from webkitscmpy import local, mocks


class TestSvn(unittest.TestCase):
    path = '/mock/repository'

    def test_detection(self):
        with mocks.local.Svn(self.path), mocks.local.Git():
            detect = local.Scm.from_path(self.path)
            self.assertEqual(detect.executable, local.Svn.executable)

    def test_root(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(local.Svn(self.path).root_path, self.path)

            with self.assertRaises(OSError):
                local.Svn(os.path.dirname(self.path)).root_path

    def test_branch(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(local.Svn(self.path).branch, 'trunk')

    def test_remote(self):
        with mocks.local.Svn(self.path) as repo:
            self.assertEqual(local.Svn(self.path).remote(), repo.remote)

    def test_branches(self):
        with mocks.local.Svn(self.path, branches=('branch-1', 'branch-2')):
            self.assertEqual(
                local.Svn(self.path).branches,
                ['trunk', 'branch-1', 'branch-2', 'branch-a', 'branch-b'],
            )

    def test_tags(self):
        with mocks.local.Svn(self.path, tags=('tag-1', 'tag-2')):
            self.assertEqual(
                local.Svn(self.path).tags,
                ['tag-1', 'tag-2'],
            )

    def test_default_branch(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(local.Svn(self.path).default_branch, 'trunk')

    def test_scm_type(self):
        with mocks.local.Svn(self.path):
            self.assertTrue(local.Svn(self.path).is_svn)
            self.assertFalse(local.Svn(self.path).is_git)

    def test_info(self):
        with mocks.local.Svn(self.path):
            self.assertDictEqual(
                {
                    u'Path': u'.',
                    u'Working Copy Root Path': u'/mock/repository',
                    u'Repository Root': u'https://svn.mock.org/repository/repository',
                    u'URL': u'https://svn.mock.org/repository/repository/trunk',
                    u'Relative URL':  u'^/trunk',
                    u'Revision': u'6',
                    u'NodeKind': u'directory',
                    u'Schedule': u'normal',
                    u'Last Changed Author': u'jbedard@apple.com',
                    u'Last Changed Rev': u'6',
                    u'Last Changed Date': u'2020-10-02 11:58:20 -0100 (Fri, 02 Oct 2020)',
                }, local.Svn(self.path).info(),
            )

    def test_commit_revision(self):
        with mocks.local.Svn(self.path), OutputCapture():
            self.assertEqual('1@trunk', str(local.Svn(self.path).commit(revision=1)))
            self.assertEqual('2@trunk', str(local.Svn(self.path).commit(revision=2)))
            self.assertEqual('2.1@branch-a', str(local.Svn(self.path).commit(revision=3)))
            self.assertEqual('3@trunk', str(local.Svn(self.path).commit(revision=4)))
            self.assertEqual('2.2@branch-b', str(local.Svn(self.path).commit(revision=5)))
            self.assertEqual('4@trunk', str(local.Svn(self.path).commit(revision=6)))
            self.assertEqual('2.2@branch-a', str(local.Svn(self.path).commit(revision=7)))
            self.assertEqual('2.3@branch-b', str(local.Svn(self.path).commit(revision=8)))

            # Out-of-bounds commit
            with self.assertRaises(local.Svn.Exception):
                self.assertEqual(None, local.Svn(self.path).commit(revision=10))

    def test_commit_from_branch(self):
        with mocks.local.Svn(self.path), OutputCapture():
            self.assertEqual('4@trunk', str(local.Svn(self.path).commit(branch='trunk')))
            self.assertEqual('2.2@branch-a', str(local.Svn(self.path).commit(branch='branch-a')))
            self.assertEqual('2.3@branch-b', str(local.Svn(self.path).commit(branch='branch-b')))

    def test_identifier(self):
        with mocks.local.Svn(self.path), OutputCapture():
            self.assertEqual(1, local.Svn(self.path).commit(identifier='1@trunk').revision)
            self.assertEqual(2, local.Svn(self.path).commit(identifier='2@trunk').revision)
            self.assertEqual(3, local.Svn(self.path).commit(identifier='2.1@branch-a').revision)
            self.assertEqual(4, local.Svn(self.path).commit(identifier='3@trunk').revision)
            self.assertEqual(5, local.Svn(self.path).commit(identifier='2.2@branch-b').revision)
            self.assertEqual(6, local.Svn(self.path).commit(identifier='4@trunk').revision)
            self.assertEqual(7, local.Svn(self.path).commit(identifier='2.2@branch-a').revision)
            self.assertEqual(8, local.Svn(self.path).commit(identifier='2.3@branch-b').revision)

    def test_non_cannonical_identifiers(self):
        with mocks.local.Svn(self.path), OutputCapture():
            self.assertEqual('2@trunk', str(local.Svn(self.path).commit(identifier='0@branch-a')))
            self.assertEqual('1@trunk', str(local.Svn(self.path).commit(identifier='-1@branch-a')))

            self.assertEqual('2@trunk', str(local.Svn(self.path).commit(identifier='0@branch-b')))
            self.assertEqual('1@trunk', str(local.Svn(self.path).commit(identifier='-1@branch-b')))

    def test_branches_priority(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(
                'trunk',
                local.Svn(self.path).prioritize_branches(['trunk', 'branch-a', 'dev/12345', 'safari-610-branch', 'safari-610.1-branch'])
            )

            self.assertEqual(
                'safari-610-branch',
                local.Svn(self.path).prioritize_branches(['branch-a', 'dev/12345', 'safari-610-branch', 'safari-610.1-branch'])
            )

            self.assertEqual(
                'safari-610.1-branch',
                local.Svn(self.path).prioritize_branches(['branch-a', 'dev/12345', 'safari-610.1-branch'])
            )

            self.assertEqual(
                'branch-a',
                local.Svn(self.path).prioritize_branches(['branch-a', 'dev/12345'])
            )

    def test_no_network(self):
        with mocks.local.Svn(self.path) as mock_repo, OutputCapture():
            repo = local.Svn(self.path)
            self.assertEqual('4@trunk', str(repo.commit()))

            mock_repo.connected = False
            commit = repo.commit()
            self.assertEqual('4@trunk', str(commit))
            self.assertEqual(6, commit.revision)
            self.assertEqual(None, commit.message)

            commit = repo.commit(revision=4)
            self.assertEqual('3@trunk', str(commit))
            self.assertEqual(None, commit.message)

            with self.assertRaises(local.Svn.Exception):
                repo.commit(revision=3)

            mock_repo.connected = True
            self.assertEqual('2.1@branch-a', str(repo.commit(revision=3)))

            mock_repo.connected = False
            self.assertEqual('2.1@branch-a', str(repo.commit(revision=3)))

    def test_cache(self):
        try:
            dirname = tempfile.mkdtemp()
            with mocks.local.Svn(dirname) as mock_repo, OutputCapture():
                os.mkdir(os.path.join(dirname, '.svn'))
                self.assertEqual('4@trunk', str(local.Svn(dirname).commit()))

                mock_repo.connected = False
                commit = local.Svn(dirname).commit()
                print(commit.revision)
                self.assertEqual('4@trunk', str(commit))

                with self.assertRaises(local.Svn.Exception):
                    local.Svn(dirname).commit(revision=3)
        finally:
            shutil.rmtree(dirname)
