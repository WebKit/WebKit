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

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime, Terminal as MockTerminal
from webkitscmpy import program, mocks


class TestClean(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestClean, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_checkout_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('clean',),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_clean_git(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime:
            repo.modified['some/file'] = 'diff contnet'
            self.assertEqual(0, program.main(
                args=('clean',),
                path=self.path,
            ))
            self.assertDictEqual({}, repo.modified)

    def test_clean_svn(self):
        with OutputCapture(), mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(0, program.main(
                args=('clean',),
                path=self.path,
            ))

    def test_clean_branch(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime:
            self.assertIn('branch-a', repo.commits)
            self.assertEqual(0, program.main(
                args=('clean', 'branch-a'),
                path=self.path,
            ))
            self.assertNotIn('branch-a', repo.commits)

    def test_clean_pr(self):
        with OutputCapture(), mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():
            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'pr-branch'),
                path=self.path,
            ))

            self.assertIn('eng/pr-branch', repo.commits)
            with MockTerminal.input('y'):
                self.assertEqual(0, program.main(
                    args=('clean', 'pr-1'),
                    path=self.path,
                ))
            self.assertNotIn('eng/pr-branch', repo.commits)

    def test_delete_pr_branches(self):
        with OutputCapture(), mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():
            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'pr-branch'),
                path=self.path,
            ))
            remote.pull_requests[-1]['state'] = 'closed'

            self.assertIn('eng/pr-branch', repo.commits)
            self.assertEqual(0, program.main(
                args=('delete-pr-branches', '-v'),
                path=self.path,
            ))
            self.assertNotIn('eng/pr-branch', repo.commits)

    def test_delete_pr_branches_invalid_remote(self):
        with OutputCapture() as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('delete-pr-branches', '-v', '--remote=all'),
                path=self.path,
            ))

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "'{}' doesn't have a recognized remote named 'all'\n".format(self.path))
