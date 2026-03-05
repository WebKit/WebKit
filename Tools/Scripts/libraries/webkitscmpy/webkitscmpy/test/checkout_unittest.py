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

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks, local, Contributor, Commit


class TestCheckout(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestCheckout, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_checkout_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('checkout', '2@main'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_checkout_git(self):
        with OutputCapture(), mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual('d8bce26fa65c6fc8f39c17927abb77f69fab82fc', local.Git(self.path).commit().hash)

            self.assertEqual(0, program.main(
                args=('checkout', '2@main'),
                path=self.path,
            ))

            self.assertEqual('fff83bb2d9171b4d9196e977eb0508fd57e7a08d', local.Git(self.path).commit().hash)

    def test_checkout_git_svn(self):
        with OutputCapture(), mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual('d8bce26fa65c6fc8f39c17927abb77f69fab82fc', local.Git(self.path).commit().hash)

            self.assertEqual(0, program.main(
                args=('checkout', '2.2@branch-a'),
                path=self.path,
            ))

            self.assertEqual('621652add7fc416099bd2063366cc38ff61afe36', local.Git(self.path).commit().hash)

    def test_no_pr_github(self):
        with OutputCapture() as captured, mocks.remote.GitHub() as remote, \
                mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('checkout', 'PR-1'),
                path=self.path,
            ))

        self.assertEqual(
            "Request to 'https://api.github.example.com/repos/WebKit/WebKit/pulls/1' returned status code '404'\n"
            "Message: Not found\n"
            "Is your API token out of date? Run 'git-webkit setup' to refresh credentials\n"
            "Failed to find 'PR-1' associated with this repository\n",
            captured.stderr.getvalue(),
        )

    def test_no_pr_bitbucket(self):
        with OutputCapture() as captured, mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('checkout', 'PR-1'),
                path=self.path,
            ))

        self.assertEqual(
            "Request to 'https://bitbucket.example.com/rest/api/1.0/projects/WEBKIT/repos/webkit/pull-requests/1' returned status code '404'\n"
            "Failed to find 'PR-1' associated with this repository\n",
            captured.stderr.getvalue(),
        )

    def test_pr_github(self):
        with OutputCapture(), mocks.remote.GitHub() as remote, \
                mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo, mocks.local.Svn():
            remote.users = dict(
                rreviewer=Contributor('Ricky Reviewer', ['rreviewer@webkit.org'], github='rreviewer'),
                tcontributor=Contributor('Tim Contributor', ['tcontributor@webkit.org'], github='tcontributor'),
            )
            remote.issues = {
                1: dict(
                    comments=[],
                    assignees=[],
                )
            }
            remote.pull_requests = [dict(
                number=1,
                state='open',
                title='Example Change',
                user=dict(login='tcontributor'),
                body='''#### a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd
<pre>
To Be Committed

Reviewed by NOBODY (OOPS!).
</pre>
''',
                head=dict(ref='tcontributor:eng/example'),
                base=dict(ref='main'),
                requested_reviews=[dict(login='rreviewer')],
                reviews=[dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')],
                draft=False,
            )]
            repo.commits['remotes/tcontributor/eng/example'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier='3.1@eng/example',
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]

            self.assertEqual(0, program.main(
                args=('checkout', 'PR-1'),
                path=self.path,
            ))

            self.assertEqual('a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', local.Git(self.path).commit().hash)

    def test_checkout_specific_remote(self):
        with OutputCapture(), mocks.remote.GitHub() as remote, mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo, mocks.local.Svn():
            repo.edit_config('remote.webkit.url', 'https://github.com/WebKit/WebKit')
            repo.commits['remotes/webkit/eng/example'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier='3.1@eng/example',
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]

            self.assertEqual(0, program.main(
                args=('checkout', 'webkit:eng/example'),
                path=self.path,
            ))

            self.assertEqual('a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', local.Git(self.path).commit().hash)

    def test_checkout_ambiguous_remote(self):
        with OutputCapture(), mocks.remote.GitHub() as remote, mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo, mocks.local.Svn():
            repo.edit_config('remote.webkit.url', 'https://github.com/WebKit/WebKit')
            repo.edit_config('remote.webkit-integration.url', 'https://github.com/WebKit/WebKit')
            repo.commits['remotes/webkit-integration/eng/example'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier='3.1@eng/example',
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]

            self.assertEqual(0, program.main(
                args=('checkout', 'webkit:eng/example'),
                path=self.path,
            ))

            self.assertEqual('a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', local.Git(self.path).commit().hash)

    def test_pr_bitbucket(self):
        with OutputCapture(), mocks.remote.BitBucket() as remote, mocks.local.Git(self.path, remote='ssh://git@{}/{}/{}.git'.format(
            remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
        )) as repo, mocks.local.Svn():
            remote.pull_requests = [dict(
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
                ), body='''#### a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd
```
To Be Committed

Reviewed by NOBODY (OOPS!).
```
''',
                fromRef=dict(displayId='eng/example', id='refs/heads/eng/example'),
                toRef=dict(displayId='main', id='refs/heads/main'),
                reviewers=[
                    dict(
                        user=dict(
                            displayName='Ricky Reviewer',
                            emailAddress='rreviewer@webkit.org',
                        ), approved=False,
                        status='NEEDS_WORK',
                    ),
                ],
            )]
            repo.commits['eng/example'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier='3.1@eng/example',
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]

            self.assertEqual(0, program.main(
                args=('checkout', 'PR-1'),
                path=self.path,
            ))

            self.assertEqual('a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', local.Git(self.path).commit().hash)

    def test_checkout_svn(self):
        with OutputCapture(), mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(6, local.Svn(self.path).commit().revision)

            self.assertEqual(0, program.main(
                args=('checkout', '3@trunk'),
                path=self.path,
            ))

            self.assertEqual(4, local.Svn(self.path).commit().revision)

    def test_svn_pr(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(1, program.main(
                args=('checkout', 'PR-1'),
                path=self.path,
            ))

            self.assertEqual(
                'No pull-requests associated with repository\n',
                captured.stderr.getvalue(),
            )

    def test_checkout_remote(self):
        with OutputCapture(), mocks.remote.Svn():
            self.assertEqual(1, program.main(
                args=('-C', 'https://svn.webkit.org/repository/webkit', 'checkout', '3@trunk'),
                path=self.path,
            ))
