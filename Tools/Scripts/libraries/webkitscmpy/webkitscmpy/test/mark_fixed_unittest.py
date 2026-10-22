# Copyright (C) 2026 Apple Inc. All rights reserved.
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
import time
import unittest
from unittest.mock import patch

from webkitbugspy import Tracker, radar
from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Environment
from webkitcorepy.mocks import Terminal as MockTerminal

from webkitscmpy import Commit, CommitClassifier, Contributor, local, mocks, program
from webkitscmpy.program.mark_fixed import MarkFixed


def repository(path, message, remote=None):
    result = mocks.local.Git(path, remote=remote)
    result.commits[result.default_branch][-1].message = message
    return result


class TestIssueFor(unittest.TestCase):
    def issue_for(self, message):
        with patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            issue = MarkFixed.issue_for(Commit(hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd', message=message))
            return issue.id if issue else None

    def test_title(self):
        self.assertEqual(self.issue_for('rdar://1 (Fix a crash)\n\nReviewed by Ricky Reviewer.\n'), 1)

    def test_middle_of_line(self):
        self.assertEqual(self.issue_for('Fix a crash\nWork towards rdar://2 (Crash)\n\nReviewed by Ricky Reviewer.\n'), 2)

    def test_punctuation(self):
        self.assertEqual(self.issue_for('Unreviewed: Disable a test on\nFizz(rdar://3).\n'), 3)
        self.assertEqual(self.issue_for('Unreviewed: Add an expected failure for rdar://4.\n'), 4)
        self.assertEqual(self.issue_for('Fix a crash\n<rdar://problem/5>\n'), 5)

    def test_quoted_revert(self):
        self.assertIsNone(self.issue_for('Revert "rdar://1 (Fix a crash)"\n\nThis reverts commit d8bce26fa65c.\n'))
        self.assertEqual(self.issue_for('rdar://2 Revert "rdar://1 (Fix a crash)"\n'), 2)
        self.assertEqual(self.issue_for('Revert "rdar://1 (Fix a crash)"\n\nrdar://2 (Crash came back)\n'), 2)

    def test_quoted_reapply(self):
        self.assertEqual(self.issue_for('Reapply "rdar://1 (Fix a crash)"\n'), 1)

    def test_description(self):
        self.assertIsNone(self.issue_for('Fix a crash\n\nReviewed by Ricky Reviewer.\n\nSimilar to rdar://1.\n'))


class TestMarkFixed(testing.PathTestCase):
    basepath = 'mock/repository'
    MESSAGE = 'rdar://1 (Fix a crash)\n\nReviewed by Ricky Reviewer.\n'

    def setUp(self):
        super(TestMarkFixed, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_default(self):
        with OutputCapture() as captured, repository(self.path, self.MESSAGE), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            head = local.Git(self.path).commit()

            self.assertEqual(0, program.main(args=('mark-fixed',), path=self.path))
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.state, 'Verify')
            self.assertEqual(issue.source_changes, ['Repository, merge, {}'.format(head.hash)])
            self.assertEqual(
                issue.comments[-1].content,
                'Committed {} to git@example.org:mock/repository (origin/main).'.format(head.hash[:Commit.HASH_LABEL_SIZE]),
            )

        self.assertEqual(captured.stdout.getvalue(), 'Moved rdar://1 to Verify\n')
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_integrate(self):
        with OutputCapture(), repository(self.path, self.MESSAGE) as repo, mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.edit_config('webkitscmpy.land-radar-state', 'Integrate')

            self.assertEqual(0, program.main(args=('mark-fixed', 'HEAD'), path=self.path))
            self.assertEqual(Tracker.instance().issue(1).state, 'Integrate')

    def test_rerun(self):
        with OutputCapture() as captured, repository(self.path, self.MESSAGE), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            head = local.Git(self.path).commit()

            self.assertEqual(0, program.main(args=('mark-fixed',), path=self.path))
            self.assertEqual(0, program.main(args=('mark-fixed',), path=self.path))
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.source_changes, ['Repository, merge, {}'.format(head.hash)])
            self.assertEqual(len([comment for comment in issue.comments if comment.content.startswith('Committed')]), 1)

        self.assertEqual(
            captured.stderr.getvalue(),
            "'Repository, merge, {}' is already a registered source change\n".format(head.hash),
        )

    def test_without_milestone(self):
        with OutputCapture() as captured, repository(self.path, self.MESSAGE), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=[dict(issue, milestone='') for issue in bmocks.ISSUES]), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(1, program.main(args=('mark-fixed',), path=self.path))
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.state, 'Analyze')
            self.assertEqual(len(issue.source_changes), 1)

        self.assertEqual(captured.stderr.getvalue(), 'rdar://1 has no milestone, leaving it in Analyze\n')

    def test_no_issue(self):
        with OutputCapture() as captured, repository(self.path, 'Fix a crash\n\nReviewed by Ricky Reviewer.\n'), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(1, program.main(args=('mark-fixed',), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), 'No issues referenced by 5@main\n')

    def test_not_pushed(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.commits[repo.default_branch].append(Commit(
                hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                identifier='6@main',
                timestamp=int(time.time()),
                author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
                message=self.MESSAGE,
            ))
            repo.head = repo.commits[repo.default_branch][-1]

            self.assertEqual(1, program.main(args=('mark-fixed',), path=self.path))
            self.assertEqual(Tracker.instance().issue(1).source_changes, [])

        self.assertEqual(captured.stderr.getvalue(), "'6@main' has not been pushed to 'origin'\n")

    def test_bitbucket(self):
        with OutputCapture(), mocks.remote.BitBucket() as remote, mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), \
                repository(self.path, self.MESSAGE, remote='ssh://git@{}/{}/{}.git'.format(
                    remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
                )):
            head = local.Git(self.path).commit()

            self.assertEqual(0, program.main(args=('mark-fixed',), path=self.path))
            url = 'https://{}/{}'.format(remote.hosts[0], remote.project)
            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Committed {} to {} (origin/main).\n\n{}/commits/{}'.format(head.hash[:Commit.HASH_LABEL_SIZE], url, url, head.hash),
            )
            self.assertEqual(Tracker.instance().issue(1).source_changes, ['Webkit, merge, {}'.format(head.hash)])

    def test_issue_argument(self):
        with OutputCapture(), repository(self.path, 'Fix a crash\n\nReviewed by Ricky Reviewer.\n'), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            head = local.Git(self.path).commit()

            self.assertEqual(0, program.main(args=('mark-fixed', '--issue', 'rdar://2'), path=self.path))
            self.assertEqual(Tracker.instance().issue(2).source_changes, ['Repository, merge, {}'.format(head.hash)])
            self.assertEqual(Tracker.instance().issue(2).state, 'Verify')

    def test_invalid_issue_argument(self):
        with OutputCapture() as captured, repository(self.path, self.MESSAGE), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(1, program.main(args=('mark-fixed', '--issue', 'crash'), path=self.path))
            self.assertEqual(Tracker.instance().issue(1).source_changes, [])

        self.assertEqual(captured.stderr.getvalue(), "'crash' is not an issue this repository tracks\n")

    def test_range(self):
        with OutputCapture(), repository(self.path, self.MESSAGE) as repo, mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.commits[repo.default_branch][-2].message = 'Fix a hang\n<rdar://problem/2>\n'
            commits = repo.commits[repo.default_branch]

            self.assertEqual(0, program.main(args=('mark-fixed', 'HEAD~2..HEAD'), path=self.path))
            self.assertEqual(Tracker.instance().issue(1).source_changes, ['Repository, merge, {}'.format(commits[-1].hash)])
            self.assertEqual(Tracker.instance().issue(2).source_changes, ['Repository, merge, {}'.format(commits[-2].hash)])

    def test_empty_range(self):
        with OutputCapture() as captured, repository(self.path, self.MESSAGE), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(1, program.main(args=('mark-fixed', 'HEAD..HEAD', '--issue', 'rdar://1'), path=self.path))
            self.assertEqual(Tracker.instance().issue(1).source_changes, [])

        self.assertEqual(captured.stderr.getvalue(), 'No commits found in HEAD..HEAD\n')

    def test_gardening(self):
        with OutputCapture() as captured, repository(self.path, 'Add an expected failure for rdar://1.\n'), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            head = local.Git(self.path).commit()

            self.assertEqual(0, program.main(
                args=('mark-fixed', '--defaults'), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=[r'\bexpected failure'])]),
            ))
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.state, 'Analyze')
            self.assertEqual(issue.source_changes, ['Repository, merge, {}'.format(head.hash)])

        self.assertEqual(captured.stdout.getvalue(), 'Not moving rdar://1 to Verify for a gardening change\n')

    def test_gardening_radar(self):
        with OutputCapture() as captured, repository(self.path, 'Add an expected failure for rdar://1.\n'), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(
                args=('mark-fixed', '--defaults'), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=[r'\bexpected failure', '^Example issue'])]),
            ))
            self.assertEqual(Tracker.instance().issue(1).state, 'Verify')

        self.assertEqual(captured.stdout.getvalue(), 'Moved rdar://1 to Verify\n')

    def test_gardening_prompt(self):
        with OutputCapture() as captured, repository(self.path, 'Add an expected failure for rdar://1.\n'), mocks.local.Svn(), \
                MockTerminal.input('Yes'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(
                args=('mark-fixed',), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=[r'\bexpected failure'])]),
            ))
            self.assertEqual(Tracker.instance().issue(1).state, 'Verify')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Move rdar://1 to Verify? It only has gardening changes (Yes/[No]): \n'
            'Moved rdar://1 to Verify\n',
        )
