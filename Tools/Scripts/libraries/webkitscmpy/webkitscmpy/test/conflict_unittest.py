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


class TestConflict(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestConflict, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_checkout_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('conflict', '1234'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_conflict_not_found(self):
        with OutputCapture() as captured, mocks.remote.GitHub() as remote, mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)):
            self.assertEqual(1, program.main(
                args=('conflict', '1234'),
                path=self.path,
            ))

            self.assertEqual(captured.stderr.getvalue(), 'No conflict pull request found with branch integration/conflict/1234\n')

    def test_conflict_found(self):
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
                head=dict(ref='tcontributor:integration/conflict/1234'),
                base=dict(ref='main'),
                requested_reviews=[dict(login='rreviewer')],
                reviews=[dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')],
                draft=False,
            )]
            repo.edit_config('remote.tcontributor.url', 'https://github.com/tcontributor/WebKit')
            repo.commits['remotes/tcontributor/integration/conflict/1234'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier="3.1@integration/conflict/1234",
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]
            self.assertEqual('d8bce26fa65c6fc8f39c17927abb77f69fab82fc', local.Git(self.path).commit().hash)

            self.assertEqual('integration/conflict/1234', program.main(
                args=('conflict', '1234'),
                path=self.path,
            ).branch)

            self.assertEqual('a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', local.Git(self.path).commit().hash)
