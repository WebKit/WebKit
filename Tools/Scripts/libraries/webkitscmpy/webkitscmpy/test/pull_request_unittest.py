# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
import unittest

from mock import patch
from webkitbugspy import Tracker, bugzilla, github, radar, mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Terminal as MockTerminal, Environment
from webkitscmpy import Contributor, Commit, PullRequest, local, program, mocks, remote


class TestPullRequest(unittest.TestCase):
    def test_representation(self):
        self.assertEqual('PR 123 | [scoping] Bug to fix', str(PullRequest(123, title='[scoping] Bug to fix')))
        self.assertEqual('PR 1234', str(PullRequest(1234)))

    def test_create_body_single_linked(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix\n\nReviewed by Tim Contributor.\n',
            )]), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
[scoping] Bug to fix

Reviewed by Tim Contributor.
</pre>''',
        )

    def test_create_body_single_no_link(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix\n\nReviewed by Tim Contributor.\n',
            )], linkify=False), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''',
        )

    def test_parse_body_single(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix\n\nReviewed by Tim Contributor.')

    def test_create_body_multiple_linked(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix (Part 3)\nhttps://bugs.webkit.org/1234\n\nReviewed by Tim Contributor.\n',
            ), Commit(
                hash='53ea230fcedbce327eb1c45a6ab65a88de864505',
                message='[scoping] Bug to fix (Part 2)\n<http://bugs.webkit.org/1234>\n\nReviewed by Tim Contributor.\n',
            ), Commit(
                hash='ccc39e76f938a1685e388991fc3127a85d0be0f0',
                message='[scoping] Bug to fix (Part 1)\n<rdar:///1234>\n\nReviewed by Tim Contributor.\n',
            )]), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
[scoping] Bug to fix (Part 3)
<a href="https://bugs.webkit.org/1234">https://bugs.webkit.org/1234</a>

Reviewed by Tim Contributor.
</pre>
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
<pre>
[scoping] Bug to fix (Part 2)
&lt;<a href="http://bugs.webkit.org/1234">http://bugs.webkit.org/1234</a>&gt;

Reviewed by Tim Contributor.
</pre>
----------------------------------------------------------------------
#### ccc39e76f938a1685e388991fc3127a85d0be0f0
<pre>
[scoping] Bug to fix (Part 1)
&lt;rdar:///1234&gt;

Reviewed by Tim Contributor.
</pre>''',
        )

    def test_create_body_multiple_no_link(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix (Part 2)\nhttps://bugs.webkit.org/1234\n\nReviewed by Tim Contributor.\n',
            ), Commit(
                hash='53ea230fcedbce327eb1c45a6ab65a88de864505',
                message='[scoping] Bug to fix (Part 1)\n<http://bugs.webkit.org/1234>\n\nReviewed by Tim Contributor.\n',
            )], linkify=False), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix (Part 2)
https://bugs.webkit.org/1234

Reviewed by Tim Contributor.
```
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
```
[scoping] Bug to fix (Part 1)
<http://bugs.webkit.org/1234>

Reviewed by Tim Contributor.
```''',
        )

    def test_parse_body_multiple(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix (Part 2)

Reviewed by Tim Contributor.
```
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
```
[scoping] Bug to fix (Part 1)

Reviewed by Tim Contributor.
```''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 2)

        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix (Part 2)\n\nReviewed by Tim Contributor.')

        self.assertEqual(commits[1].hash, '53ea230fcedbce327eb1c45a6ab65a88de864505')
        self.assertEqual(commits[1].message, '[scoping] Bug to fix (Part 1)\n\nReviewed by Tim Contributor.')

    def test_parse_html_body_multiple(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
[scoping] Bug to fix (Part 2)
<a href="https://bugs.webkit.org/1234">https://bugs.webkit.org/1234</a>

Reviewed by Tim Contributor.
</pre>
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
<pre>
[scoping] Bug to fix (Part 1)
&lt;<a href="http://bugs.webkit.org/1234">http://bugs.webkit.org/1234</a>&gt;

Reviewed by Tim Contributor.
</pre>''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 2)

        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix (Part 2)\nhttps://bugs.webkit.org/1234\n\nReviewed by Tim Contributor.')

        self.assertEqual(commits[1].hash, '53ea230fcedbce327eb1c45a6ab65a88de864505')
        self.assertEqual(commits[1].message, '[scoping] Bug to fix (Part 1)\n<http://bugs.webkit.org/1234>\n\nReviewed by Tim Contributor.')

    def test_create_body_empty(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(hash='11aa76f9fc380e9fe06157154f32b304e8dc4749')]),
            '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
???
</pre>''',
        )

    def test_parse_body_empty(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
???
```''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, None)

    def test_parse_html_body_empty(self):
        body, commits = PullRequest.parse_body('''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
???
</pre>''')
        self.assertIsNone(body)
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, None)

    def test_create_body_comment(self):
        self.assertEqual(
            PullRequest.create_body('Comment body', [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix\n\nReviewed by Tim Contributor.\n',
            )]), '''Comment body

----------------------------------------------------------------------
#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
[scoping] Bug to fix

Reviewed by Tim Contributor.
</pre>''',
        )

    def test_parse_body_single(self):
        body, commits = PullRequest.parse_body('''Comment body

----------------------------------------------------------------------
#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''')
        self.assertEqual(body, 'Comment body')
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix\n\nReviewed by Tim Contributor.')

    def test_parse_html_body_single(self):
        body, commits = PullRequest.parse_body('''Comment body

----------------------------------------------------------------------
#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
[scoping] Bug to fix

Reviewed by Tim Contributor.
</pre>''')
        self.assertEqual(body, 'Comment body')
        self.assertEqual(len(commits), 1)
        self.assertEqual(commits[0].hash, '11aa76f9fc380e9fe06157154f32b304e8dc4749')
        self.assertEqual(commits[0].message, '[scoping] Bug to fix\n\nReviewed by Tim Contributor.')


class TestDoPullRequest(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestDoPullRequest, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn(self.path), patch('webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('pull-request',),
                path=self.path,
            ))
        self.assertEqual(captured.root.log.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "Can only 'pull-request' on a native Git repository\n")

    def test_none(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('pull-request',),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "Can only 'pull-request' on a native Git repository\n")

    def test_no_modified(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('pull-request', '-i', 'pr-branch', '-v'),
                path=self.path,
            ))
        self.assertEqual(
            '\n'.join([line for line in captured.root.log.getvalue().splitlines() if 'Mock process' not in line]),
            "Creating the local development branch 'eng/pr-branch'...",
        )
        self.assertEqual(captured.stderr.getvalue(), 'No modified files\n')

    def test_staged(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            repo.staged['added.txt'] = 'added'
            self.assertEqual(1, program.main(
                args=('pull-request', '-i', 'pr-branch', '-v'),
                path=self.path,
            ))
            self.assertDictEqual(repo.staged, {})
            self.assertEqual(repo.head.hash, 'c28f53f7fabd7bd9535af890cb7dc473cb342999')

        self.assertEqual(
            '\n'.join([line for line in captured.root.log.getvalue().splitlines() if 'Mock process' not in line]),
            """Creating the local development branch 'eng/pr-branch'...
Creating commit...
Rebasing 'eng/pr-branch' on 'main'...
Rebased 'eng/pr-branch' on 'main!'
Running pre-PR checks...
No pre-PR checks to run""")
        self.assertEqual(captured.stdout.getvalue(), "Created the local development branch 'eng/pr-branch'\n")
        self.assertEqual(captured.stderr.getvalue(), "'{}' doesn't have a recognized remote\n".format(self.path))

    def test_modified(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            repo.modified['modified.txt'] = 'diff'
            self.assertEqual(1, program.main(
                args=('pull-request', '-i', 'pr-branch', '-v'),
                path=self.path,
            ))
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(repo.head.hash, '488ea15fdbafb3ddfe827f913776208ad3217d79')

        self.assertEqual(captured.stderr.getvalue(), "'{}' doesn't have a recognized remote\n".format(self.path))
        self.assertEqual(
            '\n'.join([line for line in captured.root.log.getvalue().splitlines() if 'Mock process' not in line]),
            """Creating the local development branch 'eng/pr-branch'...
    Adding modified.txt...
Creating commit...
Rebasing 'eng/pr-branch' on 'main'...
Rebased 'eng/pr-branch' on 'main!'
Running pre-PR checks...
No pre-PR checks to run""")

    def test_github(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):

            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'pr-branch', '-v', '--no-history'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/pr-branch'\n"
            "Created 'PR 1 | [Testing] Creating commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                'Creating commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_github_draft(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):

            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'pr-branch', '-v', '--no-history', '--draft'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, True)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/pr-branch'\n"
            "Created 'PR 1 | [Testing] Creating commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                'Creating commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_github_update(self):
        with mocks.remote.GitHub(labels={
            'merging-blocked': dict(color='c005E5', description='Applied to prevent a change from being merged'),
        }) as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            github_tracker = github.Tracker('https://{}'.format(remote.remote))
            self.assertEqual(github_tracker.issue(1).labels, [])
            github_tracker.issue(1).set_labels(['merging-blocked'])

            with OutputCapture(level=logging.INFO) as captured:
                repo.staged['added.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-v', '--no-history'),
                    path=self.path,
                ))

            self.assertEqual(github_tracker.issue(1).labels, [])

        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 1 | [Testing] Amending commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Amending commit...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                'Checking PR labels for active labels...',
                "Removing 'merging-blocked' from PR #1...",
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Updating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_github_append(self):
        with mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            with OutputCapture(level=logging.INFO) as captured:
                repo.staged['modified.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-v', '--no-history', '--append'),
                    path=self.path,
                ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 1 | [Testing] Creating commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                'Creating commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                'Checking PR labels for active labels...',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Updating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_github_reopen(self):
        with mocks.remote.GitHub() as remote, mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            local.Git(self.path).remote().pull_requests.get(1).close()
            self.assertFalse(local.Git(self.path).remote().pull_requests.get(1).opened)

            with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('n'):
                repo.staged['added.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-v', '--no-history'),
                    path=self.path,
                ))

            self.assertTrue(local.Git(self.path).remote().pull_requests.get(1).opened)

        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/pr-branch' is already associated with 'PR 1 | [Testing] Creating commits', which is closed.\n"
            'Would you like to create a new pull-request? (Yes/[No]): \n'
            "Updated 'PR 1 | [Testing] Amending commits'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Amending commit...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                'Checking PR labels for active labels...',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Updating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_github_bugzilla(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(projects=bmocks.PROJECTS) as remote, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )), patch(
                'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():

            repo.commits['eng/pr-branch'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/pr-branch',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/pr-branch",
                    timestamp=int(time.time()),
                    message='[Testing] Existing commit\nbugs.example.com/show_bug.cgi?id=1'
                )
            ]
            repo.head = repo.commits['eng/pr-branch'][-1]
            self.assertEqual(0, program.main(
                args=('pull-request', '-v', '--no-history'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Pull request: https://github.example.com/WebKit/WebKit/pull/1',
            )
            gh_issue = github.Tracker('https://github.example.com/WebKit/WebKit').issue(1)
            self.assertEqual(gh_issue.project, 'WebKit')
            self.assertEqual(gh_issue.component, 'Text')

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | [Testing] Existing commit'!\n"
            'Posted pull request link to https://bugs.example.com/show_bug.cgi?id=1\n'
            'https://github.example.com/WebKit/WebKit/pull/1\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Using committed changes...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/pr-branch'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
                'Syncing PR labels with issue component...',
                'Synced PR labels with issue component!',
            ],
        )

    def test_github_branch_bugzilla(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(
                projects=bmocks.PROJECTS) as remote, bmocks.Bugzilla(
                self.BUGZILLA.split('://')[-1],
                projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
                environment=Environment(
                    BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                    BUGS_EXAMPLE_COM_PASSWORD='password',
                )), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():
            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'https://bugs.example.com/show_bug.cgi?id=1', '-v', '--no-history'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Pull request: https://github.example.com/WebKit/WebKit/pull/1',
            )
            gh_issue = github.Tracker('https://github.example.com/WebKit/WebKit').issue(1)
            self.assertEqual(gh_issue.project, 'WebKit')
            self.assertEqual(gh_issue.component, 'Text')

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/Example-issue-1'\n"
            "Created 'PR 1 | Example issue 1'!\n"
            "Posted pull request link to https://bugs.example.com/show_bug.cgi?id=1\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-issue-1'...",
                'Creating commit...',
                "Rebasing 'eng/Example-issue-1' on 'main'...",
                "Rebased 'eng/Example-issue-1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/Example-issue-1' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/Example-issue-1'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
                'Syncing PR labels with issue component...',
                'Synced PR labels with issue component!',
            ],
        )

    def test_github_bugzilla_commit_on_main(self):
        # This test doesn't mock the whole process because we don't have a functional remote
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(projects=bmocks.PROJECTS) as remote, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )), patch(
                'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():

            repo.commits['main'].append(Commit(
                hash='5d697791f2e1fb3991e441fbd79f412fcff93c75',
                branch='main',
                author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                identifier="6@main",
                timestamp=int(time.time()),
                message='[Testing] Existing commit\nbugs.example.com/show_bug.cgi?id=1'
            ))
            repo.head = repo.commits['main'][-1]
            self.assertEqual(1, program.main(
                args=('pull-request', '-v', '--no-history'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), 'No modified files\n')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/Example-issue-1'\n",
        )
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/Example-issue-1'...",
            ],
        )

    def test_github_branch_bugzilla_redacted_to_origin(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(projects=bmocks.PROJECTS) as remote, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )), patch(
                'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA, redact={'.*': True})],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), MockTerminal.input('y'):

            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'https://bugs.example.com/show_bug.cgi?id=1', '-v', '--no-history', '--remote',  'origin'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Pull request: https://github.example.com/WebKit/WebKit/pull/1',
            )
            gh_issue = github.Tracker('https://github.example.com/WebKit/WebKit').issue(1)
            self.assertEqual(gh_issue.project, 'WebKit')
            self.assertEqual(gh_issue.component, 'Text')

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/1'\n"
            "Created 'PR 1 | Example issue 1'!\n"
            "Posted pull request link to https://bugs.example.com/show_bug.cgi?id=1\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/1'...",
                'Creating commit...',
                "Rebasing 'eng/1' on 'main'...",
                "Rebased 'eng/1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/1' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/1'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
                'Syncing PR labels with issue component...',
                'Synced PR labels with issue component!',
            ],
        )

    def test_github_branch_bugzilla_redacted(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(projects=bmocks.PROJECTS) as remote, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )), patch(
                'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA, redact={'.*': True})],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn(), MockTerminal.input('y'):

            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'https://bugs.example.com/show_bug.cgi?id=1', '-v', '--no-history'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Pull request: https://github.example.com/WebKit/WebKit/pull/1',
            )
            gh_issue = github.Tracker('https://github.example.com/WebKit/WebKit').issue(1)
            self.assertEqual(gh_issue.project, 'WebKit')
            self.assertEqual(gh_issue.component, 'Text')

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/1'\n"
            "https://bugs.example.com/show_bug.cgi?id=1 is considered the primary issue for your pull request\n"
            "https://bugs.example.com/show_bug.cgi?id=1 is a Bugzilla and is thus redacted\n"
            "Pull request needs to be sent to a secure remote for review\n"
            "Would you like to proceed anyways? \n"
            " (Yes/[No]): \n"
            "Created 'PR 1 | Example issue 1'!\n"
            "Posted pull request link to https://bugs.example.com/show_bug.cgi?id=1\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n",
        )
        self.assertEqual(captured.stderr.getvalue(), 'Error. You do not have access to a secure remote to make a pull request for a redacted issue\n')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/1'...",
                'Creating commit...',
                "Rebasing 'eng/1' on 'main'...",
                "Rebased 'eng/1' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/1' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/1'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
                'Syncing PR labels with issue component...',
                'Synced PR labels with issue component!',
            ],
        )

    def test_github_reopen_bugzilla(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )), patch(
                'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():

            Tracker.instance().issue(1).close(why='Looks like we will not get to this')
            repo.commits['eng/pr-branch'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/pr-branch',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/pr-branch",
                    timestamp=int(time.time()),
                    message='[Testing] Existing commit\nbugs.example.com/show_bug.cgi?id=1'
                )
            ]
            repo.head = repo.commits['eng/pr-branch'][-1]
            self.assertEqual(0, program.main(
                args=('pull-request', '-v', '--no-history'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Re-opening for pull request https://github.example.com/WebKit/WebKit/pull/1',
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | [Testing] Existing commit'!\n"
            'Posted pull request link to https://bugs.example.com/show_bug.cgi?id=1\n'
            'https://github.example.com/WebKit/WebKit/pull/1\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Using committed changes...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/pr-branch'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
                'Syncing PR labels with issue component...',
                'No label syncing required',
            ],
        )

    def test_github_bugzilla_and_radar(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(projects=bmocks.PROJECTS) as remote, bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), bmocks.Radar(issues=bmocks.ISSUES), patch(
            'webkitbugspy.Tracker._trackers', [
                bugzilla.Tracker(self.BUGZILLA, radar_importer=bmocks.USERS['Radar WebKit Bug Importer']),
                radar.Tracker(),
            ],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():

            repo.commits['eng/pr-branch'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/pr-branch',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/pr-branch",
                    timestamp=int(time.time()),
                    message='[Testing] Existing commit\nbugs.example.com/show_bug.cgi?id=1\n<rdar://problem/1>\n'
                )
            ]
            repo.head = repo.commits['eng/pr-branch'][-1]
            self.assertEqual(0, program.main(
                args=('pull-request', '-v', '--no-history'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-2].content,
                '<rdar://problem/1>',
            )
            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Pull request: https://github.example.com/WebKit/WebKit/pull/1',
            )
            gh_issue = github.Tracker('https://github.example.com/WebKit/WebKit').issue(1)
            self.assertEqual(gh_issue.project, 'WebKit')
            self.assertEqual(gh_issue.component, 'Text')

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | [Testing] Existing commit'!\n"
            'Posted pull request link to https://bugs.example.com/show_bug.cgi?id=1\n'
            'https://github.example.com/WebKit/WebKit/pull/1\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Using committed changes...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'CCing Radar WebKit Bug Importer',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Syncing 'main' to remote 'fork'",
                "Creating pull-request for 'eng/pr-branch'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
                'Syncing PR labels with issue component...',
                'Synced PR labels with issue component!',
            ],
        )

    def test_bitbucket(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):

            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'pr-branch', '-v'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).remote().pull_requests.get(1).draft, False)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/pr-branch'\n"
            "Created 'PR 1 | [Testing] Creating commits'!\n"
            "https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                'Creating commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Creating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_bitbucket_draft(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):

            repo.staged['added.txt'] = 'added'
            self.assertEqual(1, program.main(
                args=('pull-request', '-i', 'pr-branch', '-v', '--draft'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created the local development branch 'eng/pr-branch'\n",
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "'https://bitbucket.example.com/projects/WEBKIT/repos/webkit' does not support draft pull requests, aborting\n",
        )
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                'Creating commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'origin'...",
            ],
        )

    def test_bitbucket_update(self):
        with mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            with OutputCapture(level=logging.INFO) as captured:
                repo.staged['added.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-v'),
                    path=self.path,
                ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 1 | [Testing] Amending commits'!\n"
            "https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview\n"
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Amending commit...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Updating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_bitbucket_append(self):
        with mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            with OutputCapture(level=logging.INFO) as captured:
                repo.staged['modified.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-v', '--append'),
                    path=self.path,
                ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 1 | [Testing] Creating commits'!\n"
            "https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview\n"
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                'Creating commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Updating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_bitbucket_reopen(self):
        with mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            local.Git(self.path).remote().pull_requests.get(1).close()
            self.assertFalse(local.Git(self.path).remote().pull_requests.get(1).opened)

            with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('n'):
                repo.staged['added.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-v'),
                    path=self.path,
                ))

            self.assertTrue(local.Git(self.path).remote().pull_requests.get(1).opened)

        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/pr-branch' is already associated with 'PR 1 | [Testing] Creating commits', which is closed.\n"
            'Would you like to create a new pull-request? (Yes/[No]): \n'
            "Updated 'PR 1 | [Testing] Amending commits'!\n"
            "https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Amending commit...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Updating pull-request for 'eng/pr-branch'...",
            ],
        )

    def test_bitbucket_radar(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.BitBucket() as remote, mocks.local.Git(
            self.path, remote='ssh://git@{}/{}/{}.git'.format(remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3]),
        ) as repo, mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            repo.commits['eng/pr-branch'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/pr-branch',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/pr-branch",
                    timestamp=int(time.time()),
                    message='<rdar://problem/1> [Testing] Existing commit\n'
                )
            ]
            repo.head = repo.commits['eng/pr-branch'][-1]
            self.assertEqual(0, program.main(
                args=('pull-request', '-v', '--no-history'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Pull request: https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview',
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | <rdar://problem/1> [Testing] Existing commit'!\n"
            'Posted pull request link to rdar://1\n'
            'https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Using committed changes...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Creating pull-request for 'eng/pr-branch'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
            ],
        )

    def test_bitbucket_reopen_radar(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.BitBucket() as remote, mocks.local.Git(
            self.path, remote='ssh://git@{}/{}/{}.git'.format(remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3]),
        ) as repo, mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            Tracker.instance().issue(1).close(why='Looks like we will not get to this')
            repo.commits['eng/pr-branch'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/pr-branch',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/pr-branch",
                    timestamp=int(time.time()),
                    message='<rdar://problem/1> [Testing] Existing commit\n'
                )
            ]
            repo.head = repo.commits['eng/pr-branch'][-1]
            self.assertEqual(0, program.main(
                args=('pull-request', '-v', '--no-history'),
                path=self.path,
            ))

            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Re-opening for pull request https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview',
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | <rdar://problem/1> [Testing] Existing commit'!\n"
            'Posted pull request link to rdar://1\n'
            'https://bitbucket.example.com/projects/WEBKIT/repos/webkit/pull-requests/1/overview\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    Found 1 commit...',
                "Using committed changes...",
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR not found.',
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Creating pull-request for 'eng/pr-branch'...",
                'Checking issue assignee...',
                'Checking for pull request link in associated issue...',
            ],
        )


class TestNetworkPullRequestGitHub(unittest.TestCase):
    remote = 'https://github.example.com/WebKit/WebKit'

    @classmethod
    def webserver(cls):
        result = mocks.remote.GitHub()
        result.users.create('Eager Reviewer', 'ereviewer', ['ereviewer@webkit.org'])
        result.users.create('Reluctant Reviewer', 'rreviewer', ['rreviewer@webkit.org'])
        result.users.create('Suspicious Reviewer', 'sreviewer', ['sreviewer@webkit.org'])
        result.users.create('Tim Contributor', 'tcontributor', ['tcontributor@webkit.org'])
        result.issues = {
            1: dict(
                number=1,
                opened=True,
                title='Example Change',
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
            title='Example Change',
            user=dict(login='tcontributor'),
            body='''#### 95507e3a1a4a919d1a156abbc279fdf6d24b13f5
<pre>
Example Change
<a href="https://bugs.webkit.org/show_bug.cgi?id=1234">https://bugs.webkit.org/show_bug.cgi?id=1234</a>

Reviewed by NOBODY (OOPS!).

* Source/file.cpp:
</pre>
''',
            head=dict(ref='eng/pull-request'),
            base=dict(ref='main'),
            requested_reviews=[dict(login='rreviewer')],
            reviews=[
                dict(user=dict(login='ereviewer'), state='APPROVED'),
                dict(user=dict(login='sreviewer'), state='CHANGES_REQUESTED'),
            ], _links=dict(
                issue=dict(href='https://{}/issues/1'.format(result.api_remote)),
            ), draft=False,
        )]
        return result

    def test_find(self):
        with self.webserver():
            prs = list(remote.GitHub(self.remote).pull_requests.find())
            self.assertEqual(len(prs), 1)
            self.assertEqual(prs[0].number, 1)
            self.assertEqual(prs[0].title, 'Example Change')
            self.assertEqual(prs[0].head, 'eng/pull-request')
            self.assertEqual(prs[0].base, 'main')

    def test_get(self):
        with self.webserver():
            pr = remote.GitHub(self.remote).pull_requests.get(1)
            self.assertEqual(pr.number, 1)
            self.assertTrue(pr.opened)
            self.assertEqual(pr.title, 'Example Change')
            self.assertEqual(pr.head, 'eng/pull-request')
            self.assertEqual(pr.base, 'main')
            self.assertEqual(pr.draft, False)

    def test_reviewers(self):
        with self.webserver():
            pr = remote.GitHub(self.remote).pull_requests.get(1)
            self.assertEqual(pr.reviewers, [
                Contributor('Eager Reviewer', ['ereviewer@webkit.org']),
                Contributor('Reluctant Reviewer', ['rreviewer@webkit.org']),
                Contributor('Suspicious Reviewer', ['sreviewer@webkit.org']),
            ])
            self.assertEqual(pr.approvers, [Contributor('Eager Reviewer', ['ereviewer@webkit.org'])])
            self.assertEqual(pr.blockers, [Contributor('Suspicious Reviewer', ['sreviewer@webkit.org'])])

    def test_approved_edits(self):
        with self.webserver() as server:
            server.pull_requests[0]['reviews'].append(dict(user=dict(login='sreviewer'), state='APPROVED'))
            pr = remote.GitHub(self.remote).pull_requests.get(1)
            self.assertEqual(sorted(pr.approvers), sorted([
                Contributor('Eager Reviewer', ['ereviewer@webkit.org']),
                Contributor('Suspicious Reviewer', ['sreviewer@webkit.org']),
            ]))

    def test_approvers_status(self):
        with self.webserver():
            repo = remote.GitHub(self.remote)
            repo.contributors.add(Contributor(
                'Suspicious Reviewer', ['sreviewer@webkit.org'],
                github='sreviewer', status=Contributor.REVIEWER,
            ))
            pr = repo.pull_requests.get(1)
            self.assertEqual(pr.reviewers, [
                Contributor('Eager Reviewer', ['ereviewer@webkit.org']),
                Contributor('Reluctant Reviewer', ['rreviewer@webkit.org']),
                Contributor('Suspicious Reviewer', ['sreviewer@webkit.org']),
            ])
            self.assertEqual(pr.approvers, [])
            self.assertEqual(pr.blockers, [Contributor('Suspicious Reviewer', ['sreviewer@webkit.org'])])

    def test_comments(self):
        with self.webserver():
            repo = remote.GitHub(self.remote)
            pr = repo.pull_requests.get(1)
            self.assertEqual(pr.comments, [])
            pr.comment('Commenting!')
            self.assertEqual([c.content for c in pr.comments], ['Commenting!'])

    def test_open_close(self):
        with self.webserver():
            repo = remote.GitHub(self.remote)
            pr = repo.pull_requests.get(1)
            self.assertTrue(pr.opened)
            pr.close()
            self.assertFalse(pr.opened)
            pr.open()
            self.assertTrue(pr.opened)


class TestNetworkPullRequestBitBucket(unittest.TestCase):
    remote = 'https://bitbucket.example.com/projects/WEBKIT/repos/webkit'

    @classmethod
    def webserver(cls):
        result = mocks.remote.BitBucket()
        result.pull_requests = [dict(
            id=1,
            state='OPEN',
            open=True,
            closed=False,
            activities=[],
            title='Example Change',
            author=dict(
                user=dict(
                    name='tcontributor',
                    emailAddress='tcontributor@apple.com',
                    displayName='Tim Contributor',
                ),
            ), body='''#### 95507e3a1a4a919d1a156abbc279fdf6d24b13f5
```
Example Change
https://bugs.webkit.org/show_bug.cgi?id=1234

Reviewed by NOBODY (OOPS!).

* Source/file.cpp:
```
''',
            fromRef=dict(displayId='eng/pull-request'),
            toRef=dict(displayId='main'),
            reviewers=[
                dict(
                    user=dict(
                        displayName='Reluctant Reviewer',
                        emailAddress='rreviewer@webkit.org',
                    ), approved=False,
                ), dict(
                    user=dict(
                        displayName='Eager Reviewer',
                        emailAddress='ereviewer@webkit.org',
                    ), approved=True,
                ), dict(
                    user=dict(
                        displayName='Suspicious Reviewer',
                        emailAddress='sreviewer@webkit.org',
                    ), status='NEEDS_WORK',
                ),
            ],
        )]
        return result

    def test_find(self):
        with self.webserver():
            with self.webserver():
                prs = list(remote.BitBucket(self.remote).pull_requests.find())
                self.assertEqual(len(prs), 1)
                self.assertEqual(prs[0].number, 1)
                self.assertEqual(prs[0].title, 'Example Change')
                self.assertEqual(prs[0].head, 'eng/pull-request')
                self.assertEqual(prs[0].base, 'main')

    def test_get(self):
        with self.webserver():
            pr = remote.BitBucket(self.remote).pull_requests.get(1)
            self.assertEqual(pr.number, 1)
            self.assertTrue(pr.opened)
            self.assertEqual(pr.title, 'Example Change')
            self.assertEqual(pr.head, 'eng/pull-request')
            self.assertEqual(pr.base, 'main')
            self.assertEqual(pr.draft, False)

    def test_reviewers(self):
        with self.webserver():
            pr = remote.BitBucket(self.remote).pull_requests.get(1)
            self.assertEqual(pr.reviewers, [
                Contributor('Eager Reviewer', ['ereviewer@webkit.org']),
                Contributor('Reluctant Reviewer', ['rreviewer@webkit.org']),
                Contributor('Suspicious Reviewer', ['sreviewer@webkit.org']),
            ])
            self.assertEqual(pr.approvers, [Contributor('Eager Reviewer', ['ereviewer@webkit.org'])])
            self.assertEqual(pr.blockers, [Contributor('Suspicious Reviewer', ['sreviewer@webkit.org'])])

    def test_approvers_status(self):
        with self.webserver():
            repo = remote.BitBucket(self.remote)
            repo.contributors.add(Contributor(
                'Suspicious Reviewer', ['sreviewer@webkit.org'],
                github='sreviewer', status=Contributor.REVIEWER,
            ))
            pr = repo.pull_requests.get(1)
            self.assertEqual(pr.reviewers, [
                Contributor('Eager Reviewer', ['ereviewer@webkit.org']),
                Contributor('Reluctant Reviewer', ['rreviewer@webkit.org']),
                Contributor('Suspicious Reviewer', ['sreviewer@webkit.org']),
            ])
            self.assertEqual(pr.approvers, [])
            self.assertEqual(pr.blockers, [Contributor('Suspicious Reviewer', ['sreviewer@webkit.org'])])

    def test_comments(self):
        with self.webserver():
            repo = remote.BitBucket(self.remote)
            pr = repo.pull_requests.get(1)
            self.assertEqual(pr.comments, [])
            pr.comment('Commenting!')
            self.assertEqual([c.content for c in pr.comments], ['Commenting!'])

    def test_open_close(self):
        with self.webserver():
            repo = remote.BitBucket(self.remote)
            pr = repo.pull_requests.get(1)
            self.assertTrue(pr.opened)
            pr.close()
            self.assertFalse(pr.opened)
            pr.open()
            self.assertTrue(pr.opened)
