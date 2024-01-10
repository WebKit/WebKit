# Copyright (C) 2023 Apple Inc. All rights reserved.
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

from mock import patch
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Terminal as MockTerminal, Environment
from webkitbugspy import Tracker, radar, mocks as bmocks
from webkitscmpy import program, mocks


class TestClone(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestClone, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_no_radar(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), bmocks.NoRadar(), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            self.assertEqual(255, program.main(
                args=('clone', 'HEAD'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stderr.getvalue(),
            "'cloning' is a concept which belongs solely to radar\nRadar is not available on this machine\n",
        )

    def test_no_reason(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            milestones=bmocks.MILESTONES,
        ), OutputCapture() as captured, patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(255, program.main(
                args=('clone', 'rdar://1'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stderr.getvalue(),
            'No reason for cloning issue has been provided\n',
        )

    def test_basic(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            milestones=bmocks.MILESTONES,
        ), OutputCapture() as captured, patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(
                args=('clone', 'rdar://1', '--reason', 'Cloning for a future branch', '--milestone', 'Future'),
                path=self.path,
            ))
            tracker = radar.Tracker()
            raw_issue = tracker.client.radar_for_id(4)

            self.assertEqual(raw_issue.milestone.name, 'Future')
            self.assertIsNone(raw_issue.category)
            self.assertIsNone(raw_issue.event)
            self.assertIsNone(raw_issue.tentpole)

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'rdar://4 Example issue 1'\n"
            'Moved clone to Future and into Analyze: Prepare\n',
        )

    def test_no_prompt(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            milestones=bmocks.MILESTONES,
        ), MockTerminal.input('1'), OutputCapture() as captured, patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(255, program.main(
                args=('clone', 'rdar://1', '--reason', 'Cloning for an October branch', '--milestone', 'October', '--no-prompt'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stderr.getvalue(),
            "Category required for 'October', but not provided\n"
            'Too few attributes specified to clone rdar://1 Example issue 1\n',
        )
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_prompt(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            milestones=bmocks.MILESTONES,
        ), MockTerminal.input('3'), OutputCapture() as captured, patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            self.assertEqual(0, program.main(
                args=('clone', 'rdar://1', '--reason', 'Cloning for an October branch', '--milestone', 'October', '--prompt'),
                path=self.path,
            ))
            tracker = radar.Tracker()
            raw_issue = tracker.client.radar_for_id(4)

            self.assertEqual(raw_issue.milestone.name, 'October')
            self.assertEqual(raw_issue.category.name, 'Test Development')
            self.assertIsNone(raw_issue.event)
            self.assertIsNone(raw_issue.tentpole)

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            'Pick a category for your clone:\n'
            '    1) Escape / Regression in the Build\n'
            '    2) Tentpole Feature Work\n'
            '    3) Test Development\n: \n'
            "Created 'rdar://4 Example issue 1'\n"
            'Moved clone to October and into Analyze: Prepare\n',
        )

    def test_merge_back(self):
        issues = [issue.copy() for issue in bmocks.ISSUES]
        issues[0]['category'] = 'Tentpole Feature Work'

        with mocks.local.Git(self.path), mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(
            issues=issues,
            projects=bmocks.PROJECTS,
            milestones=bmocks.MILESTONES,
        ), MockTerminal.input('1'), OutputCapture() as captured, patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            self.assertEqual(0, program.main(
                args=('clone', 'rdar://1', '--reason', 'Cloning for an October branch', '--milestone', 'October', '--merge-back'),
                path=self.path,
            ))
            tracker = radar.Tracker()
            raw_issue = tracker.client.radar_for_id(4)

            self.assertEqual(raw_issue.milestone.name, 'October')
            self.assertEqual(raw_issue.category.name, 'Tentpole Feature Work')
            self.assertIsNone(raw_issue.event)
            self.assertIsNone(raw_issue.tentpole)

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "Created '[merge-back] rdar://4 Example issue 1'\n"
            'Moved clone to October and into Analyze: Prepare\n',
        )
