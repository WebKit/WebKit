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

from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime

from webkitscmpy import Commit, Contributor, local, mocks, program


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

    def _mock_radar_response_malformed(self, *args, **kwargs):
        # A radar's sourceChanges field is free-form text. When a merge has
        # multiple source changes they are separated by a blank line, so
        # splitlines() yields an empty string between entries (rdar://181637205).
        # That empty line must be skipped rather than crash the 'repo, action,
        # sha' parse. Uses two distinct shas so the branch is built from the
        # first and last change.
        rdar = bmocks.Radar()
        rdar.sourceChanges = '\n'.join([
            'WebKit, merge, sha123',
            '',
            'WebKit, merge, sha456',
        ])
        rdar.source_changes = rdar.sourceChanges.splitlines()
        rdar.id = 1234
        return rdar

    def _mock_radar_response_reversed(self, *args, **kwargs):
        # Source changes are not guaranteed to be ordered the same way the branch
        # was named: here the branch is sha123_sha456 but the radar lists the
        # changes in the opposite order, so shas[0]_shas[-1] alone (sha456_sha123)
        # would not match. Every ordered pair must be tried.
        rdar = bmocks.Radar()
        rdar.sourceChanges = '\n'.join([
            'WebKit, merge, sha456',
            'WebKit, merge, sha123',
        ])
        rdar.source_changes = rdar.sourceChanges.splitlines()
        rdar.id = 1234
        return rdar

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
        # Single source change: the radar lists one 'WebKit, merge, sha123'
        # entry, so shas[0] == shas[-1] and the branch is the common
        # <sha>_<sha> shape produced when a conflict PR is created per source
        # change.
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

    def test_conflict_malformed_source_changes(self):
        # Regression test: when a radar has multiple source changes they are
        # separated by a blank line, so splitlines() yields an empty string
        # that must not crash the 'repo, action, sha' parse (rdar://181637205).
        # The branch is reconstructed from the first and last change's sha.
        with (
            OutputCapture(),
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo,
            mocks.local.Svn(),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response_malformed),
            patch('webkitscmpy.program.conflict.Conflict.get_open_integration_prs', lambda x: [Mock(head='integration/conflict/sha123_sha456/target_branch', _metadata={'full_name': 'tcontributor'})])
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
                head=dict(ref='tcontributor:integration/conflict/sha123_sha456/target_branch'),
                base=dict(ref='main'),
                requested_reviews=[dict(login='rreviewer')],
                reviews=[dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')],
                draft=False,
            )]
            repo.edit_config('remote.tcontributor.url', 'https://github.com/tcontributor/WebKit')
            repo.commits['remotes/tcontributor/integration/conflict/sha123_sha456/target_branch'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier="3.1@integration/conflict/sha123_sha456/target_branch",
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]

            self.assertEqual('integration/conflict/sha123_sha456/target_branch', program.main(
                args=('conflict', '1234'),
                path=self.path,
            ).branch)

    def test_conflict_single_endpoint_branch(self):
        # Regression test: a radar may accumulate multiple source changes across
        # independent merges, in which case the conflict branch is named after a
        # single change repeated (<sha>_<sha>) rather than the first..last range
        # (rdar://181637205, whose branch is d01d9e5b_d01d9e5b even though it has
        # two source changes). find_conflict_pr must still locate the PR.
        with (
            OutputCapture(),
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo,
            mocks.local.Svn(),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response_malformed),
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

            self.assertEqual('integration/conflict/sha123_sha123/target_branch', program.main(
                args=('conflict', '1234'),
                path=self.path,
            ).branch)

    def test_conflict_source_changes_out_of_order(self):
        # Regression test: the branch (sha123_sha456) is named in the opposite
        # order from how the radar lists its source changes (sha456, sha123), so
        # only shas[0]_shas[-1] would miss it. Every ordered pair must be tried.
        with (
            OutputCapture(),
            mocks.remote.GitHub() as remote,
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo,
            mocks.local.Svn(),
            patch('webkitscmpy.program.conflict.Tracker.from_string', TestConflict._mock_radar_response_reversed),
            patch('webkitscmpy.program.conflict.Conflict.get_open_integration_prs', lambda x: [Mock(head='integration/conflict/sha123_sha456/target_branch', _metadata={'full_name': 'tcontributor'})])
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
                head=dict(ref='tcontributor:integration/conflict/sha123_sha456/target_branch'),
                base=dict(ref='main'),
                requested_reviews=[dict(login='rreviewer')],
                reviews=[dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')],
                draft=False,
            )]
            repo.edit_config('remote.tcontributor.url', 'https://github.com/tcontributor/WebKit')
            repo.commits['remotes/tcontributor/integration/conflict/sha123_sha456/target_branch'] = [
                repo.commits[repo.default_branch][2],
                Commit(
                    hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                    identifier="3.1@integration/conflict/sha123_sha456/target_branch",
                    timestamp=int(time.time()) - 60,
                    author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                    message='To Be Committed\n\nReviewed by NOBODY (OOPS!).\n',
                )
            ]

            self.assertEqual('integration/conflict/sha123_sha456/target_branch', program.main(
                args=('conflict', '1234'),
                path=self.path,
            ).branch)
