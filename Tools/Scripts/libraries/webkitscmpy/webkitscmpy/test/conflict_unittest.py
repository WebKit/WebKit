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
from unittest.mock import Mock, patch

from webkitbugspy import Issue, mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime

from webkitscmpy import Commit, Contributor, local, mocks, program
from webkitscmpy.program.conflict import Conflict


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

    @staticmethod
    def _mock_radar_response(*args, **kwargs):
        rdar = bmocks.Radar()
        rdar.sourceChanges = 'WebKit, merge, sha123'
        rdar.source_changes = rdar.sourceChanges.splitlines()
        rdar.id = 1234
        rdar.comments = []
        return rdar

    @staticmethod
    def _pr(number, title='Cherry-pick(s)', body='Cherry-pick(s). rdar://1234', head='integration/conflict/sha_sha', base='main', state='open'):
        return dict(
            number=number, state=state, title=title, user=dict(login='tcontributor'), body=body,
            head=dict(ref=head, repo=dict(full_name='tcontributor')),
            base=dict(ref=base), requested_reviews=[], reviews=[], draft=False,
        )

    def test_conflict_not_found(self):
        with (
            OutputCapture() as captured,
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response)
        ):
            self.assertEqual(1, program.main(
                args=('conflict', '1234'),
                path=self.path,
            ))

            self.assertEqual(captured.stderr.getvalue(), 'No conflict pull request found for rdar://1234, check out the conflict pull request linked in the merge conflict alert instead\n')

    def test_conflict_no_radar_access(self):
        with (
            OutputCapture() as captured,
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)),
            patch('webkitscmpy.program.conflict.Tracker.from_string', lambda *args, **kwargs: None)
        ):
            self.assertEqual(1, program.main(
                args=('conflict', '1234'),
                path=self.path,
            ))

            self.assertEqual(captured.stderr.getvalue(), 'Unable to access rdar://1234, check out the conflict pull request linked in the merge conflict alert instead\n')

    def test_conflict_via_comments_validates_radar_reference(self):
        radar_obj = Mock(id=1234)
        with mocks.remote.GitHub() as remote:
            remote.pull_requests = [
                self._pr(1, title='Unrelated change', body='No radar reference here.', head='eng/unrelated'),
                self._pr(2),
            ]
            radar_obj.comments = [
                Issue.Comment(user=None, timestamp=200, content='See also https://{}/pull/1'.format(remote.remote)),
                Issue.Comment(user=None, timestamp=100, content='Conflict PR: https://{}/pull/2'.format(remote.remote)),
            ]

            self.assertEqual(Conflict.find_conflict_pr_via_comments(radar_obj).number, 2)

    def test_conflict_via_comments_prefers_most_recent_open(self):
        radar_obj = Mock(id=1234)
        with mocks.remote.GitHub() as remote:
            remote.pull_requests = [self._pr(1), self._pr(2), self._pr(3, state='closed')]
            radar_obj.comments = [
                Issue.Comment(user=None, timestamp=100, content='Conflict PR: https://{}/pull/1'.format(remote.remote)),
                Issue.Comment(user=None, timestamp=200, content='Conflict PR: https://{}/pull/2'.format(remote.remote)),
                Issue.Comment(user=None, timestamp=300, content='Conflict PR: https://{}/pull/3'.format(remote.remote)),
            ]

            self.assertEqual(Conflict.find_conflict_pr_via_comments(radar_obj).number, 2)

    def test_conflict_via_comments_filters_by_branch(self):
        radar_obj = Mock(id=1234)
        with mocks.remote.GitHub() as remote:
            remote.pull_requests = [self._pr(1, base='safari-7626-branch'), self._pr(2)]
            radar_obj.comments = [
                Issue.Comment(user=None, timestamp=100, content='Conflict PR: https://{}/pull/1'.format(remote.remote)),
                Issue.Comment(user=None, timestamp=200, content='Conflict PR: https://{}/pull/2'.format(remote.remote)),
            ]

            self.assertEqual(Conflict.find_conflict_pr_via_comments(radar_obj, branch='safari-7626-branch').number, 1)

    def test_conflict_via_comments_no_matching_pr(self):
        radar_obj = Mock(id=1234)
        with mocks.remote.GitHub() as remote:
            remote.pull_requests = [self._pr(1, title='Unrelated change', body='No radar reference here.', head='eng/unrelated')]
            radar_obj.comments = [
                Issue.Comment(user=None, timestamp=100, content='See also https://{}/pull/1'.format(remote.remote)),
            ]

            self.assertIsNone(Conflict.find_conflict_pr_via_comments(radar_obj))

    def test_conflict_multiple_matches(self):
        with (
            OutputCapture() as captured,
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response)
        ):
            remote.pull_requests = [
                self._pr(1, head='integration/conflict/sha123_sha123_main'),
                self._pr(2, head='integration/conflict/sha123_sha123_safari-7626-branch', base='safari-7626-branch'),
            ]
            self.assertEqual(1, program.main(
                args=('conflict', '1234'),
                path=self.path,
            ))

            self.assertEqual(
                captured.stderr.getvalue(),
                'Multiple conflict pull requests found for rdar://1234:\n'
                '    tcontributor/WebKit:integration/conflict/sha123_sha123_main (targeting main)\n'
                '    tcontributor/WebKit:integration/conflict/sha123_sha123_safari-7626-branch (targeting safari-7626-branch)\n'
                'Re-run with --branch <target branch> to pick one\n',
            )

    def test_conflict_found(self):
        with (
            OutputCapture(),
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo,
            mocks.local.Svn(),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response),
        ):
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
                head=dict(ref='integration/conflict/sha123_sha123/target_branch', repo=dict(full_name='tcontributor')),
                base=dict(ref='main'),
                requested_reviews=[dict(login='rreviewer')],
                reviews=[dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')],
                draft=False,
            ), self._pr(2, head='integration/conflict/sha123_sha123_safari-7626-branch', base='safari-7626-branch')]
            repo.edit_config('remote.tcontributor.url', 'https://github.com/tcontributor/WebKit')
            repo.commits['remotes/tcontributor/integration/conflict/sha123_sha123/target_branch'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier="3.1@integration/conflict/sha123_sha123/target_branch",
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]
            self.assertEqual('d8bce26fa65c6fc8f39c17927abb77f69fab82fc', local.Git(self.path).commit().hash)

            self.assertEqual('integration/conflict/sha123_sha123/target_branch', program.main(
                args=('conflict', '1234', '--branch', 'main'),
                path=self.path,
            ).branch)

            self.assertEqual('a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', local.Git(self.path).commit().hash)
