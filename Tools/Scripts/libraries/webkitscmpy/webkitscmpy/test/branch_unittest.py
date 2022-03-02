# Copyright (C) 2021, 2022 Apple Inc. All rights reserved.
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

import logging
import os

from mock import patch
from webkitbugspy import bugzilla, mocks as bmocks, radar
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime, Terminal as MockTerminal, Environment
from webkitscmpy import local, program, mocks


class TestBranch(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestBranch, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_basic_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(1, program.main(
                args=('branch', '-i', '1234'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "Can only 'branch' on a native Git repository\n")

    def test_basic_git(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('branch', '-i', '1234', '-v'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/1234')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/1234'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Created the local development branch 'eng/1234'\n")

    def test_prompt_git(self):
        with MockTerminal.input('eng/example'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/example')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/example'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Enter name of new branch (or bug URL): \nCreated the local development branch 'eng/example'\n")

    def test_prompt_number(self):
        with MockTerminal.input('2'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/Example-feature-1')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/Example-feature-1'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Enter name of new branch (or bug URL): \nCreated the local development branch 'eng/Example-feature-1'\n")

    def test_prompt_url(self):
        with MockTerminal.input('<rdar://2>'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), \
            bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), mocks.local.Svn(), MockTime:

            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/Example-feature-1')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/Example-feature-1'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Enter name of new branch (or bug URL): \nCreated the local development branch 'eng/Example-feature-1'\n")

    def test_invalid_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('branch', '-i', 'reject_underscores'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "'eng/reject_underscores' is an invalid branch name, cannot create it\n")

    def test_to_branch_name(self):
        self.assertEqual(program.Branch.to_branch_name('something with spaces'), 'something-with-spaces')
        self.assertEqual(program.Branch.to_branch_name('[EWS] bug description'), 'EWS-bug-description')
        self.assertEqual(program.Branch.to_branch_name('[git-webkit] change'), 'git-webkit-change')
        self.assertEqual(program.Branch.to_branch_name('Add Terminal.open_url'), 'Add-Terminal-open_url')
