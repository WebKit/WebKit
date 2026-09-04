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


class KeyValuePairNotFoundException(Exception):
    pass


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
    def _mock_radar_response(pr_url=None, comments=None):
        def callback(*args, **kwargs):
            rdar = bmocks.Radar()
            rdar.sourceChanges = 'WebKit, merge, sha123'
            rdar.source_changes = rdar.sourceChanges.splitlines()
            rdar.id = 1234
            rdar.comments = comments or []

            library = Mock()
            library.exceptions.KeyValuePairNotFoundException = KeyValuePairNotFoundException

            client = Mock()
            if pr_url:
                client.key_values_for_radar_id.return_value.key_value_pair_for_key.return_value = Mock(value=pr_url)
            else:
                client.key_values_for_radar_id.return_value.key_value_pair_for_key.side_effect = KeyValuePairNotFoundException()

            rdar.tracker = Mock(library=library, client=client)
            return rdar
        return callback

    def test_conflict_not_found(self):
        with (
            OutputCapture() as captured,
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response())
        ):
            self.assertEqual(1, program.main(
                args=('conflict', '1234'),
                path=self.path,
            ))

            self.assertEqual(captured.stderr.getvalue(), 'No conflict pull request found via the scm-integrate:pr_url key-value, radar comments, or branch-guessing on rdar://1234\n')

    def test_conflict_via_comments_validates_radar_reference(self):
        radar_obj = Mock(id=1234)
        with mocks.remote.GitHub() as remote:
            remote.pull_requests = [
                dict(
                    number=1, state='open', title='Unrelated change', user=dict(login='tcontributor'),
                    body='No radar reference here.',
                    head=dict(ref='eng/unrelated', repo=dict(full_name='tcontributor')),
                    base=dict(ref='main'), requested_reviews=[], reviews=[], draft=False,
                ),
                dict(
                    number=2, state='open', title='Cherry-pick(s)', user=dict(login='tcontributor'),
                    body='Cherry-pick(s). rdar://1234',
                    head=dict(ref='integration/conflict/sha_sha', repo=dict(full_name='tcontributor')),
                    base=dict(ref='main'), requested_reviews=[], reviews=[], draft=False,
                ),
            ]
            radar_obj.comments = [
                # Most recent comment links an unrelated PR - should be checked first and rejected.
                Issue.Comment(user=None, timestamp=200, content='See also https://{}/pull/1'.format(remote.remote)),
                # Older comment links the real conflict PR, which references this radar.
                Issue.Comment(user=None, timestamp=100, content='Conflict PR: https://{}/pull/2'.format(remote.remote)),
            ]

            pr = Conflict.find_conflict_pr_via_comments(radar_obj)
            self.assertIsNotNone(pr)
            self.assertEqual(pr.number, 2)

    def test_conflict_via_comments_no_matching_pr(self):
        radar_obj = Mock(id=1234)
        with mocks.remote.GitHub() as remote:
            remote.pull_requests = [dict(
                number=1, state='open', title='Unrelated change', user=dict(login='tcontributor'),
                body='No radar reference here.',
                head=dict(ref='eng/unrelated', repo=dict(full_name='tcontributor')),
                base=dict(ref='main'), requested_reviews=[], reviews=[], draft=False,
            )]
            radar_obj.comments = [
                Issue.Comment(user=None, timestamp=100, content='See also https://{}/pull/1'.format(remote.remote)),
            ]

            self.assertIsNone(Conflict.find_conflict_pr_via_comments(radar_obj))

    def test_conflict_found(self):
        with (
            OutputCapture(),
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo,
            mocks.local.Svn(),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response(pr_url='https://{}/pull/1'.format(remote.remote))),
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
            )]
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
                args=('conflict', '1234'),
                path=self.path,
            ).branch)

            self.assertEqual('a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', local.Git(self.path).commit().hash)
