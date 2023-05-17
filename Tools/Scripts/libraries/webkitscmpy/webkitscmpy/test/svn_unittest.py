# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

from datetime import datetime, timedelta
from webkitcorepy import OutputCapture, LoggerCapture, testing
from webkitscmpy import Commit, local, mocks, remote


class TestLocalSvn(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestLocalSvn, self).setUp()
        os.mkdir(os.path.join(self.path, '.svn'))

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

    def test_url(self):
        with mocks.local.Svn(self.path) as repo:
            self.assertEqual(local.Svn(self.path).url(), repo.remote)

    def test_remote(self):
        with mocks.local.Svn(self.path) as repo:
            self.assertIsInstance(local.Svn(self.path).remote(), remote.Svn)

    def test_branches(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(
                local.Svn(self.path).branches,
                ['trunk', 'branch-a', 'branch-b'],
            )

    def test_tags(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(
                local.Svn(self.path).tags(),
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
                    u'Working Copy Root Path': self.path,
                    u'Repository Root': u'https://svn.mock.org/repository/repository',
                    u'URL': u'https://svn.mock.org/repository/repository/trunk',
                    u'Relative URL':  u'^/trunk',
                    u'Revision': u'6',
                    u'NodeKind': u'directory',
                    u'Schedule': u'normal',
                    u'Last Changed Author': u'jbedard@apple.com',
                    u'Last Changed Rev': u'6',
                    u'Last Changed Date': datetime.utcfromtimestamp(1601665100).strftime('%Y-%m-%d %H:%M:%S 0000 (%a, %d %b %Y)'),
                }, local.Svn(self.path).info(),
            )

    def test_commit_revision(self):
        with mocks.local.Svn(self.path), LoggerCapture():
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
                self.assertEqual(None, local.Svn(self.path).commit(revision=11))

    def test_commit_from_branch(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            self.assertEqual('4@trunk', str(local.Svn(self.path).commit(branch='trunk')))
            self.assertEqual('2.2@branch-a', str(local.Svn(self.path).commit(branch='branch-a')))
            self.assertEqual('2.3@branch-b', str(local.Svn(self.path).commit(branch='branch-b')))

    def test_identifier(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            self.assertEqual(1, local.Svn(self.path).commit(identifier='1@trunk').revision)
            self.assertEqual(2, local.Svn(self.path).commit(identifier='2@trunk').revision)
            self.assertEqual(3, local.Svn(self.path).commit(identifier='2.1@branch-a').revision)
            self.assertEqual(4, local.Svn(self.path).commit(identifier='3@trunk').revision)
            self.assertEqual(5, local.Svn(self.path).commit(identifier='2.2@branch-b').revision)
            self.assertEqual(6, local.Svn(self.path).commit(identifier='4@trunk').revision)
            self.assertEqual(7, local.Svn(self.path).commit(identifier='2.2@branch-a').revision)
            self.assertEqual(8, local.Svn(self.path).commit(identifier='2.3@branch-b').revision)

    def test_non_cannonical_identifiers(self):
        with mocks.local.Svn(self.path), LoggerCapture():
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
        with mocks.local.Svn(self.path) as mock_repo, OutputCapture():
            self.assertEqual('4@trunk', str(local.Svn(self.path).commit()))

            mock_repo.connected = False
            commit = local.Svn(self.path).commit()
            self.assertEqual('4@trunk', str(commit))

            with self.assertRaises(local.Svn.Exception):
                local.Svn(self.path).commit(revision=3)

    def test_tag(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            self.assertEqual(9, local.Svn(self.path).commit(tag='tag-1').revision)

    def test_tag_previous(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            self.assertEqual(7, local.Svn(self.path).commit(identifier='2.2@tags/tag-1').revision)

    def test_checkout(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            repository = local.Svn(self.path)

            self.assertEqual(6, repository.commit().revision)
            self.assertEqual(5, repository.checkout('r5').revision)
            self.assertEqual(5, repository.commit().revision)

            self.assertEqual(4, repository.checkout('3@trunk').revision)
            self.assertEqual(4, repository.commit().revision)

            self.assertEqual(9, repository.checkout('tag-1').revision)
            self.assertEqual(9, repository.commit().revision)

    def test_no_log(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            self.assertIsNone(local.Svn(self.path).commit(identifier='4@trunk', include_log=False).message)

    def test_alternative_default_branch(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            self.assertEqual(str(local.Svn(self.path).find('4@main')), '4@trunk')
            self.assertEqual(str(local.Svn(self.path).find('4@master')), '4@trunk')

    def test_no_identifier(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            self.assertIsNone(local.Svn(self.path).find('trunk', include_identifier=False).identifier)

    def test_commits(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            svn = local.Svn(self.path)
            self.assertEqual(Commit.Encoder().default([
                svn.commit(revision='r6'),
                svn.commit(revision='r4'),
                svn.commit(revision='r2'),
                svn.commit(revision='r1'),
            ]), Commit.Encoder().default(list(svn.commits(begin=dict(revision='r1'), end=dict(revision='r6')))))

    def test_commits_branch(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            svn = local.Svn(self.path)
            self.assertEqual(Commit.Encoder().default([
                svn.commit(revision='r7'),
                svn.commit(revision='r3'),
                svn.commit(revision='r2'),
                svn.commit(revision='r1'),
            ]), Commit.Encoder().default(list(svn.commits(begin=dict(argument='r1'), end=dict(argument='r7')))))

    def test_revision_cache(self):
        with mocks.local.Svn(self.path), LoggerCapture():
            svn = local.Svn(self.path)
            self.assertEqual(svn.cache.to_revision(identifier='1@trunk'), 1)
            self.assertEqual(svn.cache.to_revision(identifier='2.1@branch-a'), 3)
            self.assertEqual(svn.cache.to_revision(identifier='2.-1@branch-a'), 1)
            self.assertEqual(svn.cache.to_identifier(revision='r6'), '4@trunk')
            self.assertEqual(svn.cache.to_identifier(revision='r3', branch='branch-a'), '2.1@branch-a')

            self.assertEqual(svn.cache.to_identifier(revision=100), None)
            self.assertEqual(svn.cache.to_revision(hash='badc0dd1f'), None)
            self.assertEqual(svn.cache.to_revision(identifier='6@trunk'), None)

class TestRemoteSvn(testing.TestCase):
    remote = 'https://svn.example.org/repository/webkit'

    def test_detection(self):
        self.assertEqual(remote.Svn.is_webserver('https://svn.example.org/repository/webkit'), True)
        self.assertEqual(remote.Svn.is_webserver('http://svn.example.org/repository/webkit'), True)
        self.assertEqual(remote.Svn.is_webserver('https://github.example.org/WebKit/webkit'), False)
        self.assertEqual(remote.GitHub.is_webserver('https://bitbucket.example.com/projects/WebKit/repos/webkit'), False)

    def test_branches(self):
        with mocks.remote.Svn():
            self.assertEqual(
                remote.Svn(self.remote).branches,
                ['trunk', 'branch-a', 'branch-b'],
            )

    def test_tags(self):
        with mocks.remote.Svn():
            self.assertEqual(
                remote.Svn(self.remote).tags(),
                ['tag-1', 'tag-2'],
            )

    def test_scm_type(self):
        self.assertTrue(remote.Svn(self.remote).is_svn)
        self.assertFalse(remote.Svn(self.remote).is_git)

    def test_info(self):
        with mocks.remote.Svn():
            self.assertDictEqual({
                'Last Changed Author': 'jbedard@apple.com',
                'Last Changed Date': datetime.utcfromtimestamp(1601665100 - timedelta(hours=7).seconds).strftime('%Y-%m-%d %H:%M:%S'),
                'Last Changed Rev': '6',
                'Revision': 10,
            }, remote.Svn(self.remote).info())

    def test_commit_revision(self):
        with mocks.remote.Svn():
            self.assertEqual('1@trunk', str(remote.Svn(self.remote).commit(revision=1)))
            self.assertEqual('2@trunk', str(remote.Svn(self.remote).commit(revision=2)))
            self.assertEqual('2.1@branch-a', str(remote.Svn(self.remote).commit(revision=3)))
            self.assertEqual('3@trunk', str(remote.Svn(self.remote).commit(revision=4)))
            self.assertEqual('2.2@branch-b', str(remote.Svn(self.remote).commit(revision=5)))
            self.assertEqual('4@trunk', str(remote.Svn(self.remote).commit(revision=6)))
            self.assertEqual('2.2@branch-a', str(remote.Svn(self.remote).commit(revision=7)))
            self.assertEqual('2.3@branch-b', str(remote.Svn(self.remote).commit(revision=8)))

            # Out-of-bounds commit
            with self.assertRaises(remote.Svn.Exception):
                self.assertEqual(None, remote.Svn(self.remote).commit(revision=11))

    def test_commit_from_branch(self):
        with mocks.remote.Svn():
            self.assertEqual('4@trunk', str(remote.Svn(self.remote).commit(branch='trunk')))
            self.assertEqual('2.2@branch-a', str(remote.Svn(self.remote).commit(branch='branch-a')))
            self.assertEqual('2.3@branch-b', str(remote.Svn(self.remote).commit(branch='branch-b')))

    def test_identifier(self):
        with mocks.remote.Svn():
            self.assertEqual(1, remote.Svn(self.remote).commit(identifier='1@trunk').revision)
            self.assertEqual(2, remote.Svn(self.remote).commit(identifier='2@trunk').revision)
            self.assertEqual(3, remote.Svn(self.remote).commit(identifier='2.1@branch-a').revision)
            self.assertEqual(4, remote.Svn(self.remote).commit(identifier='3@trunk').revision)
            self.assertEqual(5, remote.Svn(self.remote).commit(identifier='2.2@branch-b').revision)
            self.assertEqual(6, remote.Svn(self.remote).commit(identifier='4@trunk').revision)
            self.assertEqual(7, remote.Svn(self.remote).commit(identifier='2.2@branch-a').revision)
            self.assertEqual(8, remote.Svn(self.remote).commit(identifier='2.3@branch-b').revision)

    def test_non_cannonical_identifiers(self):
        with mocks.remote.Svn():
            self.assertEqual('2@trunk', str(remote.Svn(self.remote).commit(identifier='0@branch-a')))
            self.assertEqual('1@trunk', str(remote.Svn(self.remote).commit(identifier='-1@branch-a')))

            self.assertEqual('2@trunk', str(remote.Svn(self.remote).commit(identifier='0@branch-b')))
            self.assertEqual('1@trunk', str(remote.Svn(self.remote).commit(identifier='-1@branch-b')))

    def test_tag(self):
        with mocks.remote.Svn():
            self.assertEqual(9, remote.Svn(self.remote).commit(tag='tag-1').revision)

    def test_tag_previous(self):
        with mocks.remote.Svn():
            self.assertEqual(7, remote.Svn(self.remote).commit(identifier='2.2@tags/tag-1').revision)

    def test_no_log(self):
        with mocks.remote.Svn():
            self.assertIsNone(remote.Svn(self.remote).commit(identifier='4@trunk', include_log=False).message)

    def test_alternative_default_branch(self):
        with mocks.remote.Svn():
            self.assertEqual(str(remote.Svn(self.remote).find('4@main')), '4@trunk')
            self.assertEqual(str(remote.Svn(self.remote).find('4@master')), '4@trunk')

    def test_no_identifier(self):
        with mocks.remote.Svn():
            self.assertIsNone(remote.Svn(self.remote).find('trunk', include_identifier=False).identifier)

    def test_id(self):
        self.assertEqual(remote.Svn(self.remote).id, 'webkit')

    def test_commits(self):
        self.maxDiff = None
        with mocks.remote.Svn():
            svn = remote.Svn(self.remote)
            self.assertEqual(Commit.Encoder().default([
                svn.commit(revision='r6'),
                svn.commit(revision='r4'),
                svn.commit(revision='r2'),
                svn.commit(revision='r1'),
            ]), Commit.Encoder().default(list(svn.commits(begin=dict(revision='r1'), end=dict(revision='r6')))))

    def test_commits_branch(self):
        with mocks.remote.Svn(), OutputCapture():
            svn = remote.Svn(self.remote)
            self.assertEqual(Commit.Encoder().default([
                svn.commit(revision='r7'),
                svn.commit(revision='r3'),
                svn.commit(revision='r2'),
                svn.commit(revision='r1'),
            ]), Commit.Encoder().default(list(svn.commits(begin=dict(argument='r1'), end=dict(argument='r7')))))

    def test_checkout_url(self):
        self.assertEqual(remote.Svn(self.remote).checkout_url(), 'https://svn.example.org/repository/webkit/trunk')
        self.assertEqual(remote.Svn(self.remote).checkout_url(http=True), 'https://svn.example.org/repository/webkit/trunk')
        with self.assertRaises(ValueError):
            remote.Svn(self.remote).checkout_url(ssh=True)
        with self.assertRaises(ValueError):
            remote.Svn(self.remote).checkout_url(http=True, ssh=True)
