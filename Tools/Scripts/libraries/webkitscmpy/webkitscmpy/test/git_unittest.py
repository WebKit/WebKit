# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
import time

from datetime import datetime
from webkitcorepy import run, testing, LoggerCapture, OutputCapture
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import Commit, local, mocks, remote


class TestGit(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestGit, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

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

    def test_url(self):
        with mocks.local.Git(self.path) as repo:
            self.assertEqual(local.Git(self.path).url(), repo.remote)

    def test_remote(self):
        with mocks.local.Git(self.path):
            self.assertEqual(local.Git(self.path).remote(), None)

    def test_remote_github(self):
        with mocks.local.Git(self.path, remote='git@github.example.com:WebKit/WebKit.git'):
            self.assertIsInstance(local.Git(self.path).remote(), remote.GitHub)
        with mocks.local.Git(self.path, remote='https://github.example.com/WebKit/WebKit.git'):
            self.assertIsInstance(local.Git(self.path).remote(), remote.GitHub)

    def test_remote_bitbucket(self):
        with mocks.local.Git(self.path, remote='ssh://git@stash.example.com:webkit/webkit.git'):
            self.assertIsInstance(local.Git(self.path).remote(), remote.BitBucket)
        with mocks.local.Git(self.path, remote='http://git@stash.example.com/webkit/webkit.git'):
            self.assertIsInstance(local.Git(self.path).remote(), remote.BitBucket)

    def test_branches(self):
        with mocks.local.Git(self.path):
            self.assertEqual(
                local.Git(self.path).branches,
                ['branch-a', 'branch-b', 'eng/squash-branch', 'main'],
            )

    def test_tags(self):
        with mocks.local.Git(self.path) as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]
            mock.tags['tag-2'] = mock.commits['branch-b'][-1]

            self.assertEqual(
                local.Git(self.path).tags(),
                ['tag-1', 'tag-2'],
            )

            self.assertEqual(
                local.Git(self.path).tags(remote='origin'),
                ['tag-1', 'tag-2'],
            )

    def test_default_branch(self):
        with mocks.local.Git(self.path):
            self.assertEqual(local.Git(self.path).default_branch, 'main')

    def test_scm_type(self):
        with mocks.local.Git(self.path, remote='git@example.org:{}'.format(self.path)), MockTime, LoggerCapture():
            self.assertTrue(local.Git(self.path).is_git)
            self.assertFalse(local.Git(self.path).is_svn)

        with mocks.local.Git(self.path, git_svn=True, remote='git@example.org:{}'.format(self.path)), MockTime, LoggerCapture():
            self.assertTrue(local.Git(self.path).is_git)
            self.assertTrue(local.Git(self.path).is_svn)

    def test_info(self):
        with mocks.local.Git(self.path, remote='git@example.org:mock/repository'), MockTime, LoggerCapture():
            with self.assertRaises(local.Git.Exception):
                self.assertEqual(dict(), local.Git(self.path).info())

        with mocks.local.Git(self.path, git_svn=True, remote='git@example.org:mock/repository'), MockTime:
            self.assertDictEqual(
                {
                    'Path': '.',
                    'Repository Root': 'git@example.org:mock/repository',
                    'URL': 'git@example.org:mock/repository/main',
                    'Revision': '9',
                    'Node Kind': 'directory',
                    'Schedule': 'normal',
                    'Last Changed Author': 'jbedard@apple.com',
                    'Last Changed Rev': '9',
                    'Last Changed Date': datetime.fromtimestamp(1601668000).strftime('%Y-%m-%d %H:%M:%S'),
                }, local.Git(self.path).info(),
            )

    def test_commit_revision(self):
        with mocks.local.Git(self.path), MockTime, LoggerCapture():
            with self.assertRaises(local.Git.Exception):
                self.assertEqual(None, local.Git(self.path).commit(revision=1))

        with mocks.local.Git(self.path, git_svn=True, remote='git@example.org:{}'.format(self.path)), MockTime, LoggerCapture():
            self.assertEqual('1@main', str(local.Git(self.path).commit(revision=1)))
            self.assertEqual('2@main', str(local.Git(self.path).commit(revision=2)))
            self.assertEqual('2.1@branch-a', str(local.Git(self.path).commit(revision=3)))
            self.assertEqual('3@main', str(local.Git(self.path).commit(revision=4)))
            self.assertEqual('2.2@branch-b', str(local.Git(self.path).commit(revision=5)))
            self.assertEqual('2.2@branch-a', str(local.Git(self.path).commit(revision=6)))
            self.assertEqual('2.3@branch-b', str(local.Git(self.path).commit(revision=7)))
            self.assertEqual('4@main', str(local.Git(self.path).commit(revision=8)))

            # Out-of-bounds commit
            with self.assertRaises(local.Git.Exception):
                self.assertEqual(None, local.Git(self.path).commit(revision=10))

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
                self.assertEqual('5@main', str(local.Git(self.path).commit(branch='main')))
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
            with mock, LoggerCapture():
                mock.tags['tag-1'] = mock.commits['branch-a'][-1]

                self.assertEqual(
                    '621652add7fc416099bd2063366cc38ff61afe36',
                    local.Git(self.path).commit(tag='tag-1').hash,
                )

    def test_checkout(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, LoggerCapture():
                mock.tags['tag-1'] = mock.commits['branch-a'][-1]

                repository = local.Git(self.path)
                self.assertEqual('d8bce26fa65c6fc8f39c17927abb77f69fab82fc', repository.commit().hash)
                self.assertEqual('3cd32e352410565bb543821fbf856a6d3caad1c4', repository.checkout('3cd32e3524').hash)
                self.assertEqual('3cd32e352410565bb543821fbf856a6d3caad1c4', repository.commit().hash)

                self.assertEqual('1abe25b443e985f93b90d830e4a7e3731336af4d', repository.checkout('3@main').hash)
                self.assertEqual('1abe25b443e985f93b90d830e4a7e3731336af4d', repository.commit().hash)

                self.assertEqual('621652add7fc416099bd2063366cc38ff61afe36', repository.checkout('tag-1').hash)
                self.assertEqual('621652add7fc416099bd2063366cc38ff61afe36', repository.commit().hash)

    def test_no_log(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, LoggerCapture():
                self.assertIsNone(local.Git(self.path).commit(identifier='4@main', include_log=False).message)

    def test_alternative_default_branch(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock:
                self.assertEqual(str(local.Git(self.path).find('4@trunk')), '4@main')
                self.assertEqual(str(local.Git(self.path).find('4@master')), '4@main')

    def test_no_identifier(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock:
                self.assertIsNone(local.Git(self.path).find('main', include_identifier=False).identifier)

    def test_order(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, LoggerCapture():
                self.assertEqual(0, local.Git(self.path).commit(hash='bae5d1e90999').order)
                self.assertEqual(1, local.Git(self.path).commit(hash='d8bce26fa65c').order)

    def test_commits(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, LoggerCapture():
                git = local.Git(self.path)
                self.assertEqual(Commit.Encoder().default([
                    git.commit(hash='bae5d1e9'),
                    git.commit(hash='1abe25b4'),
                    git.commit(hash='fff83bb2'),
                ]), Commit.Encoder().default(list(git.commits(begin=dict(hash='9b8311f2'), end=dict(hash='bae5d1e9')))))

    def test_commits_branch(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, LoggerCapture():
                git = local.Git(self.path)
                self.assertEqual(Commit.Encoder().default([
                    git.commit(hash='621652ad'),
                    git.commit(hash='a30ce849'),
                    git.commit(hash='fff83bb2'),
                ]), Commit.Encoder().default(list(git.commits(begin=dict(argument='9b8311f2'), end=dict(argument='621652ad')))))

    def test_log(self):
        with mocks.local.Git(self.path, git_svn=True):
            self.assertEqual(
                run([
                    local.Git.executable(), 'log', '--format=fuller', '--no-decorate', '--date=unix', 'remotes/origin/main...1abe25b4',
                ], cwd=self.path, capture_output=True, encoding='utf-8').stdout,
                '''commit d8bce26fa65c6fc8f39c17927abb77f69fab82fc
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: {time_a}
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: {time_a}

    Patch Series
    git-svn-id: https://svn.example.org/repository/repository/trunk@9 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit bae5d1e90999d4f916a8a15810ccfa43f37a2fd6
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: {time_a}
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: {time_a}

    8th commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@8 268f45cc-cd09-0410-ab3c-d52691b4dbfc
'''.format(
                time_a=1601668000,
                time_b=1601663000,
            ))

    def test_branch_log(self):
        with mocks.local.Git(self.path, git_svn=True):
            self.assertEqual(
                run([
                    local.Git.executable(), 'log', '--format=fuller', '--no-decorate', '--date=unix', 'main..branch-b',
                ], cwd=self.path, capture_output=True, encoding='utf-8').stdout,
                '''commit 790725a6d79e28db2ecdde29548d2262c0bd059d
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: {time_a}
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: {time_a}

    7th commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@7 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit 3cd32e352410565bb543821fbf856a6d3caad1c4
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: {time_b}
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: {time_b}

    5th commit
        Cherry pick
        git-svn-id: https://svn.webkit.org/repository/webkit/trunk@6 268f45cc-cd09-0410-ab3c-d52691b4dbfc
    git-svn-id: https://svn.example.org/repository/repository/trunk@5 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit a30ce8494bf1ac2807a69844f726be4a9843ca55
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: {time_c}
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: {time_c}

    3rd commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@3 268f45cc-cd09-0410-ab3c-d52691b4dbfc
'''.format(
                time_a=1601667000,
                time_b=1601664000,
                time_c=1601662000,
            ))

    def test_cache(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, OutputCapture():
                repo = local.Git(self.path, cached=True)

                self.assertEqual(repo.cache.to_hash(identifier='1@main'), '9b8311f25a77ba14923d9d5a6532103f54abefcb')
                self.assertEqual(repo.cache.to_identifier(hash='d8bce26fa65c'), '5@main')
                self.assertEqual(repo.cache.to_hash(identifier='2.3@branch-b'), '790725a6d79e28db2ecdde29548d2262c0bd059d')
                self.assertEqual(repo.cache.to_hash(identifier='2.1@branch-a'), 'a30ce8494bf1ac2807a69844f726be4a9843ca55')
                self.assertEqual(repo.cache.to_identifier(hash='a30ce8494bf1'), '2.1@branch-a')

                self.assertEqual(repo.cache.to_identifier(hash='badc0dd1f'), None)
                self.assertEqual(repo.cache.to_hash(identifier='6@main'), None)

    def test_revision_cache(self):
        with mocks.local.Git(self.path, git_svn=True), OutputCapture():
            repo = local.Git(self.path, cached=True)

            self.assertEqual(repo.cache.to_revision(identifier='1@main'), 1)
            self.assertEqual(repo.cache.to_identifier(revision='r9'), '5@main')
            self.assertEqual(repo.cache.to_hash(revision='r9'), 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc')

            self.assertEqual(repo.cache.to_identifier(revision=100), None)
            self.assertEqual(repo.cache.to_revision(hash='badc0dd1f'), None)
            self.assertEqual(repo.cache.to_revision(identifier='6@main'), None)

    def test_config(self):
        with mocks.local.Git(self.path, git_svn=True) as m:
            repo = local.Git(self.path)

            self.assertEqual(repo.config()['user.name'], 'Tim Apple')
            self.assertEqual(repo.config()['core.filemode'], 'true')
            self.assertEqual(repo.config()['remote.origin.url'], 'git@example.org:/mock/repository')
            self.assertEqual(repo.config()['svn-remote.svn.url'], 'https://svn.example.org/repository/webkit')
            self.assertEqual(repo.config()['svn-remote.svn.fetch'], 'trunk:refs/remotes/origin/main')
            self.assertEqual(repo.config()['webkitscmpy.history'], 'when-user-owned')

    def test_global_config(self):
        with mocks.local.Git(self.path, git_svn=True), OutputCapture():
            self.assertEqual(local.Git.config()['user.name'], 'Tim Apple')
            self.assertEqual(local.Git.config()['sendemail.transferencoding'], 'base64')

    def test_project_config(self):
        with mocks.local.Git(self.path, git_svn=True):
            project_config = os.path.join(self.path, 'metadata', local.Git.GIT_CONFIG_EXTENSION)
            os.mkdir(os.path.dirname(project_config))
            with open(project_config, 'w') as f:
                f.write('[webkitscmpy]\n')
                f.write('    history = never\n')
                f.write('    test = example\n')

            repo = local.Git(self.path)
            self.assertEqual(repo.config()['webkitscmpy.history'], 'never')
            self.assertEqual(repo.config()['webkitscmpy.test'], 'example')

    def test_modified(self):
        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)

            self.assertEqual(repo.modified(), [])

            mrepo.modified['unstaged-added.txt'] = 'added'
            self.assertEqual(repo.modified(), [])

            mrepo.modified['unstaged-modified.txt'] = 'diff'
            self.assertEqual(repo.modified(), ['unstaged-modified.txt'])

            mrepo.staged['staged-added.txt'] = 'added'
            self.assertEqual(repo.modified(), ['staged-added.txt', 'unstaged-modified.txt'])

            mrepo.staged['staged-modified.txt'] = 'diff'
            self.assertEqual(repo.modified(), ['staged-added.txt', 'staged-modified.txt'])

    def test_modified_no_staged(self):
        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)

            self.assertEqual(repo.modified(staged=False), [])

            mrepo.staged['staged-added.txt'] = 'added'
            self.assertEqual(repo.modified(staged=False), [])

            mrepo.modified['added.txt'] = 'added'
            self.assertEqual(repo.modified(staged=False), [])

            mrepo.modified['modified.txt'] = 'diff'
            self.assertEqual(repo.modified(staged=False), ['modified.txt'])

    def test_modified_staged(self):
        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)

            self.assertEqual(repo.modified(staged=True), [])

            mrepo.modified['unstaged-added.txt'] = 'added'
            self.assertEqual(repo.modified(staged=True), [])

            mrepo.staged['added.txt'] = 'added'
            self.assertEqual(repo.modified(staged=True), ['added.txt'])

            mrepo.staged['modified.txt'] = 'diff'
            self.assertEqual(repo.modified(staged=True), ['added.txt', 'modified.txt'])

    def test_rebase(self):
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertEqual(str(repo.commit(branch='branch-a')), '2.2@branch-a')
            self.assertEqual(repo.rebase(target='main', base='main', head='branch-a', recommit=False), 0)
            self.assertEqual(str(repo.commit(branch='branch-a')), '5.2@branch-a')

    def test_diff_lines(self):
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertEqual(
                ['--- a/ChangeLog', '+++ b/ChangeLog', '@@ -1,0 +1,0 @@', '+ Patch Series'],
                list(repo.diff_lines(base='bae5d1e90999d4f916a8a15810ccfa43f37a2fd6'))
            )

    def test_diff_lines_identifier(self):
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertEqual(
                ['--- a/ChangeLog', '+++ b/ChangeLog', '@@ -1,0 +1,0 @@', '+ 8th commit'],
                list(repo.diff_lines(base='3@main', head='4@main'))
            )

    def test_pull(self):
        with mocks.local.Git(self.path) as mocked, OutputCapture():
            mocked.staged['added.txt'] = 'added'
            self.assertEqual(local.Git(self.path).pull(rebase=True), 0)

    def test_pull_no_stash(self):
        with mocks.local.Git(self.path) as mocked, OutputCapture():
            mocked.staged['added.txt'] = 'added'
            self.assertEqual(local.Git(self.path).pull(), 128)

    def test_source_remotes_default(self):
        with mocks.local.Git(self.path), OutputCapture():
            self.assertEqual(local.Git(self.path).source_remotes(), ['origin'])

    def test_source_remotes_single(self):
        with mocks.local.Git(self.path, remotes={
            'origin': 'git@github.example.com:WebKit/WebKit.git',
            'fork': 'git@github.example.com:Contributor/WebKit.git',
        }), OutputCapture():
            project_config = os.path.join(self.path, 'metadata', local.Git.GIT_CONFIG_EXTENSION)
            os.mkdir(os.path.dirname(project_config))
            with open(project_config, 'w') as f:
                f.write('[webkitscmpy "remotes"]\n')
                f.write('    origin = git@github.example.com:WebKit/WebKit.git\n')
                f.write('    security = git@github.example.com:WebKit/WebKit-security.git\n')

            self.assertEqual(local.Git(self.path).source_remotes(), ['origin'])

    def test_source_remotes_multiple(self):
        with mocks.local.Git(self.path, remotes={
            'origin': 'git@github.example.com:WebKit/WebKit.git',
            'fork': 'git@github.example.com:Contributor/WebKit.git',
            'security': 'git@github.example.com:WebKit/WebKit-security.git',
            'security-fork': 'git@github.example.com:Contributor/WebKit-security.git',
        }), OutputCapture():
            project_config = os.path.join(self.path, 'metadata', local.Git.GIT_CONFIG_EXTENSION)
            os.mkdir(os.path.dirname(project_config))
            with open(project_config, 'w') as f:
                f.write('[webkitscmpy "remotes"]\n')
                f.write('    origin = git@github.example.com:WebKit/WebKit.git\n')
                f.write('    security = git@github.example.com:WebKit/WebKit-security.git\n')

            self.assertEqual(local.Git(self.path).source_remotes(), ['origin', 'security'])
            self.assertEqual(
                local.Git(self.path).source_remotes(personal=True),
                ['origin', 'security', 'fork', 'security-fork'],
            )

    def test_files_changed(self):
        with mocks.local.Git(self.path), OutputCapture():
            self.assertEqual(
                local.Git(self.path).files_changed('4@main'),
                ['Source/main.cpp', 'Source/main.h'],
            )

    def test_merge_base(self):
        with mocks.local.Git(self.path), OutputCapture():
            self.assertEqual(
                str(local.Git(self.path).merge_base('main', 'branch-b')),
                '2@main',
            )
            self.assertEqual(
                str(local.Git(self.path).merge_base('branch-a', 'branch-b')),
                '2@main',
            )


class TestGitHub(testing.TestCase):
    remote = 'https://github.example.com/WebKit/WebKit'

    def test_detection(self):
        self.assertEqual(remote.GitHub.is_webserver('https://github.example.com/WebKit/webkit'), True)
        self.assertEqual(remote.GitHub.is_webserver('http://github.example.com/WebKit/webkit'), True)
        self.assertEqual(remote.GitHub.is_webserver('https://svn.example.org/repository/webkit'), False)
        self.assertEqual(remote.GitHub.is_webserver('https://bitbucket.example.com/projects/WebKit/repos/webkit'), False)

    def test_branches(self):
        with mocks.remote.GitHub():
            self.assertEqual(
                remote.GitHub(self.remote).branches,
                ['branch-a', 'branch-b', 'eng/squash-branch', 'main'],
            )

    def test_tags(self):
        with mocks.remote.GitHub() as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]
            mock.tags['tag-2'] = mock.commits['branch-b'][-1]

            self.assertEqual(
                remote.GitHub(self.remote).tags(),
                ['tag-1', 'tag-2'],
            )

    def test_scm_type(self):
        self.assertFalse(remote.GitHub(self.remote).is_svn)
        self.assertTrue(remote.GitHub(self.remote).is_git)

    def test_commit_hash(self):
        with mocks.remote.GitHub():
            self.assertEqual('1@main', str(remote.GitHub(self.remote).commit(hash='9b8311f2')))
            self.assertEqual('2@main', str(remote.GitHub(self.remote).commit(hash='fff83bb2')))
            self.assertEqual('2.1@branch-a', str(remote.GitHub(self.remote).commit(hash='a30ce849')))
            self.assertEqual('3@main', str(remote.GitHub(self.remote).commit(hash='1abe25b4')))
            self.assertEqual('2.2@branch-b', str(remote.GitHub(self.remote).commit(hash='3cd32e35')))
            self.assertEqual('4@main', str(remote.GitHub(self.remote).commit(hash='bae5d1e9')))
            self.assertEqual('2.2@branch-a', str(remote.GitHub(self.remote).commit(hash='621652ad')))
            self.assertEqual('2.3@branch-b', str(remote.GitHub(self.remote).commit(hash='790725a6')))

    def test_commit_revision(self):
        with mocks.remote.GitHub(git_svn=True):
            self.assertEqual(1, remote.GitHub(self.remote).commit(hash='9b8311f2').revision)
            self.assertEqual(2, remote.GitHub(self.remote).commit(hash='fff83bb2').revision)
            self.assertEqual(3, remote.GitHub(self.remote).commit(hash='a30ce849').revision)
            self.assertEqual(4, remote.GitHub(self.remote).commit(hash='1abe25b4').revision)
            self.assertEqual(5, remote.GitHub(self.remote).commit(hash='3cd32e35').revision)
            self.assertEqual(6, remote.GitHub(self.remote).commit(hash='621652ad').revision)
            self.assertEqual(7, remote.GitHub(self.remote).commit(hash='790725a6').revision)
            self.assertEqual(8, remote.GitHub(self.remote).commit(hash='bae5d1e9').revision)

    def test_commit_from_branch(self):
        with mocks.remote.GitHub():
            self.assertEqual('5@main', str(remote.GitHub(self.remote).commit(branch='main')))
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

    def test_no_identifier(self):
        with mocks.remote.GitHub():
            self.assertIsNone(remote.GitHub(self.remote).find('main', include_identifier=False).identifier)

    def test_order(self):
        with mocks.remote.GitHub():
            self.assertEqual(0, remote.GitHub(self.remote).commit(hash='bae5d1e90999').order)
            self.assertEqual(1, remote.GitHub(self.remote).commit(hash='d8bce26fa65c').order)

    def test_id(self):
        self.assertEqual(remote.GitHub(self.remote).id, 'webkit')

    def test_commits(self):
        with mocks.remote.GitHub():
            git = remote.GitHub(self.remote)
            self.assertEqual(Commit.Encoder().default([
                git.commit(hash='bae5d1e9'),
                git.commit(hash='1abe25b4'),
                git.commit(hash='fff83bb2'),
                git.commit(hash='9b8311f2'),
            ]), Commit.Encoder().default(list(git.commits(begin=dict(hash='9b8311f2'), end=dict(hash='bae5d1e9')))))

    def test_commits_branch(self):
        with mocks.remote.GitHub():
            git = remote.GitHub(self.remote)
            self.assertEqual(Commit.Encoder().default([
                git.commit(hash='621652ad'),
                git.commit(hash='a30ce849'),
                git.commit(hash='fff83bb2'),
                git.commit(hash='9b8311f2'),
            ]), Commit.Encoder().default(list(git.commits(begin=dict(argument='9b8311f2'), end=dict(argument='621652ad')))))

    def test_commits_branch_ref(self):
        with mocks.remote.GitHub():
            git = remote.GitHub(self.remote)
            self.assertEqual(
                ['2.3@branch-b', '2.2@branch-b', '2.1@branch-b'],
                [str(commit) for commit in git.commits(begin=dict(argument='a30ce849'), end=dict(argument='branch-b'))],
            )

    def test_files_changed(self):
        with mocks.remote.GitHub():
            self.assertEqual(
                remote.GitHub(self.remote).files_changed('4@main'),
                ['Source/main.cpp', 'Source/main.h'],
            )


class TestBitBucket(testing.TestCase):
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
                ['branch-a', 'branch-b', 'eng/squash-branch', 'main'],
            )

    def test_tags(self):
        with mocks.remote.BitBucket() as mock:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]
            mock.tags['tag-2'] = mock.commits['branch-b'][-1]

            self.assertEqual(
                remote.BitBucket(self.remote).tags(),
                ['tag-1', 'tag-2'],
            )

    def test_scm_type(self):
        self.assertFalse(remote.BitBucket(self.remote).is_svn)
        self.assertTrue(remote.BitBucket(self.remote).is_git)

    def test_commit_hash(self):
        with mocks.remote.BitBucket():
            self.assertEqual('1@main', str(remote.BitBucket(self.remote).commit(hash='9b8311f2')))
            self.assertEqual('2@main', str(remote.BitBucket(self.remote).commit(hash='fff83bb2')))
            self.assertEqual('2.1@branch-a', str(remote.BitBucket(self.remote).commit(hash='a30ce849')))
            self.assertEqual('3@main', str(remote.BitBucket(self.remote).commit(hash='1abe25b4')))
            self.assertEqual('2.2@branch-b', str(remote.BitBucket(self.remote).commit(hash='3cd32e35')))
            self.assertEqual('4@main', str(remote.BitBucket(self.remote).commit(hash='bae5d1e9')))
            self.assertEqual('2.2@branch-a', str(remote.BitBucket(self.remote).commit(hash='621652ad')))
            self.assertEqual('2.3@branch-b', str(remote.BitBucket(self.remote).commit(hash='790725a6')))

    def test_commit_revision(self):
        with mocks.remote.BitBucket(git_svn=True):
            self.assertEqual(1, remote.BitBucket(self.remote).commit(hash='9b8311f2').revision)
            self.assertEqual(2, remote.BitBucket(self.remote).commit(hash='fff83bb2').revision)
            self.assertEqual(3, remote.BitBucket(self.remote).commit(hash='a30ce849').revision)
            self.assertEqual(4, remote.BitBucket(self.remote).commit(hash='1abe25b4').revision)
            self.assertEqual(5, remote.BitBucket(self.remote).commit(hash='3cd32e35').revision)
            self.assertEqual(6, remote.BitBucket(self.remote).commit(hash='621652ad').revision)
            self.assertEqual(7, remote.BitBucket(self.remote).commit(hash='790725a6').revision)
            self.assertEqual(8, remote.BitBucket(self.remote).commit(hash='bae5d1e9').revision)

    def test_commit_from_branch(self):
        with mocks.remote.BitBucket():
            self.assertEqual('5@main', str(remote.BitBucket(self.remote).commit(branch='main')))
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

    def test_no_identifier(self):
        with mocks.remote.BitBucket():
            self.assertIsNone(remote.BitBucket(self.remote).find('main', include_identifier=False).identifier)

    def test_order(self):
        with mocks.remote.BitBucket():
            self.assertEqual(0, remote.BitBucket(self.remote).commit(hash='bae5d1e90999').order)
            self.assertEqual(1, remote.BitBucket(self.remote).commit(hash='d8bce26fa65c').order)

    def test_id(self):
        self.assertEqual(remote.BitBucket(self.remote).id, 'webkit')

    def test_files_changed(self):
        with mocks.remote.BitBucket():
            self.assertEqual(
                remote.BitBucket(self.remote).files_changed('4@main'),
                ['Source/main.cpp', 'Source/main.h'],
            )
