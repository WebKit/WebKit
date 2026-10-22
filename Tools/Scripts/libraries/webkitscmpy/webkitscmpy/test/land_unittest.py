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
from unittest.mock import PropertyMock, patch

from webkitbugspy import Tracker, User, bugzilla, radar
from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Environment
from webkitcorepy.mocks import Terminal as MockTerminal
from webkitcorepy.mocks import Time as MockTime

from webkitscmpy import Commit, CommitClassifier, Contributor, local, mocks, program
from webkitscmpy.program.canonicalize import IdentifierTrailer


def repository(path, has_oops=True, remote=None, remotes=None, git_svn=False, issue_url=None):
    branch = 'eng/example'
    result = mocks.local.Git(path, remote=remote, remotes=remotes, git_svn=git_svn)
    result.commits[branch] = [
        result.commits[result.default_branch][2],
        Commit(
            hash='a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
            identifier='3.1@{}'.format(branch),
            revision=10,
            timestamp=int(time.time()) - 60,
            author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
            message='To Be Committed\n{}\nReviewed by {}.\n{}'.format(
                '{}\n'.format(issue_url) if issue_url else '',
                {True: 'NOBODY (OOPS!)', False: 'Ricky Reviewer'}.get(has_oops, has_oops),
                '\ngit-svn-id: https://svn.{}/repository/{}/trunk@10 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n'.format(
                    result.remote.split('@')[-1].split(':')[0],
                    os.path.basename(result.path),
                ) if git_svn else '',
            ),
        )
    ]
    result.head = result.commits[branch][-1]
    return result


def add_commit(result, issue_url):
    branch = 'eng/example'
    result.commits[branch].append(Commit(
        hash='6e3c9fdc67f1cf79f2a58e8e9f50cb3a9ab2c8d4',
        identifier='3.2@{}'.format(branch),
        revision=11,
        timestamp=int(time.time()) - 30,
        author=Contributor('Tim Committer', ['tcommitter@webkit.org']),
        message='Also To Be Committed\n{}\nReviewed by Ricky Reviewer.\n'.format(issue_url),
    ))
    result.head = result.commits[branch][-1]


class TestLand(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'
    COMMENT = 'Committed a5fe8afe9bf7 to git@example.org:mock/repository (origin/main).'

    def setUp(self):
        super(TestLand, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))
        patcher = patch.object(Tracker, '_trackers', [])
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_none(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_non_editable(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '5@main')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual([line for line in log if 'Mock process' not in line], [])
        self.assertEqual(
            captured.stderr.getvalue(),
            "Can only 'land' editable branches\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_with_oops(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...'
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n"
            "Found '(OOPS!)' message in commit messages, please resolve before committing\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_with_removed_oops(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops='- NOBODY (OOPS!)') as repo, mocks.local.Svn(), MockTerminal.input('n'):
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                ' Local branch is tracking the remote branch.',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Landed a5fe8afe9bf7!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_default(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops=False), mocks.local.Svn(), MockTerminal.input('n'):
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                ' Local branch is tracking the remote branch.',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Landed a5fe8afe9bf7!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_canonicalize(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops=False), mocks.local.Svn(), MockTerminal.input('n'):
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
                identifier_template='Canonical link: https://commits.webkit.org/{}',
            ))

            commit = local.Git(self.path).commit(branch='main')
            self.assertEqual(str(commit), '6@main')
            self.assertEqual(
                commit.message,
                'To Be Committed\n\n'
                'Reviewed by Ricky Reviewer.\n\n'
                'Canonical link: https://commits.webkit.org/6@main',
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                ' Local branch is tracking the remote branch.',
                '1 commit to be edited...',
                'Base commit is 5@main (ref d8bce26fa65c6fc8f39c17927abb77f69fab82fc)',
                ' Local branch is tracking the remote branch.',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd (1/1) (--- seconds passed, remaining --- predicted)\n'
            'Overwriting a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd\n'
            '1 commit successfully canonicalized!\n'
            'Landed https://commits.webkit.org/6@main (a5fe8afe9bf7)!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_canonicalize_with_identifier_trailer(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops=False), mocks.local.Svn(), MockTerminal.input('n'):
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
                identifier_template=IdentifierTrailer(
                    name='Canonical link',
                    value_template='https://commits.webkit.org/{}',
                    aliases=('Canonical-link',),
                ),
            ))

            commit = local.Git(self.path).commit(branch='main')
            self.assertEqual(str(commit), '6@main')
            self.assertEqual(
                commit.message,
                'To Be Committed\n\n'
                'Reviewed by Ricky Reviewer.\n\n'
                'Canonical link: https://commits.webkit.org/6@main',
            )

        self.assertIn(
            'Landed https://commits.webkit.org/6@main (a5fe8afe9bf7)!\n',
            captured.stdout.getvalue(),
        )

    def test_no_svn_canonical_svn(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops=False), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path, canonical_svn=True,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        self.assertEqual(
            captured.stderr.getvalue(),
            "Cannot 'land' on a canonical SVN repository that is not configured as git-svn\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_svn(self):
        with MockTime, OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops=False, git_svn=True), mocks.local.Svn(), MockTerminal.input('n'):
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path, canonical_svn=True,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                '    Verifying mirror processesed change',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Landed a5fe8afe9bf7!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_default_with_radar(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')
            self.assertEqual(Tracker.instance().issue(1).comments[-1].content, self.COMMENT)
            self.assertFalse(Tracker.instance().issue(1).opened)
            self.assertEqual(Tracker.instance().issue(1).state, 'Verify')
            self.assertEqual(
                Tracker.instance().issue(1).source_changes,
                ['Repository, merge, a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd'],
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                ' Local branch is tracking the remote branch.',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Landed a5fe8afe9bf7!\n'
            'Moved rdar://1 to Verify\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_integrate_radar(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/1>') as repo, mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.edit_config('webkitscmpy.land-radar-state', 'Integrate')

            self.assertEqual(0, program.main(args=('land',), path=self.path))
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.state, 'Integrate')
            self.assertTrue(issue.opened)
            self.assertEqual(issue.comments[-1].content, self.COMMENT)
            self.assertEqual(issue.source_changes, ['Repository, merge, a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd'])

    def test_radar_state_argument(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/1>') as repo, mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.edit_config('webkitscmpy.land-radar-state', 'Integrate')

            self.assertEqual(0, program.main(args=('land', '--radar-state', 'Verify'), path=self.path))
            self.assertEqual(Tracker.instance().issue(1).state, 'Verify')

    def test_invalid_radar_state(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>') as repo, mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.edit_config('webkitscmpy.land-radar-state', 'Closed')

            self.assertEqual(0, program.main(args=('land',), path=self.path))
            self.assertEqual(Tracker.instance().issue(1).state, 'Verify')

        self.assertIn(
            "'Closed' is not a valid value for 'webkitscmpy.land-radar-state', using 'Verify' instead\n",
            captured.stderr.getvalue(),
        )

    def test_radar_does_not_move_backwards(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/3>') as repo, mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.edit_config('webkitscmpy.land-radar-state', 'Integrate')

            self.assertEqual(0, program.main(args=('land',), path=self.path))
            issue = Tracker.instance().issue(3)
            self.assertEqual(issue.state, 'Verify')
            self.assertEqual(issue.comments[-1].content, self.COMMENT)
            self.assertEqual(issue.source_changes, ['Repository, merge, a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd'])

    def test_radar_without_milestone(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=[dict(issue, milestone='') for issue in bmocks.ISSUES]), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(1, program.main(args=('land',), path=self.path))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')
            self.assertEqual(Tracker.instance().issue(1).source_changes, [])

        self.assertEqual(
            captured.stderr.getvalue(),
            "rdar://1 has no milestone, set one or pass '--no-update-radar' to land without moving it to Verify\n",
        )

    def test_unreadable_radar(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), \
                patch('webkitbugspy.Issue.state', new_callable=PropertyMock, return_value=None):

            self.assertEqual(1, program.main(args=('land',), path=self.path))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to read rdar://1, pass '--no-issue' to land without updating it\n",
        )

    def test_gardening_radar(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('No', 'n'), Environment(RADAR_USERNAME='tcontributor'), \
                bmocks.Radar(issues=[dict(issue, milestone='') for issue in bmocks.ISSUES]), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(
                args=('land',), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=['To Be Committed'])]),
            ))
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.state, 'Analyze')
            self.assertEqual(issue.comments[-1].content, self.COMMENT)
            self.assertEqual(issue.source_changes, ['Repository, merge, a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd'])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Move rdar://1 to Verify? It only has gardening changes (Yes/[No]): \n'
            'Landed a5fe8afe9bf7!\n'
            'Not moving rdar://1 to Verify for a gardening change\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_gardening_radar_finished(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('Yes', 'n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(
                args=('land',), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=['To Be Committed'])]),
            ))
            self.assertEqual(Tracker.instance().issue(1).state, 'Verify')

    def test_gardening_radar_failure(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('No', 'n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES) as radar_mock, \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), \
                patch('webkitbugspy.Issue.add_comment', side_effect=radar_mock.exceptions.UnsuccessfulResponseException('Radar refused the comment')):

            self.assertEqual(0, program.main(
                args=('land',), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=['To Be Committed'])]),
            ))

        self.assertTrue(captured.stderr.getvalue().endswith(" --issue rdar://1 --no-update-radar' to retry updating issues\n"))

    def test_no_update_radar(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), \
                bmocks.Radar(issues=[dict(issue, milestone='') for issue in bmocks.ISSUES]), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(args=('land', '--no-update-radar'), path=self.path))
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.state, 'Analyze')
            self.assertEqual(issue.comments[-1].content, self.COMMENT)
            self.assertEqual(issue.source_changes, ['Repository, merge, a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd'])

    def test_no_issue(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(args=('land', '--no-issue'), path=self.path))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')
            issue = Tracker.instance().issue(1)
            self.assertEqual(issue.state, 'Analyze')
            self.assertEqual(issue.comments[-1].content, 'Trying to prepare a fix for 1.2.4')
            self.assertEqual(issue.source_changes, [])

    def test_multiple_commits(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/1>') as repo, mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            add_commit(repo, issue_url='<rdar://problem/1>')

            self.assertEqual(0, program.main(args=('land', '--no-squash'), path=self.path))
            issue = Tracker.instance().issue(1)
            self.assertEqual(
                issue.comments[-1].content,
                'Committed 6e3c9fdc67f1 and a5fe8afe9bf7 to git@example.org:mock/repository (origin/main).',
            )
            self.assertEqual(
                issue.source_changes, [
                    'Repository, merge, 6e3c9fdc67f1cf79f2a58e8e9f50cb3a9ab2c8d4',
                    'Repository, merge, a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd',
                ],
            )

    def test_multiple_radars(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>') as repo, mocks.local.Svn(), \
                MockTerminal.input('No'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            add_commit(repo, issue_url='<rdar://problem/2>')

            self.assertEqual(1, program.main(args=('land',), path=self.path))
            self.assertEqual(str(local.Git(self.path).commit()), '3.2@eng/example')

        self.assertEqual(captured.stdout.getvalue(), 'Update rdar://2 and rdar://1 once landed? ([Yes]/No): \n')
        self.assertEqual(
            captured.stderr.getvalue(),
            "Pass '--no-issue' to land without updating issues\n",
        )

    def test_multiple_gardening_radars(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>') as repo, mocks.local.Svn(), \
                MockTerminal.input('No'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            add_commit(repo, issue_url='<rdar://problem/2>')

            self.assertEqual(1, program.main(
                args=('land', '--no-squash'), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=['To Be Committed'])]),
            ))

        self.assertEqual(captured.stdout.getvalue(), 'Update rdar://2 and rdar://1 once landed? ([Yes]/No): \n')

    def test_squashed_radars(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                MockTerminal.input('Yes', 'n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.checkout('eng/squash-branch')
            repo.commits['eng/squash-branch'][1].message = 'Changed something 1\n<rdar://problem/1>\n'
            repo.commits['eng/squash-branch'][2].message = 'Changed something 2\n<rdar://problem/2>\n'
            repo.commits['eng/squash-branch'][3].message = 'Changed something 3\n<rdar://problem/1>\n'

            self.assertEqual(0, program.main(args=('land', '--squash'), path=self.path))
            landed = local.Git(self.path).commit()
            for id in (1, 2):
                self.assertIn('Repository, merge, {}'.format(landed.hash), Tracker.instance().issue(id).source_changes)
                self.assertEqual(Tracker.instance().issue(id).state, 'Verify')

    def test_squashed_gardening(self):
        with OutputCapture(), mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                MockTerminal.input('No', 'n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            repo.checkout('eng/squash-branch')
            for commit in repo.commits['eng/squash-branch'][1:]:
                commit.message = 'Changed something\n<rdar://problem/1>\n'

            self.assertEqual(0, program.main(
                args=('land', '--squash'), path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(name='Gardening', headers=['^Changed something$'])]),
            ))
            landed = local.Git(self.path).commit()
            self.assertEqual(Tracker.instance().issue(1).state, 'Analyze')
            self.assertIn('Repository, merge, {}'.format(landed.hash), Tracker.instance().issue(1).source_changes)

    def test_first_radar_per_commit(self):
        with OutputCapture(), repository(self.path, has_oops=False, issue_url='<rdar://problem/2>\nReverted change: <rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES), \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(args=('land',), path=self.path))
            self.assertEqual(Tracker.instance().issue(2).state, 'Verify')
            self.assertEqual(Tracker.instance().issue(1).state, 'Analyze')
            self.assertEqual(Tracker.instance().issue(1).source_changes, [])

    def test_radar_update_failure(self):
        with OutputCapture() as captured, repository(self.path, has_oops=False, issue_url='<rdar://problem/1>'), mocks.local.Svn(), \
                MockTerminal.input('n'), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(issues=bmocks.ISSUES) as radar_mock, \
                patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), \
                patch('webkitbugspy.Issue.set_state', side_effect=radar_mock.exceptions.UnsuccessfulResponseException('Radar refused the state change')):

            self.assertEqual(0, program.main(args=('land',), path=self.path))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')
            self.assertEqual(Tracker.instance().issue(1).state, 'Analyze')

        self.assertTrue(captured.stderr.getvalue().startswith(
            "Failed to find pull-request associated with 'eng/example'\n"
            'Failed to update rdar://1:\n'
            'Radar refused the state change\n'
            "Run 'git-webkit mark-fixed a5fe8afe9bf7",
        ))
        self.assertTrue(captured.stderr.getvalue().endswith(" --issue rdar://1' to retry updating issues\n"))

    def test_canonicalize_with_bugzilla(self):
        with OutputCapture(level=logging.INFO) as captured, repository(self.path, has_oops=False, issue_url='{}/show_bug.cgi?id=1'.format(self.BUGZILLA)), \
                mocks.local.Svn(), MockTerminal.input('n'), bmocks.Bugzilla(
                    self.BUGZILLA.split('://')[-1],
                    issues=bmocks.ISSUES,
                    environment=Environment(
                        BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                        BUGS_EXAMPLE_COM_PASSWORD='password',
                )), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):

            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
                identifier_template='Canonical link: https://commits.webkit.org/{}',
            ))
            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Committed https://commits.webkit.org/6@main (a5fe8afe9bf7) to git@example.org:mock/repository (origin/main).',
            )
            self.assertFalse(Tracker.instance().issue(1).opened)

            commit = local.Git(self.path).commit(branch='main')
            self.assertEqual(str(commit), '6@main')
            self.assertEqual(
                commit.message,
                'To Be Committed\n'
                'https://bugs.example.com/show_bug.cgi?id=1\n\n'
                'Reviewed by Ricky Reviewer.\n\n'
                'Canonical link: https://commits.webkit.org/6@main',
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                ' Local branch is tracking the remote branch.',
                '1 commit to be edited...',
                'Base commit is 5@main (ref d8bce26fa65c6fc8f39c17927abb77f69fab82fc)',
                ' Local branch is tracking the remote branch.',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd (1/1) (--- seconds passed, remaining --- predicted)\n'
            'Overwriting a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd\n'
            '1 commit successfully canonicalized!\n'
            'Landed https://commits.webkit.org/6@main (a5fe8afe9bf7)!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_svn_with_bugzilla(self):
        with MockTime, OutputCapture(level=logging.INFO) as captured, \
                repository(self.path, has_oops=False, git_svn=True, issue_url='{}/show_bug.cgi?id=1'.format(self.BUGZILLA)), \
                mocks.local.Svn(), MockTerminal.input('n'), bmocks.Bugzilla(
                    self.BUGZILLA.split('://')[-1],
                    issues=bmocks.ISSUES,
                    environment=Environment(
                        BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                        BUGS_EXAMPLE_COM_PASSWORD='password',
                )), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):

            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path, canonical_svn=True,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '6@main')
            self.assertEqual(
                Tracker.instance().issue(1).comments[-1].content,
                'Committed r10 to git@example.org:mock/repository (origin/main).',
            )
            self.assertFalse(Tracker.instance().issue(1).opened)

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                '    Verifying mirror processesed change',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Failed to find pull-request associated with 'eng/example'\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            'Landed a5fe8afe9bf7!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )


class TestLandGitHub(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestLandGitHub, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))
        patcher = patch.object(Tracker, '_trackers', [])
        patcher.start()
        self.addCleanup(patcher.stop)

    @classmethod
    def webserver(cls, approved=None, labels=None, environment=None):
        result = mocks.remote.GitHub(labels=labels, environment=environment)
        result.users.create('Ricky Reviewer', 'rreviewer', ['rreviewer@webkit.org'])
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
            body='''#### a5fe8afe9bf7d07158fcd9e9732ff02a712db2fd
    <pre>
    To Be Committed

    Reviewed by NOBODY (OOPS!).
    </pre>
    ''',
            head=dict(ref='username:eng/example'),
            base=dict(ref='main'),
            requested_reviews=[dict(login='rreviewer')],
            reviews=[
                dict(user=dict(login='rreviewer'), state='APPROVED')
            ] if approved else [] + [
                dict(user=dict(login='rreviewer'), state='CHANGES_REQUESTED')
            ] if approved is not None else [], _links=dict(
                issue=dict(href='https://{}/issues/1'.format(result.api_remote)),
            ), draft=False,
        )]
        return result

    def test_no_reviewer(self):
        with OutputCapture(level=logging.INFO) as captured, self.webserver() as remote, \
                repository(self.path, remote='https://{}'.format(remote.remote)), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Found '(OOPS!)' message in commit messages, please resolve before committing\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_blocking_reviewer(self):
        with OutputCapture(level=logging.INFO) as captured, self.webserver(approved=False) as remote, \
                repository(self.path, has_oops=False, remote='https://{}'.format(remote.remote)), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Ricky Reviewer is blocking landing 'PR 1 | Example Change'\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_insert_review(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('y', 'n'), self.webserver(
                approved=True) as remote, \
                repository(self.path, has_oops=True, remote='https://{}'.format(remote.remote)), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
            ))

            repo = local.Git(self.path)
            self.assertEqual(str(repo.commit()), '6@main')
            self.assertEqual(
                [comment.content for comment in repo.remote().pull_requests.get(1).comments],
                ['Landed a5fe8afe9bf7!'],
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                'Setting Ricky Reviewer as reviewer',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                "Updating 'PR 1 | Example Change' to match landing commits...",
                ' Local branch is tracking the remote branch.',
            ],
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Set 'Ricky Reviewer' as your reviewer? ([Yes]/No): \n"
            'Landed a5fe8afe9bf7!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )

    def test_merge_queue(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('y', 'n'), self.webserver(
            approved=True, labels={'merge-queue': dict(color='3AE653', description="Send PR to merge-queue")},
            environment=Environment(
                GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
                GITHUB_EXAMPLE_COM_TOKEN='token',
            ),
        ) as remote, mocks.local.Svn(), repository(
            self.path, has_oops=True,
            remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ):
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
            ))

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                'Detected merging automation, using that instead of local git tooling',
                "Rebasing 'eng/example' on 'main'...",
                "Rebased 'eng/example' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                'Checking PR labels for active labels...',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/example' to 'fork'...",
                "Creating 'eng/example-1' as a reference branch",
                "Updating pull-request for 'eng/example'...",
                "Adding 'merge-queue' to 'PR 1 | To Be Committed'",
            ],
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 1 | To Be Committed'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n"
            "Added 'merge-queue' to 'PR 1 | To Be Committed', change is in the queue to be landed\n",
        )

    def test_merge_queue_linked_bug(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('y', 'n'), self.webserver(
            approved=True, labels={'merge-queue': dict(color='3AE653', description="Send PR to merge-queue")},
            environment=Environment(
                GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
                GITHUB_EXAMPLE_COM_TOKEN='token',
            ),
        ) as remote, mocks.local.Svn(), repository(
            self.path, has_oops=True,
            issue_url='{}/show_bug.cgi?id=1'.format(self.BUGZILLA),
            remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(len(Tracker.instance().issue(1).comments), 2)

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                'Detected merging automation, using that instead of local git tooling',
                "Rebasing 'eng/example' on 'main'...",
                "Rebased 'eng/example' on 'main!'",
                'Running pre-PR checks...',
                'No pre-PR checks to run',
                'Checking if PR already exists...',
                'PR #1 found.',
                'Checking PR labels for active labels...',
                "Updating 'main' on 'https://github.example.com/Contributor/WebKit'",
                "Pushing 'eng/example' to 'fork'...",
                "Creating 'eng/example-1' as a reference branch",
                "Updating pull-request for 'eng/example'...",
                'Syncing PR labels with issue component...',
                'No label syncing required',
                "Adding 'merge-queue' to 'PR 1 | To Be Committed'",
            ],
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 1 | To Be Committed'!\n"
            "https://github.example.com/WebKit/WebKit/pull/1\n"
            "Added 'merge-queue' to 'PR 1 | To Be Committed', change is in the queue to be landed\n",
        )


class TestLandBitBucket(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestLandBitBucket, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))
        patcher = patch.object(Tracker, '_trackers', [])
        patcher.start()
        self.addCleanup(patcher.stop)

    @classmethod
    def webserver(cls, approved=None):
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
                    ), approved=True if approved else False,
                    status='NEEDS_WORK' if approved is False else None,
                ),
            ],
        )]
        return result

    def test_no_reviewer(self):
        with OutputCapture(level=logging.INFO) as captured, self.webserver() as remote, repository(
                self.path, remote='ssh://git@{}/{}/{}.git'.format(
                    remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
                )), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Found '(OOPS!)' message in commit messages, please resolve before committing\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_blocking_reviewer(self):
        with OutputCapture(level=logging.INFO) as captured, self.webserver(approved=False) as remote, repository(
                self.path, has_oops=False, remote='ssh://git@{}/{}/{}.git'.format(
                    remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
                )), mocks.local.Svn():

            self.assertEqual(1, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            self.assertEqual(str(local.Git(self.path).commit()), '3.1@eng/example')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
            ],
        )
        self.assertEqual(
            captured.stderr.getvalue(),
            "Ricky Reviewer is blocking landing 'PR 1 | Example Change'\n",
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_insert_review(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('y', 'n'), self.webserver(approved=True) as remote, repository(
                self.path, has_oops=True, remote='ssh://git@{}/{}/{}.git'.format(
                    remote.hosts[0], remote.project.split('/')[1], remote.project.split('/')[3],
                )), mocks.local.Svn():

            self.assertEqual(0, program.main(
                args=('land', '-v'),
                path=self.path,
            ))
            repo = local.Git(self.path)
            self.assertEqual(str(repo.commit()), '6@main')
            self.assertEqual(
                [comment.content for comment in repo.remote().pull_requests.get(1).comments],
                ['Landed a5fe8afe9bf7!'],
            )

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                'Using committed changes...',
                'Setting Ricky Reviewer as reviewer',
                "Rebasing 'eng/example' from 'main' to 'main'...",
                "Rebased 'eng/example' from 'main' to 'main'!",
                "Updating 'PR 1 | Example Change' to match landing commits...",
                ' Local branch is tracking the remote branch.',
            ],
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Set 'Ricky Reviewer' as your reviewer? ([Yes]/No): \n"
            'Landed a5fe8afe9bf7!\n'
            "Delete branch 'eng/example'? ([Yes]/No): \n",
        )
