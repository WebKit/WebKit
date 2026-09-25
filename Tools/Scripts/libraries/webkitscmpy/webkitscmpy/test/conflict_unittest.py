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
import re
import time
from unittest.mock import Mock, patch

from webkitbugspy import mocks as bmocks
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

    def _mock_radar_response(self, *args, **kwargs):
        rdar = bmocks.Radar()
        rdar.sourceChanges = 'WebKit, merge, sha123'
        rdar.source_changes = rdar.sourceChanges.splitlines()
        rdar.id = 1234
        return rdar

    RDAR_LINK_RE = re.compile(r'rdar://(\d+)$')

    @staticmethod
    def _mock_radar(id, related=None):
        rdar = bmocks.Radar()
        rdar.sourceChanges = 'WebKit, merge, sha123'
        rdar.source_changes = rdar.sourceChanges.splitlines()
        rdar.id = id
        rdar.related = related or {}
        return rdar

    @staticmethod
    def _mock_tracker_from_string(primary):
        # Stands in for `Tracker.from_string`: resolves `rdar://<id>` links the way the real
        # tracker would, returning `primary` for its own id and a minimal stub (only `.id` is
        # ever read by `radar_ids_from_text`) for any other radar referenced in PR titles.
        def _response(string, *args, **kwargs):
            match = TestConflict.RDAR_LINK_RE.match(string)
            if not match:
                return None
            id = int(match.group(1))
            return primary if id == primary.id else Mock(id=id)
        return _response

    @staticmethod
    def _mock_remote():
        remote = Mock()
        remote.name = 'WebKit'
        return remote

    def test_find_conflict_pr_ambiguous_resolved_by_own_radar_id(self):
        # Two candidate PRs share the same sha-prefix (as would happen when the same source
        # change conflicts on two target branches), but only one references the radar we asked
        # for in its title -- that one should be picked without prompting.
        prs = [
            Mock(head='integration/conflict/sha123_sha123_main', title='Cherry-pick sha123. rdar://184745333'),
            Mock(head='integration/conflict/sha123_sha123_su-branch', title='Cherry-pick sha123. rdar://184747345'),
        ]
        radar_obj = TestConflict._mock_radar(184747345)
        with (
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_tracker_from_string(radar_obj)),
            patch('webkitscmpy.program.conflict.Conflict.get_open_integration_prs', lambda remote: prs),
        ):
            self.assertIs(prs[1], Conflict.find_conflict_pr(TestConflict._mock_remote(), 184747345))

    def test_find_conflict_pr_ambiguous_resolved_by_clone_relationship(self):
        # rdar://184747345 is a clone of rdar://184745333; the correct conflict PR's title only
        # references the clone-parent's id, so disambiguation must walk the clone relationship.
        parent = Mock(id=184745333)
        prs = [
            Mock(head='integration/conflict/sha123_sha123_main', title='Cherry-pick sha123. rdar://999999'),
            Mock(head='integration/conflict/sha123_sha123_su-branch', title='Cherry-pick sha123. rdar://184745333'),
        ]
        radar_obj = TestConflict._mock_radar(184747345, related={'clone-of': [parent], 'cloned-to': []})
        with (
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_tracker_from_string(radar_obj)),
            patch('webkitscmpy.program.conflict.Conflict.get_open_integration_prs', lambda remote: prs),
        ):
            self.assertIs(prs[1], Conflict.find_conflict_pr(TestConflict._mock_remote(), 184747345))

    def test_find_conflict_pr_ambiguous_prompts_user(self):
        # Neither candidate's title references the radar (or a clone relative), so there's no way
        # to disambiguate automatically -- the user should be prompted to pick.
        prs = [
            Mock(head='integration/conflict/sha123_sha123_main', title='Cherry-pick sha123.'),
            Mock(head='integration/conflict/sha123_sha123_su-branch', title='Cherry-pick sha123.'),
        ]
        radar_obj = TestConflict._mock_radar(184747345)
        with (
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_tracker_from_string(radar_obj)),
            patch('webkitscmpy.program.conflict.Conflict.get_open_integration_prs', lambda remote: prs),
            patch('webkitscmpy.program.conflict.Terminal.choose') as choose,
        ):
            choose.return_value = '{} ({})'.format(prs[1].head, prs[1].title)
            self.assertIs(prs[1], Conflict.find_conflict_pr(TestConflict._mock_remote(), 184747345))
            self.assertTrue(choose.called)

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

            self.assertEqual(captured.stderr.getvalue(), 'No conflict pull request found with branch integration/conflict/1234\n')

    def test_conflict_found(self):
        with (
            OutputCapture(),
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo,
            mocks.local.Svn(),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response),
            patch('webkitscmpy.program.conflict.Conflict.get_open_integration_prs', lambda x: [Mock(head='integration/conflict/sha123_sha123/target_branch', _metadata={'full_name': 'tcontributor'})])
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
                head=dict(ref='tcontributor:integration/conflict/sha123_sha123/target_branch'),
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
