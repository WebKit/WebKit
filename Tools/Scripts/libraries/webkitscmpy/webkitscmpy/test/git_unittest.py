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

import copy
import json
import os
import shutil
import subprocess
import time
from datetime import datetime, timezone
from unittest.mock import patch

from webkitcorepy import LoggerCapture, NestedFuzzyDict, OutputCapture, run, testing, mocks as wkmocks
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import Commit, Contributor, local, mocks, remote
from webkitscmpy.test.cache_oracle import CacheOracle


class TestGit(CacheOracle, testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestGit, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    def _prepare_fetch_head_commit(self, mock_git):
        default_branch = mock_git.default_branch
        main_history = [copy.deepcopy(commit) for commit in mock_git.commits[default_branch]]
        fetched_commit = copy.deepcopy(main_history[-1])
        truncated_history = main_history[:-1]

        mock_git.commits[default_branch] = [copy.deepcopy(commit) for commit in truncated_history]
        mock_git.head = mock_git.commits[default_branch][-1]
        mock_git.remotes[f'origin/{default_branch}'] = [copy.deepcopy(commit) for commit in truncated_history]

        fetch_history = [copy.deepcopy(commit) for commit in truncated_history]
        for commit in fetch_history:
            commit.branch = 'fetch_head'
        fetched_commit.branch = 'fetch_head'
        fetch_history.append(fetched_commit)
        mock_git.commits['fetch_head'] = fetch_history

        fetched_hash = fetched_commit.hash
        for branch in list(mock_git.commits.keys()):
            if branch in (default_branch, 'fetch_head'):
                continue
            mock_git.commits[branch] = [commit for commit in mock_git.commits[branch] if commit.hash != fetched_hash]

        for remote_branch in list(mock_git.remotes.keys()):
            mock_git.remotes[remote_branch] = [commit for commit in mock_git.remotes[remote_branch] if commit.hash != fetched_hash]

        return fetched_commit

    def test_cached_matches_uncached(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, OutputCapture():
                repo = local.Git(self.path, cached=True)

                for argument in ('1@main', '5@main', '2.3@branch-b', '2.1@branch-a', 'd8bce26fa65c', 'HEAD'):
                    self.assert_matches_uncached(repo, lambda r: r.find(argument))
                for cached in (True, False):
                    with self.assertRaises(Exception):
                        local.Git(self.path, cached=cached).find('badc0dd1f')
                self.assertEqual(self.assert_matches_uncached(repo, lambda r: r.commit(identifier='5@main'))[1], '5@main')
                self.assert_matches_uncached(repo, lambda r: r.commit(hash='a30ce8494bf1'))
                self.assert_matches_uncached(repo, lambda r: r.branch_point())
                self.assert_matches_uncached(repo, lambda r: r.commits(begin=dict(argument='1@main'), end=dict(argument='5@main')))

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

    def test_find_hash_dropped_by_default_branch_rewrite_and_pruned(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            history = [commit.hash for commit in mock_git.commits['main']]
            dropped = 'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6'
            repository = local.Git(self.path)
            self.assertEqual(dropped, history[3])
            self.assertEqual('4@main', str(repository.find(dropped)))

            # Main is reset to 3@main and given 2 new commits, so 4@main is in no branch. The old 5@main also
            # survives, because the mock keeps it on eng/squash-branch
            run([git, 'reset', '--hard', history[2]], cwd=self.path, capture_output=True)
            for message in ('Rewritten commit', 'Another rewritten commit'):
                mock_git.staged['file.txt'] = message
                run([git, 'commit', '-m', message], cwd=self.path, capture_output=True)
            run([git, 'update-ref', 'refs/remotes/origin/main', mock_git.head.hash], cwd=self.path, capture_output=True)
            time.sleep(60)

            for hash, identifier in zip(history[:3], ('1@main', '2@main', '3@main')):
                self.assertEqual(identifier, self.assert_matches_uncached(repository, lambda r: r.find(hash))[1])
            for cached in (repository, local.Git(self.path), local.Git(self.path, cached=False)):
                with self.assertRaises(Exception):
                    cached.find(dropped)

    def test_find_after_git_log_fails_once_it_started(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            repository = local.Git(self.path)
            self.assertEqual('2@main', str(repository.find(mock_git.commits['main'][1].hash)))

            for message in ('First new commit', 'Second new commit'):
                mock_git.staged['file.txt'] = message
                run([git, 'commit', '-m', message], cwd=self.path, capture_output=True)
            run([git, 'update-ref', 'refs/remotes/origin/main', mock_git.head.hash], cwd=self.path, capture_output=True)
            time.sleep(60)

            real_poll = subprocess.Popen.poll
            results = iter([None, 128])

            def poll(process):
                result = real_poll(process)
                # Only the `git log` that walks a branch for the cache, not the one that reads a single commit
                if '--no-replace-objects' in process._args and 'log' in process._args:
                    return next(results, None)
                return result

            # Looking up a new commit on main walks main again, and that walk fails after it started
            with patch.object(subprocess.Popen, 'poll', poll):
                repository.find(mock_git.head.hash)
            self.assertEqual([], list(results), 'the walk of main was not polled twice')

            for number, commit in enumerate(mock_git.commits['main'], 1):
                self.assertEqual(f'{number}@main', self.assert_matches_uncached(repository, lambda r: r.find(commit.hash))[1])
                self.assertEqual(f'{number}@main', self.assert_matches_uncached(local.Git(self.path), lambda r: r.find(commit.hash))[1])

    def _move_off_default_branch(self, mock_git, dropped='bae5d1e9', new_branch='eng/rewritten'):
        """Reset main to before `dropped`, which survives only on `new_branch` (3.1@new_branch)."""
        default_branch = mock_git.default_branch
        main_history = mock_git.commits[default_branch]
        index = [commit.hash[:len(dropped)] for commit in main_history].index(dropped)
        kept = main_history[index - 1]
        moved = copy.deepcopy(main_history[index])
        moved.branch = new_branch
        moved.identifier = 1
        moved.branch_point = index
        mock_git.commits[new_branch] = [copy.deepcopy(kept), moved]

        run([local.Git.executable(), 'reset', '--hard', kept.hash], cwd=self.path, capture_output=True)
        run([local.Git.executable(), 'update-ref', f'refs/remotes/origin/{default_branch}', kept.hash], cwd=self.path, capture_output=True)
        return moved

    def _check_after_default_branch_rewrite(self, call):
        """The cache saw `bae5d1e9` as 4@main, then main was rewritten without it: the cache must not still say so."""
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            repository = local.Git(self.path)
            self.assertEqual('4@main', str(repository.commit(hash='bae5d1e9')))

            moved = self._move_off_default_branch(mock_git)
            self.assertEqual('3.1@eng/rewritten', str(local.Git(self.path, cached=False).commit(hash=moved.hash)))
            self.assert_matches_uncached(repository, call)

    def test_commit_without_identifier_after_default_branch_rewrite(self):
        self._check_after_default_branch_rewrite(lambda r: r.commit(hash='bae5d1e90999d4f916a8a15810ccfa43f37a2fd6', include_identifier=False))

    def test_commit_cached_branch_point_after_default_branch_rewrite(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            repository = local.Git(self.path)
            repository.commit(hash='bae5d1e9')

            moved = self._move_off_default_branch(mock_git)
            mock_git.head = moved
            self.assertTrue(self.assert_matches_uncached(repository, lambda r: r.branch_point())[0].startswith('1abe25b4'))

    def test_commit_cached_hash_still_on_default_branch(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            repository = local.Git(self.path)
            repository.commit(hash='bae5d1e9')

            self._move_off_default_branch(mock_git)
            self.assertEqual('3@main', str(repository.commit(hash='1abe25b4')))
            self.assert_matches_uncached(repository, lambda r: r.commit(hash='1abe25b4'))
            self.assert_matches_uncached(repository, lambda r: r.find('2@main'))

    def _populated_cache_file(self, mock_git):
        """Populate main and eng/foo on one instance, return the path of the cache file it wrote and its content."""
        git = local.Git.executable()
        run([git, 'checkout', '-b', 'eng/foo'], cwd=self.path, capture_output=True)
        mock_git.staged['file.txt'] = 'dev'
        run([git, 'commit', '-m', 'Dev branch commit'], cwd=self.path, capture_output=True)
        repository = local.Git(self.path)
        repository.find('5.1@eng/foo')
        path = os.path.join(repository.common_directory, 'identifiers.json')
        with open(path) as file:
            return path, json.load(file)

    def test_cache_file_truncated(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            path, _ = self._populated_cache_file(mock_git)
            with open(path) as file:
                text = file.read()
            with open(path, 'w') as file:
                file.write(text[:len(text) // 2])

            repository = local.Git(self.path)
            for something in ('3@main', mock_git.commits['main'][2].hash):
                self.assert_matches_uncached(repository, lambda r: r.find(something))
            self.assert_matches_uncached(repository, lambda r: r.commit(hash=mock_git.commits['eng/foo'][-1].hash))

    def test_cache_file_labels_hashes_on_construction(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            self._populated_cache_file(mock_git)
            repository = local.Git(self.path)
            self.assertEqual('3@main', repository.cache.to_identifier(hash=mock_git.commits['main'][2].hash, populate=False))

    def _check_stacked_branch(self, base, top):
        """`top` is stacked on `base`, whose name `top` is a suffix of (or `base` ends in `main`)."""
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            for name in (base, top):
                run([git, 'checkout', '-b', name], cwd=self.path, capture_output=True)
                mock_git.staged['file.txt'] = name
                run([git, 'commit', '-m', f'Commit on {name}'], cwd=self.path, capture_output=True)
            repository = local.Git(self.path)
            repository.find(f'5.1@{base}')
            # Only the commit unique to `top`: the base commit is on both branches, so there is no single right label
            self.assert_matches_uncached(repository, lambda r: r.find(mock_git.head.hash))
            self.assert_matches_uncached(repository, lambda r: r.commit(hash=mock_git.head.hash))

    def test_find_branch_stacked_on_unrelated_branch(self):
        self._check_stacked_branch('eng/base', 'eng/top')

    def test_find_recreated_dev_branch(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            run([git, 'checkout', '-b', 'eng/x'], cwd=self.path, capture_output=True)
            mock_git.staged['file.txt'] = 'old'
            run([git, 'commit', '-m', 'Old commit'], cwd=self.path, capture_output=True)
            old = mock_git.head
            repository = local.Git(self.path)
            repository.find('5.1@eng/x')

            parent = mock_git.commits['main'][2]
            recreated = Commit(
                hash='5ecbea7ed0000000000000000000000000000001',
                author=parent.author,
                timestamp=int(time.time()),
                branch='eng/x',
                message='Recreated commit\n',
                identifier='3.1@eng/x',
            )
            mock_git.commits['eng/x'] = [parent, recreated]
            time.sleep(60)
            self.assert_matches_uncached(repository, lambda r: r.find(recreated.hash))
            self.assert_matches_uncached(repository, lambda r: r.find('3.1@eng/x'))
            # The old commit is gone from the recreated branch
            for something in (repository, local.Git(self.path, cached=False)):
                with self.assertRaises(Exception):
                    something.find(old.hash)

    def test_find_rebased_dev_branch(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            parent = mock_git.commits['main'][2]
            mock_git.commits['eng/foo'] = [parent, Commit(
                hash='0be4a1ed0000000000000000000000000000001',
                author=parent.author,
                timestamp=int(time.time()),
                branch='eng/foo',
                message='Commit before rebase\n',
                identifier='3.1@eng/foo',
            )]
            repository = local.Git(self.path)
            repository.find('3.1@eng/foo')

            tip = mock_git.commits['main'][-1]
            rebased = Commit(
                hash='5eba5ed0000000000000000000000000000001',
                author=parent.author,
                timestamp=int(time.time()),
                branch='eng/foo',
                message='Commit after rebase\n',
                identifier='5.1@eng/foo',
            )
            mock_git.commits['eng/foo'] = [tip, rebased]
            time.sleep(60)
            self.assert_matches_uncached(repository, lambda r: r.find(rebased.hash))
            self.assert_matches_uncached(repository, lambda r: r.find('5.1@eng/foo'))

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
            with mock, LoggerCapture():
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

    def test_commits_empty_range(self):
        for mock in [mocks.local.Git(self.path), mocks.local.Git(self.path, git_svn=True)]:
            with mock, LoggerCapture():
                git = local.Git(self.path)
                self.assertEqual([], list(git.commits(begin={'hash': '9b8311f2'}, end={'hash': '9b8311f2'})))

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

    def test_cache_local_default_branch_behind_remote(self):
        with mocks.local.Git(self.path) as mock, MockTime, LoggerCapture():
            # Local main is one commit behind origin/main, which is what a `git fetch` without updating main leaves
            mock.commits['main'].pop()
            mock.head = mock.commits['main'][-1]

            # Looking up a commit on another branch walks origin/main
            repo = local.Git(self.path)
            self.assertEqual('2.2@branch-a', str(repo.commit(identifier='2.2@branch-a')))
            self.assertEqual('5@main', str(repo.commit(hash='d8bce26fa65c')))

            # Looking up an identifier on main walks local main, which hasn't lost anything on origin/main, so no
            # identifier needs to be forgotten and computed again
            time.sleep(60)
            with patch.object(NestedFuzzyDict, '__setitem__', autospec=True, side_effect=NestedFuzzyDict.__setitem__) as assigned:
                self.assertEqual('4@main', str(repo.commit(identifier='4@main')))
            self.assertEqual(0, assigned.call_count)
            self.assertEqual('5@main', str(repo.commit(hash='d8bce26fa65c')))

            self.assertEqual('5@main', str(local.Git(self.path).commit(hash='d8bce26fa65c')))

    def test_cache_local_default_branch_ahead_of_remote(self):
        with mocks.local.Git(self.path) as mock, MockTime, LoggerCapture():
            # A commit on main which hasn't been pushed yet
            unpushed = '5d697791f2e1fb3991e441fbd79f412fcff93c75'
            mock.commits['main'].append(Commit(
                hash=unpushed, branch='main', identifier='6@main',
                author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                timestamp=int(time.time()), message='[Testing] Existing commit\n',
            ))
            mock.head = mock.commits['main'][-1]
            self.assertEqual('6@main', str(local.Git(self.path).commit(hash=unpushed)))

            # Loading the cache computes every cached identifier
            with patch.object(NestedFuzzyDict, '__setitem__', autospec=True, side_effect=NestedFuzzyDict.__setitem__) as assigned:
                self.assertEqual('6@main', str(local.Git(self.path).commit(hash=unpushed)))
            loading = assigned.call_count

            # Looking up a commit on another branch walks origin/main first. origin/main's tip is already cached, so
            # only the branch's own 2 commits need identifiers
            with patch.object(NestedFuzzyDict, '__setitem__', autospec=True, side_effect=NestedFuzzyDict.__setitem__) as assigned:
                self.assertEqual('2.2@branch-a', str(local.Git(self.path).commit(identifier='2.2@branch-a')))
            self.assertEqual(loading + 2, assigned.call_count)

    def test_cache_file_revisions_shorter_than_hashes(self):
        with mocks.local.Git(self.path), MockTime, LoggerCapture():
            repo = local.Git(self.path)
            self.assertEqual('5@main', str(repo.commit(hash='d8bce26fa65c')))
            path = os.path.join(repo.common_directory, 'identifiers.json')
            with open(path) as file:
                content = json.load(file)
            content['revisions']['main'] = content['revisions']['main'][:2]
            with open(path, 'w') as file:
                json.dump(content, file)

            # A damaged cache file should be ignored, not stop a repository from being constructed
            for something in ('d8bce26fa65c', 'bae5d1e9', '4@main'):
                self.assert_matches_uncached(local.Git(self.path), lambda r: r.find(something))

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
            self.assertEqual(repo.config()['remote.origin.url'], 'git@example.org:mock/repository')
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

    def test_modified_raises_when_name_status_fails(self):
        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)
            with wkmocks.Subprocess(wkmocks.Subprocess.Route(
                mrepo.executable, '-c', 'diff.renames=false', 'diff', '--name-status', '--staged', '-z',
                cwd=self.path,
                completion=wkmocks.ProcessCompletion(returncode=128, stderr='fatal: bad object HEAD\n'),
            )):
                with self.assertRaises(subprocess.CalledProcessError) as caught:
                    repo.modified()
            self.assertEqual(caught.exception.returncode, 128)
            self.assertEqual(caught.exception.stderr, 'fatal: bad object HEAD\n')

    def test_modified_raises_when_staged_diff_fails(self):
        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)
            with wkmocks.Subprocess(wkmocks.Subprocess.Route(
                mrepo.executable, '-c', 'diff.renames=false', 'diff', '--name-only', '--staged', '-z',
                cwd=self.path,
                completion=wkmocks.ProcessCompletion(returncode=128, stderr='fatal: index file corrupt\n'),
            )):
                with self.assertRaises(subprocess.CalledProcessError) as caught:
                    repo.modified()
                self.assertEqual(caught.exception.stderr, 'fatal: index file corrupt\n')

                with self.assertRaises(subprocess.CalledProcessError):
                    repo.modified(staged=True)

    def test_modified_raises_when_unstaged_diff_fails(self):
        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)
            with wkmocks.Subprocess(wkmocks.Subprocess.Route(
                mrepo.executable, '-c', 'diff.renames=false', 'diff', '--name-only', '-z',
                cwd=self.path,
                completion=wkmocks.ProcessCompletion(returncode=128, stderr='fatal: index file corrupt\n'),
            )):
                self.assertEqual(repo.modified(staged=True), [])
                with self.assertRaises(subprocess.CalledProcessError) as caught:
                    repo.modified()
                self.assertEqual(caught.exception.stderr, 'fatal: index file corrupt\n')

                with self.assertRaises(subprocess.CalledProcessError):
                    repo.modified(staged=False)

    def test_modified_names_files_as_named(self):
        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)
            mrepo.modified['caf\u00e9.txt'] = 'diff'
            mrepo.modified['new\nline.txt'] = 'diff'
            mrepo.modified['untracked.txt'] = 'added'
            self.assertEqual(repo.modified(staged=False), ['caf\u00e9.txt', 'new\nline.txt'])
            self.assertEqual(repo.modified(), ['caf\u00e9.txt', 'new\nline.txt'])

            mrepo.staged['quote"d.txt'] = 'added'
            self.assertEqual(repo.modified(staged=True), ['quote"d.txt'])
            self.assertEqual(repo.modified(), ['quote"d.txt', 'caf\u00e9.txt', 'new\nline.txt'])

            mrepo.staged['tab\there.txt'] = 'diff'
            self.assertEqual(repo.modified(), ['quote"d.txt', 'tab\there.txt'])

    def test_modified_lists_both_names_of_a_move(self):
        commands = []

        def listing(*args, **kwargs):
            commands.append(args)
            if '--name-status' in args:
                return wkmocks.ProcessCompletion(returncode=0, stdout='D\0moved from.txt\0A\0moved\tto.txt\0')
            return wkmocks.ProcessCompletion(returncode=0, stdout='moved from.txt\0moved\tto.txt\0')

        with mocks.local.Git(self.path) as mrepo, OutputCapture():
            repo = local.Git(self.path)
            mrepo.modified['unstaged.txt'] = 'diff'
            with wkmocks.Subprocess(wkmocks.Subprocess.Route(
                mrepo.executable, '-c', 'diff.renames=false', 'diff', '--name-status', '--staged', '-z',
                cwd=self.path,
                generator=listing,
            ), wkmocks.Subprocess.Route(
                mrepo.executable, '-c', 'diff.renames=false', 'diff', '--name-only', '--staged', '-z',
                cwd=self.path,
                generator=listing,
            )):
                self.assertEqual(repo.modified(), ['moved from.txt', 'moved\tto.txt'])
            self.assertEqual(commands, [
                (mrepo.executable, '-c', 'diff.renames=false', 'diff', '--name-status', '--staged', '-z'),
                (mrepo.executable, '-c', 'diff.renames=false', 'diff', '--name-only', '--staged', '-z'),
            ])

    def test_rebase(self):
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertEqual(str(repo.commit(branch='branch-a')), '2.2@branch-a')
            self.assertEqual(repo.rebase(target='main', base='main', head='branch-a', recommit=False), 0)
            self.assertEqual(str(repo.commit(branch='branch-a')), '5.2@branch-a')

    def test_diff(self):
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertEqual(
                ['--- a/ChangeLog', '+++ b/ChangeLog', '@@ -1,0 +1,0 @@', '+Patch Series'],
                list(repo.diff(base='bae5d1e90999d4f916a8a15810ccfa43f37a2fd6'))
            )

    def test_diff_identifier(self):
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertEqual(
                ['--- a/ChangeLog', '+++ b/ChangeLog', '@@ -1,0 +1,0 @@', '+8th commit'],
                list(repo.diff(base='3@main', head='4@main'))
            )

    def test_diff_with_commit_message(self):
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertEqual(
                [
                    'From bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                    'From: Jonathan Bedard <jbedard@apple.com>',
                    'Date: {} +0000'.format(datetime.fromtimestamp(1601668000 + time.timezone, timezone.utc).strftime('%a %b %d %H:%M:%S %Y')),
                    'Subject: [PATCH] 8th commit',
                    '---',
                    'diff --git a/ChangeLog b/ChangeLog',
                    '--- a/ChangeLog',
                    '+++ b/ChangeLog',
                    '@@ -1,0 +1,0 @@',
                    '+8th commit',
                ],
                list(repo.diff(base='3@main', head='4@main', include_log=True))
            )

    def test_pull(self):
        with mocks.local.Git(self.path) as mocked, OutputCapture():
            mocked.staged['added.txt'] = 'added'
            self.assertEqual(local.Git(self.path).pull(rebase=True), 0)

    # The mock only rebases a branch given by name, so the others assert on which branches the cache keeps
    def _cache_after(self, call, head='branch-a', populate=('branch-a', 'branch-b')):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            mock_git.head = mock_git.commits[head][-1]
            repo = local.Git(self.path)
            for branch in populate:
                repo.cache.populate(branch=branch)
            call(repo)
            return set(repo.cache._ordered_commits.keys())

    def test_rebase_clears_the_rebased_branch(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, OutputCapture():
            # The branch rebased isn't the current one
            mock_git.head = mock_git.commits['branch-b'][-1]
            tip = mock_git.commits['branch-a'][-1].hash
            repo = local.Git(self.path)
            self.assertEqual('2.2@branch-a', str(repo.commit(hash=tip)))

            repo.rebase(target='main', base='main', head='branch-a', recommit=False)
            self.assertEqual('5.2@branch-a', self.assert_matches_uncached(repo, lambda r: r.commit(hash=tip))[1])

    def test_rebase_of_head_clears_the_current_branch(self):
        cached = self._cache_after(lambda repo: repo.rebase(target='main', base='main', recommit=False))
        self.assertNotIn('branch-a', cached)
        self.assertIn('branch-b', cached)

    def test_pull_rebase_of_other_branch_clears_the_current_branch(self):
        cached = self._cache_after(lambda repo: repo.pull(rebase=True, branch='main', remote='origin'))
        self.assertNotIn('branch-a', cached)
        self.assertIn('branch-b', cached)

    def test_pull_no_stash(self):
        with mocks.local.Git(self.path) as mocked, OutputCapture():
            mocked.staged['added.txt'] = 'added'
            self.assertEqual(local.Git(self.path).pull(), 128)

    def test_is_worktree_false_for_normal_repo(self):
        """Test that is_worktree returns False for a normal repository."""
        with mocks.local.Git(self.path), OutputCapture():
            repo = local.Git(self.path)
            self.assertFalse(repo.is_worktree)

    def test_is_worktree_true_for_worktree(self):
        """Test that is_worktree returns True when running in a worktree."""
        with mocks.local.Git(self.path, is_worktree=True), OutputCapture():
            repo = local.Git(self.path)
            self.assertTrue(repo.is_worktree)

    def test_fetch_fails_in_worktree(self):
        """Test that fetch with refspec fails when in a worktree.

        Git refuses to fetch into a branch that is checked out in any worktree
        with: "fatal: refusing to fetch into branch 'refs/heads/main' checked out at <path>"
        """
        with mocks.local.Git(self.path, is_worktree=True), OutputCapture():
            repo = local.Git(self.path)
            # fetch() with a branch runs 'git fetch origin main:main' which fails in worktrees
            result = repo.fetch(branch='main', remote='origin')
            self.assertNotEqual(result, 0)

    def test_fetch_succeeds_in_normal_repo(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            repo = local.Git(self.path)
            default_branch = mock_git.default_branch

            original_commits = mock_git.commits[default_branch][:]
            mock_git.commits[default_branch] = original_commits[:-1]
            mock_git.head = mock_git.commits[default_branch][-1]

            self.assertNotEqual(mock_git.commits[default_branch], mock_git.remotes['origin/{}'.format(default_branch)])

            result = repo.fetch(branch=default_branch, remote='origin')
            self.assertEqual(result, 0)
            self.assertEqual(mock_git.commits[default_branch], mock_git.remotes['origin/{}'.format(default_branch)])

    def test_fetch_with_prune(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            repo = local.Git(self.path)
            default_branch = mock_git.default_branch

            original_commits = mock_git.commits[default_branch][:]
            mock_git.commits[default_branch] = original_commits[:-1]
            mock_git.head = mock_git.commits[default_branch][-1]

            result = repo.fetch(branch=default_branch, remote='origin', prune=True)
            self.assertEqual(result, 0)
            self.assertEqual(mock_git.commits[default_branch], mock_git.remotes['origin/{}'.format(default_branch)])

    def test_fetch_default_remote(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            repo = local.Git(self.path)
            default_branch = mock_git.default_branch

            original_commits = mock_git.commits[default_branch][:]
            mock_git.commits[default_branch] = original_commits[:-1]
            mock_git.head = mock_git.commits[default_branch][-1]

            result = repo.fetch(branch=default_branch)
            self.assertEqual(result, 0)
            self.assertEqual(mock_git.commits[default_branch], mock_git.remotes['origin/{}'.format(default_branch)])

    def test_fetch_nonexistent_remote_branch(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            repo = local.Git(self.path)
            self.assertIn('main', mock_git.commits)
            self.assertNotIn('origin/nonexistent', mock_git.remotes)
            result = repo.fetch(branch='nonexistent', remote='origin')
            self.assertNotEqual(result, 0)

    def test_fetch_creates_local_branch(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            repo = local.Git(self.path)
            self.assertIn('origin/branch-a', mock_git.remotes)
            del mock_git.commits['branch-a']
            self.assertNotIn('branch-a', mock_git.commits)
            result = repo.fetch(branch='branch-a', remote='origin')
            self.assertEqual(result, 0)
            self.assertEqual(mock_git.commits['branch-a'], mock_git.remotes['origin/branch-a'])

    def test_pull_rebase_with_branch(self):
        """Test that pull(rebase=True, branch=X) works in a normal repository.

        When not in a worktree, pull() calls fetch() first to update the
        local branch before rebasing.
        """
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            # Switch to a different branch
            mock_git.head = mock_git.commits['branch-a'][-1]

            repo = local.Git(self.path)
            # Verify this is not a worktree
            self.assertFalse(repo.is_worktree)
            # This should succeed - fetch updates local main, then rebase
            result = repo.pull(rebase=True, branch='main', remote='origin')
            self.assertEqual(result, 0)

    def test_pull_rebase_with_branch_checked_out_in_worktree(self):
        """Test that pull(rebase=True, branch=X) works when running in a worktree.

        When in a worktree, the git fetch origin main:main command is skipped
        to avoid the error: "fatal: refusing to fetch into branch 'refs/heads/main'
        checked out at <path>"
        """
        with mocks.local.Git(self.path, is_worktree=True) as mock_git, OutputCapture():
            # Switch to a different branch
            mock_git.head = mock_git.commits['branch-a'][-1]

            repo = local.Git(self.path)
            # Verify we detect this as a worktree
            self.assertTrue(repo.is_worktree)
            # This should succeed because is_worktree skips the problematic fetch
            result = repo.pull(rebase=True, branch='main', remote='origin')
            self.assertEqual(result, 0)

    def test_find_prod_branch_identifier(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            run([git, 'checkout', '-b', 'safari-610-branch'], cwd=self.path, capture_output=True)
            mock_git.staged['file.txt'] = 'prod'
            run([git, 'commit', '-m', 'Production branch commit'], cwd=self.path, capture_output=True)
            tip = mock_git.head
            mock_git.remotes['origin/safari-610-branch'] = list(mock_git.commits['safari-610-branch'])
            repository = local.Git(self.path)
            for something in ('5.1@safari-610-branch', tip.hash):
                self.assert_matches_uncached(repository, lambda r: r.find(something))

    def test_find_prod_branch_only_on_the_remote(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            run([git, 'checkout', '-b', 'safari-610-branch'], cwd=self.path, capture_output=True)
            mock_git.staged['file.txt'] = 'prod'
            run([git, 'commit', '-m', 'Production branch commit'], cwd=self.path, capture_output=True)
            tip = mock_git.head
            mock_git.remotes['origin/safari-610-branch'] = mock_git.commits.pop('safari-610-branch')
            run([git, 'checkout', 'main'], cwd=self.path, capture_output=True)

            # The hash resolves, but an identifier on a branch that only a remote has can't be looked up
            repository = local.Git(self.path)
            self.assertEqual('5.1@safari-610-branch', self.assert_matches_uncached(repository, lambda r: r.find(tip.hash))[1])
            for cached in (repository, local.Git(self.path, cached=False)):
                with self.assertRaises(Exception):
                    cached.find('5.1@safari-610-branch')

    def test_find_prod_branch_identifier_with_a_different_prod_branches_policy(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            run([git, 'checkout', '-b', 'safari-7615-branch'], cwd=self.path, capture_output=True)
            release = []
            for message in ('First production branch commit', 'Second production branch commit'):
                mock_git.staged['file.txt'] = message
                run([git, 'commit', '-m', message], cwd=self.path, capture_output=True)
                release.append(mock_git.head)
            mock_git.remotes['origin/safari-7615-branch'] = list(mock_git.commits['safari-7615-branch'])

            # The cache file is written under the default policy, which makes safari-7615-branch a production branch
            repository = local.Git(self.path)
            for commit, identifier in zip(release, ('5.1@safari-7615-branch', '5.2@safari-7615-branch')):
                self.assertEqual(identifier, self.assert_matches_uncached(repository, lambda r: r.find(commit.hash))[1])

            # Another policy, which doesn't, reads that file and still agrees with git
            custom = local.Git(self.path, cached=True, prod_branches=r'^release-')
            uncached = local.Git(self.path, cached=False, prod_branches=r'^release-')
            for commit, identifier in zip(release, ('5.1@safari-7615-branch', '5.2@safari-7615-branch')):
                self.assertEqual(identifier, str(custom.find(commit.hash)))
                self.assertEqual(self.observe(uncached.find(commit.hash)), self.observe(custom.find(commit.hash)))

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
                f.write('[webkitscmpy "remotes.origin"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit.git\n')
                f.write('    security-level = 0\n')
                f.write('[webkitscmpy "remotes.security"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit-security.git\n')
                f.write('    security-level = 1\n')

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
                f.write('[webkitscmpy "remotes.origin"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit.git\n')
                f.write('    security-level = 0\n')
                f.write('[webkitscmpy "remotes.security"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit-security.git\n')
                f.write('    security-level = 1\n')

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

    def test_last_commits_on(self):
        with mocks.local.Git(self.path), OutputCapture():
            git = local.Git(self.path)
            commits = git.last_commits_on('README.md', count=5)

            # Mock environment only produces 3 commits when README.md is requested
            # because the mock filters to commits with odd-numbered identifiers.
            self.assertEqual(len(commits), 3)
            self.assertTrue(all(len(c.hash) == 40 for c in commits))
            self.assertEqual(
                [f'{c.identifier}@{c.branch}' for c in commits],
                [
                    '1@main',
                    '3@main',
                    '5@main',
                ]
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

    def test_is_suitable_branch_for_pull_request(self):
        with mocks.local.Git(self.path, remotes={
            'origin': 'git@github.example.com:WebKit/WebKit.git'
        }):
            repo = local.Git(self.path)
            source_remote = repo.source_remotes()[0]
            self.assertEqual(repo.is_suitable_branch_for_pull_request('eng/12345', source_remote=source_remote), True)
            self.assertEqual(repo.is_suitable_branch_for_pull_request('eng/squash-branch', source_remote=source_remote), True)
            self.assertEqual(repo.is_suitable_branch_for_pull_request('integration/branch', source_remote=source_remote), True)
            self.assertEqual(repo.is_suitable_branch_for_pull_request('not-default-branch', source_remote=source_remote), True)
            self.assertEqual(repo.is_suitable_branch_for_pull_request('main', source_remote=source_remote), False)
            self.assertEqual(repo.is_suitable_branch_for_pull_request('safari-610-branch', source_remote=source_remote), False)
            self.assertEqual(repo.is_suitable_branch_for_pull_request('branch-a', source_remote=source_remote), False)
            self.assertEqual(repo.is_suitable_branch_for_pull_request(None, source_remote=source_remote), False)

    def test_fetch_head_updates_default_remote_fetch_origin(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            fetched_commit = self._prepare_fetch_head_commit(mock_git)
            repo = local.Git(self.path)
            default_branch = repo.default_branch
            remote_ref = f'origin/{default_branch}'
            fetch_path = os.path.join(repo.common_directory, 'FETCH_HEAD')

            with open(fetch_path, 'w') as fetch_head:
                fetch_head.write(f"{fetched_commit.hash}\tbranch '{default_branch}' of {repo.url()}\n")

            self.assertNotIn(default_branch, repo.branches_for(fetched_commit.hash))
            self.assertNotEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)
            self.assertTrue(repo._is_on_default_branch(fetched_commit.hash))
            self.assertEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)

    def test_fetch_head_updates_default_remote_fetch_from_url(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            fetched_commit = self._prepare_fetch_head_commit(mock_git)
            repo = local.Git(self.path)
            default_branch = repo.default_branch
            remote_ref = f'origin/{default_branch}'
            fetch_path = os.path.join(repo.common_directory, 'FETCH_HEAD')

            with open(fetch_path, 'w') as fetch_head:
                fetch_head.write(f"{fetched_commit.hash}\t\t{repo.url()}\n")

            self.assertNotIn(default_branch, repo.branches_for(fetched_commit.hash))
            self.assertNotEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)
            self.assertTrue(repo._is_on_default_branch(fetched_commit.hash))
            self.assertEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)

    # A commit fetched by hash is described as "'<hash>' of <url>", which must not move origin/main or be labelled @main
    def test_find_fetch_head_by_hash_is_not_on_default_branch(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            fetched = self._prepare_fetch_head_commit(mock_git)
            repository = local.Git(self.path)
            repository.find('1@main')
            before = mock_git.remotes['origin/main'][-1].hash
            with open(os.path.join(repository.common_directory, 'FETCH_HEAD'), 'w') as fetch_head:
                fetch_head.write(f"{fetched.hash}\t\t'{fetched.hash}' of {repository.url()}\n")

            result = self.assert_matches_uncached(repository, lambda r: r.find(fetched.hash))
            self.assertNotEqual('main', result[2])
            self.assertEqual(before, mock_git.remotes['origin/main'][-1].hash)

    def test_find_with_cache_behind_default_branch_across_instances(self):
        with mocks.local.Git(self.path) as mock_git, MockTime, LoggerCapture():
            git = local.Git.executable()
            history = [copy.deepcopy(commit) for commit in mock_git.commits['main']]
            run([git, 'reset', '--hard', history[1].hash], cwd=self.path, capture_output=True)
            run([git, 'update-ref', 'refs/remotes/origin/main', history[1].hash], cwd=self.path, capture_output=True)
            local.Git(self.path).find('1@main')

            # Stops short of the full history: the mock can't walk a fetch_head branch identical to main
            for count in range(3, len(history)):
                fetched = [copy.deepcopy(commit) for commit in history[:count]]
                for commit in fetched:
                    commit.branch = 'fetch_head'
                mock_git.commits['fetch_head'] = fetched
                repository = local.Git(self.path)
                with open(os.path.join(repository.common_directory, 'FETCH_HEAD'), 'w') as fetch_head:
                    fetch_head.write(f"{fetched[-1].hash}\t\t{repository.url()}\n")
                result = self.assert_matches_uncached(repository, lambda r: r.find(fetched[-1].hash))
                self.assertEqual(f'{count}@main', result[1])

    def test_fetch_head_from_other_remote_ignored(self):
        with mocks.local.Git(self.path) as mock_git, OutputCapture():
            fetched_commit = self._prepare_fetch_head_commit(mock_git)
            repo = local.Git(self.path)
            default_branch = repo.default_branch
            remote_ref = f'origin/{default_branch}'
            fetch_path = os.path.join(repo.common_directory, 'FETCH_HEAD')

            with open(fetch_path, 'w') as fetch_head:
                fetch_head.write(f"{fetched_commit.hash}\tbranch '{default_branch}' of https://example.invalid/WebKit.git\n")
                fetch_head.write(f"{fetched_commit.hash}\tnot-for-merge\tbranch '{default_branch}' of {repo.url()}\n")
                fetch_head.write(f"{fetched_commit.hash}\tbranch 'randombranch' of {repo.url()}\n")

            self.assertNotIn(default_branch, repo.branches_for(fetched_commit.hash))
            self.assertNotEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)
            self.assertFalse(repo._is_on_default_branch(fetched_commit.hash))
            self.assertNotEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)

    def test_fetch_head_in_worktree_uses_git_directory(self):
        """Test that FETCH_HEAD is read from git_directory in a worktree.

        In a worktree, git_directory differs from common_directory.
        FETCH_HEAD is a per-worktree file stored in git_directory.
        """
        with mocks.local.Git(self.path, is_worktree=True) as mock_git, OutputCapture():
            fetched_commit = self._prepare_fetch_head_commit(mock_git)
            repo = local.Git(self.path)
            default_branch = repo.default_branch
            remote_ref = f'origin/{default_branch}'

            # Verify we're in a worktree where git_directory != common_directory
            self.assertTrue(repo.is_worktree)
            self.assertNotEqual(repo.git_directory, repo.common_directory)

            # Write FETCH_HEAD to git_directory (where git pull writes it in a worktree)
            fetch_path = os.path.join(repo.git_directory, 'FETCH_HEAD')
            with open(fetch_path, 'w') as fetch_head:
                fetch_head.write(f"{fetched_commit.hash}\tbranch '{default_branch}' of {repo.url()}\n")

            self.assertNotIn(default_branch, repo.branches_for(fetched_commit.hash))
            self.assertNotEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)
            self.assertTrue(repo._is_on_default_branch(fetched_commit.hash))
            self.assertEqual(mock_git.remotes[remote_ref][-1].hash, fetched_commit.hash)

    def test_branches_for(self):
        with OutputCapture(), mocks.local.Git(
            self.path,
            remotes={
                'security': 'git@github.example.com:WebKit/WebKit-security.git',
            },
        ) as mock_git:
            del mock_git.remotes['security/eng/squash-branch']
            mock_git.remotes['security/hidden-branch'] = [mock_git.commits[mock_git.branch][-1]]

            repo = local.Git(self.path)

            all_branches = repo.branches_for()
            self.assertEqual(
                ['branch-a', 'branch-b', 'eng/squash-branch', 'hidden-branch', 'main'],
                all_branches,
            )

            origin_branches = repo.branches_for(remote='origin')
            self.assertEqual(
                ['branch-a', 'branch-b', 'main'],
                origin_branches,
            )

            remote_false_branches = repo.branches_for(remote=False)
            self.assertEqual(
                ['branch-a', 'branch-b', 'eng/squash-branch', 'main'],
                remote_false_branches,
            )

            remote_none_branches = repo.branches_for(remote=None)
            self.assertEqual(
                {
                    None: {'branch-a', 'branch-b', 'eng/squash-branch', 'main'},
                    'origin': {'branch-a', 'branch-b', 'main'},
                    'security': {'branch-a', 'branch-b', 'hidden-branch', 'main'},
                },
                dict(remote_none_branches),
            )

            contains_5_main = repo.branches_for(hash=repo.find('5@main').hash, remote=None)
            self.assertEqual(
                {
                    None: {'eng/squash-branch', 'main'},
                    'origin': {'main'},
                    'security': {'main', 'hidden-branch'},
                },
                dict(contains_5_main),
            )

            contains_1_main = repo.branches_for(hash=repo.find('1@main').hash, remote=None)
            self.assertEqual(
                {
                    None: {'branch-a', 'branch-b', 'eng/squash-branch', 'main'},
                    'origin': {'branch-a', 'branch-b', 'main'},
                    'security': {'branch-a', 'branch-b', 'hidden-branch', 'main'},
                },
                dict(contains_1_main),
            )

    def test_args_from_content_normal_commit(self):
        """Test _args_from_content correctly parses standard commit format."""
        with mocks.local.Git(self.path):
            repo = local.Git(self.path)
            content = '''commit abc123def456
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: 1600000000
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: 1600000001

    Test commit message

    More details here.
'''
            result = repo._args_from_content(content)
            self.assertEqual(result.get('timestamp'), 1600000001)
            self.assertEqual(result.get('author'), Contributor.from_scm_log('Author:     Jonathan Bedard <jbedard@apple.com>'))

    def test_args_from_content_merge_commit(self):
        """Test _args_from_content correctly parses merge commits with Merge: line."""
        with mocks.local.Git(self.path):
            repo = local.Git(self.path)
            content = '''commit abc123def456
Merge: parent1 parent2
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: 1600000000
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: 1600000001

    Merge branch 'feature' into main
'''
            result = repo._args_from_content(content)
            self.assertEqual(result.get('timestamp'), 1600000001)
            self.assertEqual(result.get('author'), Contributor.from_scm_log('Author:     Jonathan Bedard <jbedard@apple.com>'))

    def test_args_from_content_with_svn_revision(self):
        """Test _args_from_content extracts SVN revision from commit message."""
        with mocks.local.Git(self.path):
            repo = local.Git(self.path)
            content = '''commit abc123def456
Author:     Jonathan Bedard <jbedard@apple.com>
AuthorDate: 1600000000
Commit:     Jonathan Bedard <jbedard@apple.com>
CommitDate: 1600000001

    Test commit message
    git-svn-id: https://svn.webkit.org/repository/webkit/trunk@12345 268f45cc-cd09-0410-ab3c-d52691b4dbfc
'''
            result = repo._args_from_content(content)
            self.assertEqual(result.get('timestamp'), 1600000001)
            self.assertEqual(result.get('revision'), 12345)


class TestMockGit(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    def test_executable(self):
        with mocks.local.Git(self.path) as mock_git:
            self.assertEqual(mock_git.executable, local.Git.executable())

    def test_executable_stack(self):
        with patch('shutil.which', lambda _: 'everything-command'):
            with mocks.local.Git(self.path) as mock_git:
                self.assertEqual(mock_git.executable, local.Git.executable())
                self.assertEqual('everything-command', shutil.which('echo'))

    def test_executable_stack_2(self):
        with mocks.local.Git(self.path) as mock_git:
            with patch('shutil.which', lambda _: 'everything-command'):
                self.assertEqual(os.path.realpath('everything-command'), local.Git.executable())
                self.assertEqual('everything-command', shutil.which('echo'))

    def test_update_ref_moves_remote_backwards(self):
        with mocks.local.Git(self.path) as mock_git:
            history = [commit.hash for commit in mock_git.remotes['origin/main']]
            self.assertEqual(0, run(
                [local.Git.executable(), 'update-ref', 'refs/remotes/origin/main', history[1]],
                cwd=self.path, capture_output=True,
            ).returncode)
            self.assertEqual(history[:2], [commit.hash for commit in mock_git.remotes['origin/main']])

    def test_reset_hard_to_commit(self):
        with mocks.local.Git(self.path) as mock_git:
            history = [commit.hash for commit in mock_git.commits['main']]
            self.assertEqual(0, run(
                [local.Git.executable(), 'reset', '--hard', history[1]],
                cwd=self.path, capture_output=True,
            ).returncode)
            self.assertEqual(history[:2], [commit.hash for commit in mock_git.commits['main']])
            self.assertEqual(history[1], mock_git.head.hash)

    def test_default_branch(self):
        with mocks.local.Git(self.path, default_branch='master') as mock_git:
            self.assertNotIn('main', mock_git.commits)
            self.assertEqual({'master'}, {commit.branch for commit in mock_git.commits['master']})
            self.assertEqual({'master'}, {commit.branch for commit in mock_git.remotes['origin/master']})
            self.assertEqual('master', mock_git.head.branch)
            self.assertEqual('master', local.Git(self.path).branch)

            history = [commit.hash for commit in mock_git.commits['master']]
            self.assertEqual(0, run(
                [local.Git.executable(), 'reset', '--hard', 'origin/master'],
                cwd=self.path, capture_output=True,
            ).returncode)
            self.assertEqual(history[-1], mock_git.head.hash)

    def test_find_remote_only_hash(self):
        with mocks.local.Git(self.path) as mock_git:
            self.assertIsNone(mock_git.find('bae5d1e0'))
            remote_only = copy.deepcopy(mock_git.commits['main'][-1])
            remote_only.hash = '0123456789abcdef0123456789abcdef01234567'
            mock_git.remotes['origin/eng/remote-only'] = [copy.deepcopy(mock_git.commits['main'][-1]), remote_only]

            self.assertEqual(remote_only.hash, mock_git.find('0123456789ab').hash)
            self.assertEqual(mock_git.commits['main'][3].hash, mock_git.find('bae5d1e9').hash)

    def test_rev_list_remote_only_hash(self):
        with mocks.local.Git(self.path) as mock_git:
            remote_only = copy.deepcopy(mock_git.commits['main'][-1])
            remote_only.hash = '0123456789abcdef0123456789abcdef01234567'
            remote_only.identifier += 1
            mock_git.remotes['origin/main'] = [copy.deepcopy(commit) for commit in mock_git.commits['main']] + [remote_only]

            self.assertEqual(6, len(mock_git.rev_list(remote_only.hash)))
            self.assertEqual(1, len(mock_git.rev_list(f'main..{remote_only.hash}')))
            self.assertEqual(5, len(mock_git.rev_list('main')))

    def test_find_ancestor_of_remote_only_hash(self):
        with mocks.local.Git(self.path) as mock_git:
            remote_only = copy.deepcopy(mock_git.commits['main'][-1])
            remote_only.hash = '0123456789abcdef0123456789abcdef01234567'
            remote_only.branch = 'eng/remote-only'
            remote_only.identifier = 1
            remote_only.branch_point = 5
            mock_git.remotes['origin/eng/remote-only'] = [copy.deepcopy(mock_git.commits['main'][-1]), remote_only]

            self.assertEqual(mock_git.commits['main'][-1].hash, mock_git.find(f'{remote_only.hash}~1').hash)


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

    def test_files_changed_no_argument(self):
        with mocks.remote.GitHub():
            with self.assertRaises(ValueError):
                remote.GitHub(self.remote).files_changed()

    def test_checkout_url(self):
        self.assertEqual(remote.GitHub(self.remote).checkout_url(), 'git@github.example.com:WebKit/WebKit.git')
        self.assertEqual(remote.GitHub(self.remote).checkout_url(http=True), 'https://github.example.com/WebKit/WebKit.git')
        self.assertEqual(remote.GitHub(self.remote).checkout_url(ssh=True), 'git@github.example.com:WebKit/WebKit.git')
        with self.assertRaises(ValueError):
            remote.GitHub(self.remote).checkout_url(http=True, ssh=True)

    def test_diff(self):
        with mocks.remote.GitHub():
            repo = remote.GitHub(self.remote)
            self.assertEqual([
                'diff --git a/ChangeLog b/ChangeLog',
                '--- a/ChangeLog',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+Patch Series',
            ], list(repo.diff(base='bae5d1e90999d4f916a8a15810ccfa43f37a2fd6')))

    def test_diff_with_commit_message(self):
        with mocks.remote.GitHub():
            repo = remote.GitHub(self.remote)
            self.assertEqual([
                'From bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                'From: Jonathan Bedard <jbedard@apple.com>',
                'Date: {}'.format(datetime.fromtimestamp(1601642800).strftime('%a %b %d %H:%M:%S %Y')),
                'Subject: [PATCH] 8th commit',
                '---',
                'diff --git a/ChangeLog b/ChangeLog',
                '--- a/ChangeLog',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+8th commit',
            ], list(repo.diff(base='3@main', head='4@main', include_log=True)))

    def test_diff_with_commit_message_multiple(self):
        self.maxDiff = None
        with mocks.remote.GitHub():
            repo = remote.GitHub(self.remote)
            self.assertEqual([
                'From bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                'From: Jonathan Bedard <jbedard@apple.com>',
                'Date: {}'.format(datetime.fromtimestamp(1601642800).strftime('%a %b %d %H:%M:%S %Y')),
                'Subject: [PATCH 1/2] 8th commit',
                '---',
                'diff --git a/ChangeLog b/ChangeLog',
                '--- a/ChangeLog',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+8th commit',
                'From 1abe25b443e985f93b90d830e4a7e3731336af4d',
                'From: Jonathan Bedard <jbedard@apple.com>',
                'Date: {}'.format(datetime.fromtimestamp(1601637800).strftime('%a %b %d %H:%M:%S %Y')),
                'Subject: [PATCH 2/2] 4th commit',
                '---',
                'diff --git a/ChangeLog b/ChangeLog',
                '--- a/ChangeLog',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+4th commit',
            ], list(repo.diff(base='2@main', head='4@main', include_log=True)))


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

    def test_files_changed_no_argument(self):
        with mocks.remote.BitBucket():
            with self.assertRaises(ValueError):
                remote.BitBucket(self.remote).files_changed()

    def test_checkout_url(self):
        self.assertEqual(remote.BitBucket(self.remote).checkout_url(), 'git@bitbucket.example.com/WEBKIT/webkit.git')
        self.assertEqual(remote.BitBucket(self.remote).checkout_url(http=True), 'https://bitbucket.example.com/scm/WEBKIT/webkit.git')
        self.assertEqual(remote.BitBucket(self.remote).checkout_url(ssh=True), 'git@bitbucket.example.com/WEBKIT/webkit.git')
        with self.assertRaises(ValueError):
            remote.BitBucket(self.remote).checkout_url(http=True, ssh=True)

    def test_diff(self):
        with mocks.remote.BitBucket():
            repo = remote.BitBucket(self.remote)
            self.assertEqual([
                '--- /dev/null',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+Patch Series',
            ], list(repo.diff(base='bae5d1e90999d4f916a8a15810ccfa43f37a2fd6')))

    def test_diff_with_commit_message(self):
        with mocks.remote.BitBucket():
            repo = remote.BitBucket(self.remote)
            self.assertEqual([
                'From bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                'From: Jonathan Bedard <jbedard@apple.com>',
                'Date: {}'.format(datetime.fromtimestamp(1601668000).strftime('%a %b %d %H:%M:%S %Y')),
                'Subject: [PATCH] 8th commit',
                '---',
                '--- /dev/null',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+8th commit',
            ], list(repo.diff(base='3@main', head='4@main', include_log=True)))

    def test_diff_with_commit_message_multiple(self):
        self.maxDiff = None
        with mocks.remote.BitBucket():
            repo = remote.BitBucket(self.remote)
            self.assertEqual([
                'From bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                'From: Jonathan Bedard <jbedard@apple.com>',
                'Date: {}'.format(datetime.fromtimestamp(1601668000).strftime('%a %b %d %H:%M:%S %Y')),
                'Subject: [PATCH 1/2] 8th commit',
                '---',
                '--- /dev/null',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+8th commit',
                'From 1abe25b443e985f93b90d830e4a7e3731336af4d',
                'From: Jonathan Bedard <jbedard@apple.com>',
                'Date: {}'.format(datetime.fromtimestamp(1601663000).strftime('%a %b %d %H:%M:%S %Y')),
                'Subject: [PATCH 2/2] 4th commit',
                '---',
                '--- /dev/null',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+4th commit',
            ], list(repo.diff(base='2@main', head='4@main', include_log=True)))
