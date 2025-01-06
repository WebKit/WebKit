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
import time

from mock import patch
from webkitbugspy import bugzilla, mocks as bmocks, radar, Tracker
from webkitcorepy import OutputCapture, testing
from webkitscmpy import local, program, mocks, Contributor, Commit
from webkitcorepy.mocks import Time as MockTime, Terminal as MockTerminal, Environment


class TestRevert(testing.PathTestCase):

    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestRevert, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    @classmethod
    def webserver(cls, approved=None, labels=None, environment=None):
        result = mocks.remote.GitHub(labels=labels, environment=environment)
        result.users.create('Ricky Reviewer', 'rreviewer', ['rreviewer@webkit.org'])
        result.users.create('Tim Contributor', 'tcontributor', ['tcontributor@webkit.org'])
        result.issues = {
            1: dict(
                number=1,
                opened=True,
                title='Unreviewed, reverting 5@main (d8bce26fa65c)',
                description='?',
                creator=result.users.create(name='Tim Contributor', username='tcontributor'),
                timestamp=1639536160,
                assignee=None,
                comments=[],
            ),
        }
        result.pull_requests = [dict(
            number=1,
            state='open',
            title='Unreviewed, reverting 5@main (d8bce26fa65c)',
            user=dict(login='tcontributor'),
            body='''#### a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd
    <pre>
    To Be Committed

    Reviewed by NOBODY (OOPS!).
    </pre>
    ''',
            head=dict(ref='username:eng/example'),
            base=dict(ref='main'),
            requested_reviews=[dict(login='rreviewer')],
            reviews=[
                dict(user=dict(login='rreviewer'), state='APPROVED')
            ] if approved else [] + [
                dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')
            ] if approved is not None else [], _links=dict(
                issue=dict(href='https://{}/issues/1'.format(result.api_remote)),
            ), draft=False,
        )]
        return result

    def test_github(self):
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)), OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '-v', '--no-history', '--pr'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, 'Unreviewed, reverting 5@main (d8bce26fa65c)' in repo.head.message)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "This issue will track the revert and should not be the issue of the commit(s) to be reverted.\n"
            "Enter issue URL or title of new issue (reason for the revert): \n"
            "Created the local development branch 'eng/Example-feature-1'\n"
            "Created 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-feature-1'...",
                'Reverted 5@main',
                'Automatically relating issues...',
                "Rebasing 'eng/Example-feature-1' on 'main'...",
                "Rebased 'eng/Example-feature-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/Example-feature-1' to 'fork'...",
                "Creating pull-request for 'eng/Example-feature-1'...",
            ],
        )

    def test_github_two_step(self):
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)), OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '--no-pr', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, 'Unreviewed, reverting 5@main (d8bce26fa65c)' in repo.head.message)
            with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)):
                result = program.main(args=('pull-request', '-v', '--no-history'), path=self.path)
            self.assertEqual(0, result)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "This issue will track the revert and should not be the issue of the commit(s) to be reverted.\n"
            "Enter issue URL or title of new issue (reason for the revert): \n"
            "Created the local development branch 'eng/Example-feature-1'\n"
            "Created 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-feature-1'...",
                'Reverted 5@main',
                'Automatically relating issues...',
                'Using committed changes...',
                "Rebasing 'eng/Example-feature-1' on 'main'...",
                "Rebased 'eng/Example-feature-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/Example-feature-1' to 'fork'...",
                "Creating pull-request for 'eng/Example-feature-1'...",
            ],
        )

    def test_args(self):
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)), OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '--issue', '{}/show_bug.cgi?id=2'.format(self.BUGZILLA), '-v', '--no-pr'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, 'Unreviewed, reverting 5@main (d8bce26fa65c)' in repo.head.message)
            with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)):
                result = program.main(args=('pull-request', '-v', '--no-history'), path=self.path)
            self.assertEqual(0, result)
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/Example-feature-1'\n"
            "Created 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-feature-1'...",
                'Reverted 5@main',
                'Automatically relating issues...',
                'Using committed changes...',
                "Rebasing 'eng/Example-feature-1' on 'main'...",
                "Rebased 'eng/Example-feature-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/Example-feature-1' to 'fork'...",
                "Creating pull-request for 'eng/Example-feature-1'...",
            ],
        )

    def test_no_issue(self):
        with MockTerminal.input('reason for revert'), OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '--no-issue', '-v', '--no-pr'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, 'Unreviewed, reverting 5@main (d8bce26fa65c)' in repo.head.message)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Enter a reason for the revert: \n'
            "Created the local development branch 'eng/reason-for-revert'\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')

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
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)), OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '-v', '--pr'),
                path=self.path,
            )
            self.assertEqual(0, result)
            with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)):
                result = program.main(
                    args=('pull-request', '-v', '--no-history'),
                    path=self.path,
                )
            self.assertEqual(0, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            "This issue will track the revert and should not be the issue of the commit(s) to be reverted.\n"
            "Enter issue URL or title of new issue (reason for the revert): \n"
            "Created the local development branch 'eng/Example-feature-1'\n"
            "Created 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n"
            "Updated 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-feature-1'...",
                'Reverted 5@main',
                'Automatically relating issues...',
                "Rebasing 'eng/Example-feature-1' on 'main'...",
                "Rebased 'eng/Example-feature-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/Example-feature-1' to 'fork'...",
                "Creating 'eng/Example-feature-1-1' as a reference branch",
                "Creating pull-request for 'eng/Example-feature-1'...",
                'Using committed changes...',
                "Rebasing 'eng/Example-feature-1' on 'main'...",
                "Rebased 'eng/Example-feature-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                'Checking PR labels for active labels...',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/Example-feature-1' to 'fork'...",
                "Updating pull-request for 'eng/Example-feature-1'..."
            ],
        )

    def test_pr(self):
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA)), OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '--issue', '{}/show_bug.cgi?id=2'.format(self.BUGZILLA), '-v', '--pr'),
                path=self.path,
            )
            self.assertEqual(0, result)
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(True, 'Unreviewed, reverting 5@main (d8bce26fa65c)' in repo.head.message)
            self.assertEqual(
                captured.stdout.getvalue(),
                "Created the local development branch 'eng/Example-feature-1'\n"
                "Created 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!\n"
                "https://github.example.com/WebKit/WebKit/pull/1\n"
            )

    def test_land_safe(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('y', 'n'), self.webserver(
            approved=True, labels={'merge-queue': dict(color='3AE653', description="Send PR to merge-queue")},
            environment=Environment(
                GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
                GITHUB_EXAMPLE_COM_TOKEN='token',
            ),
        ) as remote, mocks.local.Svn(), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):

            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '--issue', '{}/show_bug.cgi?id=1'.format(self.BUGZILLA), '-v', '--safe'),
                path=self.path,
            )
            self.assertEqual(0, result)

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-issue-1'...",
                'Reverted 5@main',
                'Automatically relating issues...',
                'Using committed changes...',
                'Detected merging automation, using that instead of local git tooling',
                "Rebasing 'eng/Example-issue-1' on 'main'...",
                "Rebased 'eng/Example-issue-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/Example-issue-1' to 'fork'...",
                "Creating 'eng/Example-issue-1-1' as a reference branch",
                "Creating pull-request for 'eng/Example-issue-1'...",
                "Adding 'merge-queue' to 'PR 2 | Unreviewed, reverting 5@main (d8bce26fa65c)'",
            ],
        )

    def test_land_unsafe(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('y', 'n'), self.webserver(
            approved=True, labels={'merge-queue': dict(color='3AE653', description="Send PR to merge-queue"), 'unsafe-merge-queue': dict(color='3AE653', description="Send PR to unsafe-merge-queue")},
            environment=Environment(
                GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
                GITHUB_EXAMPLE_COM_TOKEN='token',
            ),
        ) as remote, mocks.local.Svn(), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):

            result = program.main(
                args=('revert', 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc', '--issue', '{}/show_bug.cgi?id=1'.format(self.BUGZILLA), '-v', '--unsafe'),
                path=self.path,
            )
            self.assertEqual(0, result)

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-issue-1'...",
                'Reverted 5@main',
                'Automatically relating issues...',
                'Using committed changes...',
                'Detected merging automation, using that instead of local git tooling',
                "Rebasing 'eng/Example-issue-1' on 'main'...",
                "Rebased 'eng/Example-issue-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/Example-issue-1' to 'fork'...",
                "Creating 'eng/Example-issue-1-1' as a reference branch",
                "Creating pull-request for 'eng/Example-issue-1'...",
                "Adding 'unsafe-merge-queue' to 'PR 2 | Unreviewed, reverting 5@main (d8bce26fa65c)'",
            ],
        )
