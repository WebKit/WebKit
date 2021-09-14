# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import os
import sys
import unittest

from webkitcorepy import OutputCapture, testing
from webkitscmpy import Commit, PullRequest, program, mocks


class TestPullRequest(unittest.TestCase):
    def test_representation(self):
        self.assertEqual('PR 123 | [scoping] Bug to fix', str(PullRequest(123, title='[scoping] Bug to fix')))
        self.assertEqual('PR 1234', str(PullRequest(1234)))

    def test_create_body_single(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix\n\nReviewed by Tim Contributor.\n',
            )]), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
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

    def test_create_body_multiple(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix (Part 2)\n\nReviewed by Tim Contributor.\n',
            ), Commit(
                hash='53ea230fcedbce327eb1c45a6ab65a88de864505',
                message='[scoping] Bug to fix (Part 1)\n\nReviewed by Tim Contributor.\n',
            )]), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix (Part 2)

Reviewed by Tim Contributor.
```
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
```
[scoping] Bug to fix (Part 1)

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

    def test_create_body_empty(self):
        self.assertEqual(
            PullRequest.create_body(None, [Commit(hash='11aa76f9fc380e9fe06157154f32b304e8dc4749')]),
            '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
???
```''',
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

    def test_create_body_comment(self):
        self.assertEqual(
            PullRequest.create_body('Comment body', [Commit(
                hash='11aa76f9fc380e9fe06157154f32b304e8dc4749',
                message='[scoping] Bug to fix\n\nReviewed by Tim Contributor.\n',
            )]), '''Comment body

----------------------------------------------------------------------
#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
```
[scoping] Bug to fix

Reviewed by Tim Contributor.
```''',
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


class TestDoPullRequest(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestDoPullRequest, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path):
            self.assertEqual(1, program.main(
                args=('pull-request',),
                path=self.path,
            ))
        self.assertEqual(captured.root.log.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "Can only 'pull-request' on a native Git repository\n")

    def test_no_modified(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('pull-request', '-i', 'pr-branch'),
                path=self.path,
            ))
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/pr-branch'...\n")
        self.assertEqual(captured.stderr.getvalue(), 'No modified files\n')

    def test_staged(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn():
            repo.staged['added.txt'] = 'added'
            self.assertEqual(1, program.main(
                args=('pull-request', '-i', 'pr-branch'),
                path=self.path,
            ))
            self.assertDictEqual(repo.staged, {})
            self.assertEqual(repo.head.hash, 'e4390abc95a2026370b8c9813b7e55c61c5d6ebb')

        self.assertEqual(
            '\n'.join([line for line in captured.root.log.getvalue().splitlines() if 'Mock process' not in line]),
            """Creating the local development branch 'eng/pr-branch'...
Creating commit...
    Found 1 commit...
Rebasing 'eng/pr-branch' on 'main'...
Rebased 'eng/pr-branch' on 'main!'""")
        self.assertEqual(captured.stderr.getvalue(), "'{}' doesn't have a recognized remote\n".format(self.path))

    def test_modified(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn():
            repo.modified['modified.txt'] = 'diff'
            self.assertEqual(1, program.main(
                args=('pull-request', '-i', 'pr-branch'),
                path=self.path,
            ))
            self.assertDictEqual(repo.modified, dict())
            self.assertDictEqual(repo.staged, dict())
            self.assertEqual(repo.head.hash, 'd05082bf6707252aef3472692598a587ed3fb213')

        self.assertEqual(captured.stderr.getvalue(), "'{}' doesn't have a recognized remote\n".format(self.path))
        self.assertEqual(
            '\n'.join([line for line in captured.root.log.getvalue().splitlines() if 'Mock process' not in line]),
            """Creating the local development branch 'eng/pr-branch'...
    Adding modified.txt...
Creating commit...
    Found 1 commit...
Rebasing 'eng/pr-branch' on 'main'...
Rebased 'eng/pr-branch' on 'main!'""")

    def test_github(self):
        with OutputCapture() as captured, mocks.remote.GitHub() as remote, \
                mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo, mocks.local.Svn():

            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'pr-branch'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                'Creating commit...',
                '    Found 1 commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Creating pull-request for 'eng/pr-branch'...",
                "Created 'PR 1 | Created commit'!",
            ],
        )

    def test_github_update(self):
        with mocks.remote.GitHub() as remote, mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo, mocks.local.Svn():
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            with OutputCapture() as captured:
                repo.staged['added.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request',),
                    path=self.path,
                ))

        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.maxDiff = None
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Amending commit...",
                '    Found 1 commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                "Pushing 'eng/pr-branch' to 'fork'...",
                "Updating pull-request for 'eng/pr-branch'...",
                "Updated 'PR 1 | Amended commit'!",
            ],
        )

    def test_bitbucket(self):
        with OutputCapture() as captured, mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn():

            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('pull-request', '-i', 'pr-branch'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Creating the local development branch 'eng/pr-branch'...",
                'Creating commit...',
                '    Found 1 commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Creating pull-request for 'eng/pr-branch'...",
                "Created 'PR 1 | Created commit'!",
            ],
        )

    def test_bitbucket_update(self):
        with mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn():
            with OutputCapture():
                repo.staged['added.txt'] = 'added'
                self.assertEqual(0, program.main(
                    args=('pull-request', '-i', 'pr-branch'),
                    path=self.path,
                ))

            with OutputCapture() as captured:
                repo.staged['added.txt'] = 'diff'
                self.assertEqual(0, program.main(
                    args=('pull-request',),
                    path=self.path,
                ))

        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                "Amending commit...",
                '    Found 1 commit...',
                "Rebasing 'eng/pr-branch' on 'main'...",
                "Rebased 'eng/pr-branch' on 'main!'",
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Updating pull-request for 'eng/pr-branch'...",
                "Updated 'PR 1 | Amended commit'!",
            ],
        )
