# Copyright (C) 2020, 2021 Apple Inc. All rights reserved.
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

from datetime import datetime
from webkitcorepy import LoggerCapture, OutputCapture
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import local, mocks, remote


class TestGit(unittest.TestCase):
    path = '/mock/repository'

    def test_detection(self):
        with OutputCapture(), mocks.local.Git(self.path), mocks.local.Svn():
            detect = local.Scm.from_path(self.path)
            self.assertEqual(detect.executable, local.Git.executable)

    def test_root(self):
        with mocks.local.Git(self.path):
            self.assertEqual(local.Git(self.path).root_path, self.path)

            with self.assertRaises(OSError):
                local.Git(os.path.dirname(self.path)).root_path

    def test_branch(self):
        with mocks.local.Git(self.path):
            self.assertEqual(local.Git(self.path).branch, 'main')

        with mocks.local.Git(self.path, detached=True):
            self.assertEqual(local.Git(self.path).branch, None)

    def test_remote(self):
        with mocks.local.Git(self.path) as repo:
            self.assertEqual(local.Git(self.path).remote(), repo.remote)

    def test_branches(self):
        with mocks.local.Git(self.path):
            self.assertEqual(
                local.Git(self.path).branches,
                ['branch-a', 'branch-b', 'main'],
            )

    def test_tags(self):
        with mocks.local.Git(self.path) as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]
            mock.tags['tag-2'] = mock.commits['branch-b'][-1]

            self.assertEqual(
                local.Git(self.path).tags,
                ['tag-1', 'tag-2'],
            )

    def test_default_branch(self):
        with mocks.local.Git(self.path):
            self.assertEqual(local.Git(self.path).default_branch, 'main')

    def test_scm_type(self):
        try:
            dirname = tempfile.mkdtemp()
            with mocks.local.Git(dirname, remote='git@example.org:{}'.format(self.path)), MockTime, LoggerCapture():
                self.assertTrue(local.Git(dirname).is_git)
                self.assertFalse(local.Git(dirname).is_svn)

            with mocks.local.Git(dirname, git_svn=True, remote='git@example.org:{}'.format(self.path)), MockTime, LoggerCapture():
                self.assertTrue(local.Git(dirname).is_git)
                self.assertTrue(local.Git(dirname).is_svn)

        finally:
            shutil.rmtree(dirname)

    def test_info(self):
        try:
            dirname = tempfile.mkdtemp()
            with mocks.local.Git(dirname, remote='git@example.org:{}'.format(self.path)), MockTime, LoggerCapture():
                with self.assertRaises(local.Git.Exception):
                    self.assertEqual(dict(), local.Git(dirname).info())

            with mocks.local.Git(dirname, git_svn=True, remote='git@example.org:{}'.format(self.path)), MockTime:
                self.assertDictEqual(
                    {
                        'Path': '.',
                        'Repository Root': 'git@example.org:/mock/repository',
                        'URL': 'git@example.org:/mock/repository/main',
                        'Revision': '6',
                        'Node Kind': 'directory',
                        'Schedule': 'normal',
                        'Last Changed Author': 'jbedard@apple.com',
                        'Last Changed Rev': '6',
                        'Last Changed Date': datetime.fromtimestamp(1601665000).strftime('%Y-%m-%d %H:%M:%S'),
                    }, local.Git(dirname).info(),
                )
        finally:
            shutil.rmtree(dirname)

    def test_commit_revision(self):
        try:
            dirname = tempfile.mkdtemp()
            with mocks.local.Git(dirname), MockTime, LoggerCapture():
                with self.assertRaises(local.Git.Exception):
                    self.assertEqual(None, local.Git(dirname).commit(revision=1))

            with mocks.local.Git(dirname, git_svn=True, remote='git@example.org:{}'.format(self.path)), MockTime, LoggerCapture():
                self.assertEqual('1@main', str(local.Git(dirname).commit(revision=1)))
                self.assertEqual('2@main', str(local.Git(dirname).commit(revision=2)))
                self.assertEqual('2.1@branch-a', str(local.Git(dirname).commit(revision=3)))
                self.assertEqual('3@main', str(local.Git(dirname).commit(revision=4)))
                self.assertEqual('2.2@branch-b', str(local.Git(dirname).commit(revision=5)))
                self.assertEqual('4@main', str(local.Git(dirname).commit(revision=6)))
                self.assertEqual('2.2@branch-a', str(local.Git(dirname).commit(revision=7)))
                self.assertEqual('2.3@branch-b', str(local.Git(dirname).commit(revision=8)))

                # Out-of-bounds commit
                with self.assertRaises(local.Git.Exception):
                    self.assertEqual(None, local.Git(dirname).commit(revision=10))
        finally:
            shutil.rmtree(dirname)

    def test_commit_hash(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, MockTime, LoggerCapture():
                self.assertEqual('1@main', str(local.Git(self.path).commit(hash='9b8311f2')))
                self.assertEqual('2@main', str(local.Git(self.path).commit(hash='fff83bb2')))
                self.assertEqual('3@main', str(local.Git(self.path).commit(hash='1abe25b4')))
                self.assertEqual('4@main', str(local.Git(self.path).commit(hash='bae5d1e9')))

                self.assertEqual('2.1@branch-a', str(local.Git(self.path).commit(hash='a30ce849')))
                self.assertEqual('2.2@branch-a', str(local.Git(self.path).commit(hash='621652ad')))

                self.assertEqual('2.2@branch-b', str(local.Git(self.path).commit(hash='3cd32e35')))
                self.assertEqual('2.3@branch-b', str(local.Git(self.path).commit(hash='790725a6')))

    def test_commit_from_branch(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, MockTime, LoggerCapture():
                self.assertEqual('4@main', str(local.Git(self.path).commit(branch='main')))
                self.assertEqual('2.2@branch-a', str(local.Git(self.path).commit(branch='branch-a')))
                self.assertEqual('2.3@branch-b', str(local.Git(self.path).commit(branch='branch-b')))

    def test_identifier(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, MockTime, LoggerCapture():
                self.assertEqual(
                    '9b8311f25a77ba14923d9d5a6532103f54abefcb',
                    local.Git(self.path).commit(identifier='1@main').hash,
                )
                self.assertEqual(
                    'fff83bb2d9171b4d9196e977eb0508fd57e7a08d',
                    local.Git(self.path).commit(identifier='2@main').hash,
                )
                self.assertEqual(
                    '1abe25b443e985f93b90d830e4a7e3731336af4d',
                    local.Git(self.path).commit(identifier='3@main').hash,
                )
                self.assertEqual(
                    'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                    local.Git(self.path).commit(identifier='4@main').hash,
                )

                self.assertEqual(
                    'a30ce8494bf1ac2807a69844f726be4a9843ca55',
                    local.Git(self.path).commit(identifier='2.1@branch-a').hash,
                )
                self.assertEqual(
                    '621652add7fc416099bd2063366cc38ff61afe36',
                    local.Git(self.path).commit(identifier='2.2@branch-a').hash,
                )

                self.assertEqual(
                    '3cd32e352410565bb543821fbf856a6d3caad1c4',
                    local.Git(self.path).commit(identifier='2.2@branch-b').hash,
                )
                self.assertEqual(
                    '790725a6d79e28db2ecdde29548d2262c0bd059d',
                    local.Git(self.path).commit(identifier='2.3@branch-b').hash,
                )

    def test_non_cannonical_identifiers(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, MockTime, LoggerCapture():
                self.assertEqual('2@main', str(local.Git(self.path).commit(identifier='0@branch-a')))
                self.assertEqual('1@main', str(local.Git(self.path).commit(identifier='-1@branch-a')))

                self.assertEqual('2.1@branch-a', str(local.Git(self.path).commit(identifier='2.1@branch-b')))
                self.assertEqual('2@main', str(local.Git(self.path).commit(identifier='0@branch-b')))
                self.assertEqual('1@main', str(local.Git(self.path).commit(identifier='-1@branch-b')))

    def test_git_svn_revision_extract(self):
        with mocks.local.Git(self.path, git_svn=True), MockTime, LoggerCapture():
            self.assertEqual(1, local.Git(self.path).commit(hash='9b8311f2').revision)

    def test_branches_priority(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock:
                self.assertEqual(
                    'main',
                    local.Git(self.path).prioritize_branches(['main', 'branch-a', 'dev/12345', 'safari-610-branch', 'safari-610.1-branch'])
                )

                self.assertEqual(
                    'safari-610-branch',
                    local.Git(self.path).prioritize_branches(['branch-a', 'dev/12345', 'safari-610-branch', 'safari-610.1-branch'])
                )

                self.assertEqual(
                    'safari-610.1-branch',
                    local.Git(self.path).prioritize_branches(['branch-a', 'dev/12345', 'safari-610.1-branch'])
                )

                self.assertEqual(
                    'branch-a',
                    local.Git(self.path).prioritize_branches(['branch-a', 'dev/12345'])
                )

    def test_tag(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock:
                mock.tags['tag-1'] = mock.commits['branch-a'][-1]

                self.assertEqual(
                    '621652add7fc416099bd2063366cc38ff61afe36',
                    local.Git(self.path).commit(tag='tag-1').hash,
                )

    def test_checkout(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock:
                mock.tags['tag-1'] = mock.commits['branch-a'][-1]

                repository = local.Git(self.path)
                self.assertEqual('bae5d1e90999d4f916a8a15810ccfa43f37a2fd6', repository.commit().hash)
                self.assertEqual('3cd32e352410565bb543821fbf856a6d3caad1c4', repository.checkout('3cd32e3524').hash)
                self.assertEqual('3cd32e352410565bb543821fbf856a6d3caad1c4', repository.commit().hash)

                self.assertEqual('1abe25b443e985f93b90d830e4a7e3731336af4d', repository.checkout('3@main').hash)
                self.assertEqual('1abe25b443e985f93b90d830e4a7e3731336af4d', repository.commit().hash)

                self.assertEqual('621652add7fc416099bd2063366cc38ff61afe36', repository.checkout('tag-1').hash)
                self.assertEqual('621652add7fc416099bd2063366cc38ff61afe36', repository.commit().hash)

    def test_no_log(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock:
                self.assertIsNone(local.Git(self.path).commit(identifier='4@main', include_log=False).message)

    def test_alternative_default_branch(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock:
                self.assertEqual(str(local.Git(self.path).find('4@trunk')), '4@main')
                self.assertEqual(str(local.Git(self.path).find('4@master')), '4@main')

class TestGitHub(unittest.TestCase):
    remote = 'https://github.example.com/WebKit/webkit'

    def test_detection(self):
        self.assertEqual(remote.GitHub.is_webserver('https://github.example.com/WebKit/webkit'), True)
        self.assertEqual(remote.GitHub.is_webserver('http://github.example.com/WebKit/webkit'), True)
        self.assertEqual(remote.GitHub.is_webserver('https://svn.example.org/repository/webkit'), False)
        self.assertEqual(remote.GitHub.is_webserver('https://bitbucket.example.com/projects/WebKit/repos/webkit'), False)

    def test_branches(self):
        with mocks.remote.GitHub():
            self.assertEqual(
                remote.GitHub(self.remote).branches,
                ['branch-a', 'branch-b', 'main'],
            )

    def test_tags(self):
        with mocks.remote.GitHub() as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]
            mock.tags['tag-2'] = mock.commits['branch-b'][-1]

            self.assertEqual(
                remote.GitHub(self.remote).tags,
                ['tag-1', 'tag-2'],
            )

    def test_scm_type(self):
        self.assertFalse(remote.GitHub(self.remote).is_svn)
        self.assertTrue(remote.GitHub(self.remote).is_git)

    def test_commit_revision(self):
        with mocks.remote.GitHub():
            self.assertEqual('1@main', str(remote.GitHub(self.remote).commit(hash='9b8311f2')))
            self.assertEqual('2@main', str(remote.GitHub(self.remote).commit(hash='fff83bb2')))
            self.assertEqual('2.1@branch-a', str(remote.GitHub(self.remote).commit(hash='a30ce849')))
            self.assertEqual('3@main', str(remote.GitHub(self.remote).commit(hash='1abe25b4')))
            self.assertEqual('2.2@branch-b', str(remote.GitHub(self.remote).commit(hash='3cd32e35')))
            self.assertEqual('4@main', str(remote.GitHub(self.remote).commit(hash='bae5d1e9')))
            self.assertEqual('2.2@branch-a', str(remote.GitHub(self.remote).commit(hash='621652ad')))
            self.assertEqual('2.3@branch-b', str(remote.GitHub(self.remote).commit(hash='790725a6')))

    def test_commit_from_branch(self):
        with mocks.remote.GitHub():
            self.assertEqual('4@main', str(remote.GitHub(self.remote).commit(branch='main')))
            self.assertEqual('2.2@branch-a', str(remote.GitHub(self.remote).commit(branch='branch-a')))
            self.assertEqual('2.3@branch-b', str(remote.GitHub(self.remote).commit(branch='branch-b')))

    def test_identifier(self):
        with mocks.remote.GitHub():
            self.assertEqual(
                '9b8311f25a77ba14923d9d5a6532103f54abefcb',
                remote.GitHub(self.remote).commit(identifier='1@main').hash,
            )
            self.assertEqual(
                'fff83bb2d9171b4d9196e977eb0508fd57e7a08d',
                remote.GitHub(self.remote).commit(identifier='2@main').hash,
            )
            self.assertEqual(
                'a30ce8494bf1ac2807a69844f726be4a9843ca55',
                remote.GitHub(self.remote).commit(identifier='2.1@branch-a').hash,
            )
            self.assertEqual(
                '1abe25b443e985f93b90d830e4a7e3731336af4d',
                remote.GitHub(self.remote).commit(identifier='3@main').hash,
            )
            self.assertEqual(
                '3cd32e352410565bb543821fbf856a6d3caad1c4',
                remote.GitHub(self.remote).commit(identifier='2.2@branch-b').hash,
            )
            self.assertEqual(
                'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                remote.GitHub(self.remote).commit(identifier='4@main').hash,
            )
            self.assertEqual(
                '621652add7fc416099bd2063366cc38ff61afe36',
                remote.GitHub(self.remote).commit(identifier='2.2@branch-a').hash,
            )
            self.assertEqual(
                '790725a6d79e28db2ecdde29548d2262c0bd059d',
                remote.GitHub(self.remote).commit(identifier='2.3@branch-b').hash,
            )

    def test_non_cannonical_identifiers(self):
        with mocks.remote.GitHub():
            self.assertEqual('2@main', str(remote.GitHub(self.remote).commit(identifier='0@branch-a')))
            self.assertEqual('1@main', str(remote.GitHub(self.remote).commit(identifier='-1@branch-a')))

            self.assertEqual('2@main', str(remote.GitHub(self.remote).commit(identifier='0@branch-b')))
            self.assertEqual('1@main', str(remote.GitHub(self.remote).commit(identifier='-1@branch-b')))

    def test_tag(self):
        with mocks.remote.GitHub() as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]

            self.assertEqual(
                '621652add7fc416099bd2063366cc38ff61afe36',
                remote.GitHub(self.remote).commit(tag='tag-1').hash,
            )

    def test_no_log(self):
        with mocks.remote.GitHub():
            self.assertIsNone(remote.GitHub(self.remote).commit(identifier='4@main', include_log=False).message)

    def test_alternative_default_branch(self):
        with mocks.remote.GitHub():
            self.assertEqual(str(remote.GitHub(self.remote).find('4@trunk')), '4@main')
            self.assertEqual(str(remote.GitHub(self.remote).find('4@master')), '4@main')


class TestBitBucket(unittest.TestCase):
    remote = 'https://bitbucket.example.com/projects/WEBKIT/repos/webkit'

    def test_detection(self):
        self.assertEqual(remote.BitBucket.is_webserver('https://bitbucket.example.com/projects/WebKit/repos/webkit'), True)
        self.assertEqual(remote.BitBucket.is_webserver('http://bitbucket.example.com/projects/WebKit/repos/webkit'), True)
        self.assertEqual(remote.BitBucket.is_webserver('https://svn.example.org/repository/webkit'), False)
        self.assertEqual(remote.BitBucket.is_webserver('https://github.example.com/WebKit/webkit'), False)

    def test_branches(self):
        with mocks.remote.BitBucket():
            self.assertEqual(
                remote.BitBucket(self.remote).branches,
                ['branch-a', 'branch-b', 'main'],
            )

    def test_tags(self):
        with mocks.remote.BitBucket() as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]
            mock.tags['tag-2'] = mock.commits['branch-b'][-1]

            self.assertEqual(
                remote.BitBucket(self.remote).tags,
                ['tag-1', 'tag-2'],
            )

    def test_scm_type(self):
        self.assertFalse(remote.BitBucket(self.remote).is_svn)
        self.assertTrue(remote.BitBucket(self.remote).is_git)

    def test_commit_revision(self):
        with mocks.remote.BitBucket():
            self.assertEqual('1@main', str(remote.BitBucket(self.remote).commit(hash='9b8311f2')))
            self.assertEqual('2@main', str(remote.BitBucket(self.remote).commit(hash='fff83bb2')))
            self.assertEqual('2.1@branch-a', str(remote.BitBucket(self.remote).commit(hash='a30ce849')))
            self.assertEqual('3@main', str(remote.BitBucket(self.remote).commit(hash='1abe25b4')))
            self.assertEqual('2.2@branch-b', str(remote.BitBucket(self.remote).commit(hash='3cd32e35')))
            self.assertEqual('4@main', str(remote.BitBucket(self.remote).commit(hash='bae5d1e9')))
            self.assertEqual('2.2@branch-a', str(remote.BitBucket(self.remote).commit(hash='621652ad')))
            self.assertEqual('2.3@branch-b', str(remote.BitBucket(self.remote).commit(hash='790725a6')))

    def test_commit_from_branch(self):
        with mocks.remote.BitBucket():
            self.assertEqual('4@main', str(remote.BitBucket(self.remote).commit(branch='main')))
            self.assertEqual('2.2@branch-a', str(remote.BitBucket(self.remote).commit(branch='branch-a')))
            self.assertEqual('2.3@branch-b', str(remote.BitBucket(self.remote).commit(branch='branch-b')))

    def test_identifier(self):
        with mocks.remote.BitBucket():
            self.assertEqual(
                '9b8311f25a77ba14923d9d5a6532103f54abefcb',
                remote.BitBucket(self.remote).commit(identifier='1@main').hash,
            )
            self.assertEqual(
                'fff83bb2d9171b4d9196e977eb0508fd57e7a08d',
                remote.BitBucket(self.remote).commit(identifier='2@main').hash,
            )
            self.assertEqual(
                'a30ce8494bf1ac2807a69844f726be4a9843ca55',
                remote.BitBucket(self.remote).commit(identifier='2.1@branch-a').hash,
            )
            self.assertEqual(
                '1abe25b443e985f93b90d830e4a7e3731336af4d',
                remote.BitBucket(self.remote).commit(identifier='3@main').hash,
            )
            self.assertEqual(
                '3cd32e352410565bb543821fbf856a6d3caad1c4',
                remote.BitBucket(self.remote).commit(identifier='2.2@branch-b').hash,
            )
            self.assertEqual(
                'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                remote.BitBucket(self.remote).commit(identifier='4@main').hash,
            )
            self.assertEqual(
                '621652add7fc416099bd2063366cc38ff61afe36',
                remote.BitBucket(self.remote).commit(identifier='2.2@branch-a').hash,
            )
            self.assertEqual(
                '790725a6d79e28db2ecdde29548d2262c0bd059d',
                remote.BitBucket(self.remote).commit(identifier='2.3@branch-b').hash,
            )

    def test_non_cannonical_identifiers(self):
        with mocks.remote.BitBucket():
            self.assertEqual('2@main', str(remote.BitBucket(self.remote).commit(identifier='0@branch-a')))
            self.assertEqual('1@main', str(remote.BitBucket(self.remote).commit(identifier='-1@branch-a')))

            self.assertEqual('2@main', str(remote.BitBucket(self.remote).commit(identifier='0@branch-b')))
            self.assertEqual('1@main', str(remote.BitBucket(self.remote).commit(identifier='-1@branch-b')))

    def test_tag(self):
        with mocks.remote.BitBucket() as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]

            self.assertEqual(
                '621652add7fc416099bd2063366cc38ff61afe36',
                remote.BitBucket(self.remote).commit(tag='tag-1').hash,
            )

    def test_no_log(self):
        with mocks.remote.BitBucket():
            self.assertIsNone(remote.BitBucket(self.remote).commit(identifier='4@main', include_log=False).message)

    def test_alternative_default_branch(self):
        with mocks.remote.BitBucket():
            self.assertEqual(str(remote.BitBucket(self.remote).find('4@trunk')), '4@main')
            self.assertEqual(str(remote.BitBucket(self.remote).find('4@master')), '4@main')
