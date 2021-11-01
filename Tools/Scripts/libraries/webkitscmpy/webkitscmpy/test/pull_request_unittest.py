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
from webkitscmpy import Contributor, Commit, PullRequest, program, mocks, remote


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
                message='[scoping] Bug to fix (Part 2)\nhttps://bugs.webkit.org/1234\n\nReviewed by Tim Contributor.\n',
            ), Commit(
                hash='53ea230fcedbce327eb1c45a6ab65a88de864505',
                message='[scoping] Bug to fix (Part 1)\n<http://bugs.webkit.org/1234>\n\nReviewed by Tim Contributor.\n',
            )]), '''#### 11aa76f9fc380e9fe06157154f32b304e8dc4749
<pre>
[scoping] Bug to fix (Part 2)
<a href="https://bugs.webkit.org/1234">https://bugs.webkit.org/1234</a>

Reviewed by Tim Contributor.
</pre>
----------------------------------------------------------------------
#### 53ea230fcedbce327eb1c45a6ab65a88de864505
<pre>
[scoping] Bug to fix (Part 1)
&lt;<a href="http://bugs.webkit.org/1234">http://bugs.webkit.org/1234</a> &gt;

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
        self.maxDiff = None
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
&lt;<a href="http://bugs.webkit.org/1234">http://bugs.webkit.org/1234</a> &gt;

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
Rebased 'eng/pr-branch' on 'main!'
    Found 1 commit...""")
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
Rebased 'eng/pr-branch' on 'main!'
    Found 1 commit...""")

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
                "    Found 1 commit...",
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
                "    Found 1 commit...",
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
                "    Found 1 commit...",
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
                "    Found 1 commit...",
                "Pushing 'eng/pr-branch' to 'origin'...",
                "Updating pull-request for 'eng/pr-branch'...",
                "Updated 'PR 1 | Amended commit'!",
            ],
        )


class TestNetworkPullRequestGitHub(unittest.TestCase):
    remote = 'https://github.example.com/WebKit/WebKit'

    @classmethod
    def webserver(cls):
        result = mocks.remote.GitHub()
        result.users = dict(
            ereviewer=Contributor('Eager Reviewer', ['ereviewer@webkit.org'], github='ereviewer'),
            rreviewer=Contributor('Reluctant Reviewer', ['rreviewer@webkit.org'], github='rreviewer'),
            sreviewer=Contributor('Suspicious Reviewer', ['sreviewer@webkit.org'], github='sreviewer'),
            tcontributor=Contributor('Tim Contributor', ['tcontributor@webkit.org']),
        )
        result.issues = {
            1: dict(
                comments=[],
                assignees=[],
            )
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
            ),
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
