# Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
from webkitcorepy import run


class TestSquash(testing.PathTestCase):

    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestSquash, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_github_with_previous_history(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            repo.checkout('eng/squash-branch')
            result = program.main(
                args=('pull-request', '-v', '--squash', '--no-history'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, '[Testing] Creating commits' in repo.head.message)
            self.assertEqual(True, '* a.cpp' in repo.head.message)
            self.assertEqual(True, '* b.cpp' in repo.head.message)
            self.assertEqual(True, '* c.cpp' in repo.head.message)
            self.assertEqual(True, 'Changed something 1' in repo.head.message)
            self.assertEqual(True, 'Changed something 2' in repo.head.message)
            self.assertEqual(True, 'Changed something 3' in repo.head.message)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | [Testing] Creating commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                '    Squashed 3 commits',
                "Rebasing 'eng/squash-branch' on 'main'...",
                "Rebased 'eng/squash-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/squash-branch' to 'fork'...",
                "Creating pull-request for 'eng/squash-branch'..."
            ],
        )

    def test_github_without_previous_history(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            repo.checkout('eng/squash-branch')
            result = program.main(
                args=('pull-request', '-v', '--squash', '--no-history', '--new-message'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, '[Testing] Creating commits' in repo.head.message)
            self.assertEqual(True, '* a.cpp' in repo.head.message)
            self.assertEqual(True, '* b.cpp' in repo.head.message)
            self.assertEqual(True, '* c.cpp' in repo.head.message)
            self.assertEqual(True, 'Changed something 1' not in repo.head.message)
            self.assertEqual(True, 'Changed something 2' not in repo.head.message)
            self.assertEqual(True, 'Changed something 3' not in repo.head.message)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | [Testing] Creating commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                '    Squashed 3 commits',
                "Rebasing 'eng/squash-branch' on 'main'...",
                "Rebased 'eng/squash-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/squash-branch' to 'fork'...",
                "Creating pull-request for 'eng/squash-branch'..."
            ],
        )

    def test_github_two_step(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            repo.checkout('eng/squash-branch')
            result = program.main(
                args=('squash', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, '[Testing] Creating commits' in repo.head.message)
            result = program.main(args=('pull-request', '-v', '--no-history'), path=self.path)
            self.assertEqual(0, result)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | [Testing] Creating commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Squashed 3 commits',
                'Using committed changes...',
                "Rebasing 'eng/squash-branch' on 'main'...",
                "Rebased 'eng/squash-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/squash-branch' to 'fork'...",
                "Creating pull-request for 'eng/squash-branch'..."
            ],
        )

    def test_modified(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, \
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote),) as repo, mocks.local.Svn(), \
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
            repo.checkout('eng/squash-branch')
            result = program.main(
                args=('squash', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

        self.assertEqual(captured.stderr.getvalue(), 'Please commit your changes or stash them before you squash to base commit: d8bce26fa65c')
