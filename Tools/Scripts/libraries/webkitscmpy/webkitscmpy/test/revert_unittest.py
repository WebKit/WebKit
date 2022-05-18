# Copyright (C) 2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import os

from mock import patch
from webkitcorepy import OutputCapture, testing
from webkitscmpy import local, program, mocks


class TestRevert(testing.PathTestCase):

    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestRevert, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_github(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):

            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '-i', 'pr-branch', '-v', '--no-history', '--pr'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, 'Revert [5@main] Patch Series' in repo.head.message)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/pr-branch'\n"
            "Created 'PR 1 | Revert [5@main] Patch Series'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                '    Found 1 commit...',
                'Reverted d8bce26fa65c6fc8f39c17927abb77f69fab82fc',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                "    Found 1 commit...",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/pr-branch'...",
                'Adding comment for reverted commits...'
            ],
        )

    def test_github_two_step(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):

            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '-i', 'pr-branch', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, 'Revert [5@main] Patch Series' in repo.head.message)
            result = program.main(args=('pull-request', '-v', '--no-history'), path=self.path)
            self.assertEqual(0, result)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/pr-branch'\n"
            "Created 'PR 1 | Revert [5@main] Patch Series'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                '    Found 1 commit...',
                'Reverted d8bce26fa65c6fc8f39c17927abb77f69fab82fc',
                '    Found 1 commit...',
                'Using committed changes...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                "    Found 1 commit...",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/pr-branch'...",
                'Adding comment for reverted commits...'
            ],
        )

    def test_modified(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, \
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo, mocks.local.Svn(), \
            patch('webkitbugspy.Tracker._trackers', []):

            repo.modified = {
                'a.py': """diff --git a/a.py b/a.py
index 05e8751..0bf3c85 100644
--- a/test
+++ b/test
@@ -1,3 +1,4 @@
+1111
 aaaa
 cccc
 bbbb
"""
            }
            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '-i', 'pr-branch', '-v', '--pr'),
                path=self.path,
            )
            self.assertEqual(1, result)

        self.assertEqual(captured.stderr.getvalue(), 'Please commit your changes or stash them before you revert commit: d8bce26fa65c6fc8f39c17927abb77f69fab82fc')

    def test_update(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):

            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '-i', 'pr-branch', '-v', '--pr'),
                path=self.path,
            )
            self.assertEqual(0, result)
            result = program.main(
                args=('pull-request', '-v', '--no-history'),
                path=self.path,
            )
            self.assertEqual(0, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/pr-branch'\n"
            "Created 'PR 1 | Revert [5@main] Patch Series'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n"
            "Updated 'PR 1 | Revert [5@main] Patch Series'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                '    Found 1 commit...',
                'Reverted d8bce26fa65c6fc8f39c17927abb77f69fab82fc',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                "    Found 1 commit...",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating 'eng/pr-branch-1' as a reference branch",
                "Creating pull-request for 'eng/pr-branch'...",
                'Adding comment for reverted commits...',
                '    Found 1 commit...',
                'Using committed changes...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                '    Found 1 commit...',
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                "Checking PR labels for 'merging-blocked'...",
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Updating pull-request for 'eng/pr-branch'..."
            ],
        )
