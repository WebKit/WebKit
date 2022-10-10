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
import time

from mock import patch
from webkitbugspy import bugzilla, mocks as bmocks, radar, Tracker
from webkitcorepy import OutputCapture, testing, mocks as wkmocks
from webkitcorepy.mocks import Time as MockTime, Terminal as MockTerminal, Environment
from webkitscmpy import local, program, mocks, log, Commit


class TestBranch(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestBranch, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_basic_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.assertEqual(1, program.main(
                args=('branch', '-i', '1234'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "Can only 'branch' on a native Git repository\n")

    def test_basic_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.assertEqual(1, program.main(
                args=('branch', '-i', '1234'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "Can only 'branch' on a native Git repository\n")

    def test_basic_git(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.assertEqual(0, program.main(
                args=('branch', '-i', '1234', '-v'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/1234')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/1234'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Created the local development branch 'eng/1234'\n")

    def test_prompt_git(self):
        with MockTerminal.input('eng/example'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), \
            mocks.local.Svn(), MockTime, patch('webkitbugspy.Tracker._trackers', []):

            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/example')

        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/example'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Enter name of new branch (or issue URL): \nCreated the local development branch 'eng/example'\n")

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
        self.assertEqual(captured.stdout.getvalue(), "Enter issue URL or title of new issue: \nCreated the local development branch 'eng/Example-feature-1'\n")

    def test_prompt_url(self):
        with MockTerminal.input('<rdar://2>'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), \
            bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), mocks.local.Svn(), MockTime:

            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/Example-feature-1')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/Example-feature-1'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Enter issue URL or title of new issue: \nCreated the local development branch 'eng/Example-feature-1'\n")

    def test_automatic_radar_cc(self):
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA), ''), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            users=bmocks.USERS,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [
            bugzilla.Tracker(self.BUGZILLA, radar_importer=bmocks.USERS['Radar WebKit Bug Importer']),
            radar.Tracker(),
        ]), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/Example-feature-1')

            issue = Tracker.from_string('{}/show_bug.cgi?id=2'.format(self.BUGZILLA))
            self.assertEqual(len(issue.references), 2)
            self.assertEqual(issue.references[0].link, 'rdar://4')
            self.assertEqual(issue.comments[-1].content, '<rdar://problem/4>')

        self.assertEqual(
            captured.root.log.getvalue(),
            "CCing Radar WebKit Bug Importer\n"
            "Creating the local development branch 'eng/Example-feature-1'...\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            "Enter issue URL or title of new issue: \n"
            "Existing radar to CC (leave empty to create new radar): \n"
            "Created the local development branch 'eng/Example-feature-1'\n",
        )

    def test_manual_radar_cc(self):
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA), '<rdar://4>'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            users=bmocks.USERS,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [
            bugzilla.Tracker(self.BUGZILLA, radar_importer=bmocks.USERS['Radar WebKit Bug Importer']),
            radar.Tracker(),
        ]), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/Example-feature-1')

            issue = Tracker.from_string('{}/show_bug.cgi?id=2'.format(self.BUGZILLA))
            self.assertEqual(len(issue.references), 2)
            self.assertEqual(issue.references[0].link, 'rdar://4')
            self.assertEqual(issue.comments[-1].content, '<rdar://problem/4>')

        self.assertEqual(
            captured.root.log.getvalue(),
            "CCing Radar WebKit Bug Importer\n"
            "Creating the local development branch 'eng/Example-feature-1'...\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            "Enter issue URL or title of new issue: \n"
            "Existing radar to CC (leave empty to create new radar): \n"
            "Created the local development branch 'eng/Example-feature-1'\n",
        )

    def test_manual_radar_cc_integer(self):
        with MockTerminal.input('{}/show_bug.cgi?id=2'.format(self.BUGZILLA), '4'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            users=bmocks.USERS,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [
            bugzilla.Tracker(self.BUGZILLA, radar_importer=bmocks.USERS['Radar WebKit Bug Importer']),
            radar.Tracker(),
        ]), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(args=('branch', '-v'), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/Example-feature-1')

            issue = Tracker.from_string('{}/show_bug.cgi?id=2'.format(self.BUGZILLA))
            self.assertEqual(len(issue.references), 2)
            self.assertEqual(issue.references[0].link, 'rdar://4')
            self.assertEqual(issue.comments[-1].content, '<rdar://problem/4>')

        self.assertEqual(
            captured.root.log.getvalue(),
            "CCing Radar WebKit Bug Importer\n"
            "Creating the local development branch 'eng/Example-feature-1'...\n",
        )
        self.assertEqual(
            captured.stdout.getvalue(),
            "Enter issue URL or title of new issue: \n"
            "Existing radar to CC (leave empty to create new radar): \n"
            "Created the local development branch 'eng/Example-feature-1'\n",
        )

    def test_redacted(self):
        class MockOptions(object):
            def __init__(self):
                self.issue = None

        with MockTerminal.input('<rdar://2>'), OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), \
            bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), mocks.local.Svn(), MockTime:

            log.setLevel(logging.INFO)
            self.assertEqual(0, program.Branch.main(
                MockOptions(), local.Git(self.path),
                redact=True,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/2')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/2'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Enter issue URL or title of new issue: \nCreated the local development branch 'eng/2'\n")


    def test_invalid_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime, patch('webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('branch', '-i', 'reject_underscores'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "'eng/reject_underscores' is an invalid branch name, cannot create it\n")

    def test_create_bug(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]), mocks.local.Svn(), MockTime, wkmocks.Terminal.input(
            '[Area] New Issue', 'Issue created via command line prompts.',
            '2', '1', '2',
        ):
            self.assertEqual(0, program.main(
                args=('branch', ),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/Area-New-Issue')

            issue = bugzilla.Tracker(self.BUGZILLA).issue(4)
            self.assertEqual(issue.title, '[Area] New Issue')
            self.assertEqual(issue.description, 'Issue created via command line prompts.')
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'SVG')
            self.assertEqual(issue.version, 'Safari 15')

        self.assertEqual(
            captured.stdout.getvalue(),
            '''Enter issue URL or title of new issue: 
Issue description: 
What project should the bug be associated with?:
    1) CFNetwork
    2) WebKit
: 
What component in 'WebKit' should the bug be associated with?:
    1) SVG
    2) Scrolling
    3) Tables
    4) Text
: 
What version of 'WebKit' should the bug be associated with?:
    1) Other
    2) Safari 15
    3) Safari Technology Preview
    4) WebKit Local Build
: 
Created 'https://bugs.example.com/show_bug.cgi?id=4 [Area] New Issue'
Created the local development branch 'eng/Area-New-Issue'
''',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_create_radar(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
        ), patch('webkitbugspy.Tracker._trackers', [radar.Tracker(project='WebKit')]), mocks.local.Svn(), MockTime, wkmocks.Terminal.input(
            '[Area] New Issue', 'Issue created via command line prompts.',
            '1', '2',
        ):
            self.assertEqual(0, program.main(
                args=('branch',),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/Area-New-Issue')

            issue = radar.Tracker(project='WebKit').issue(4)
            self.assertEqual(issue.title, '[Area] New Issue')
            self.assertEqual(issue.description, 'Issue created via command line prompts.')
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'SVG')
            self.assertEqual(issue.version, 'Safari 15')

        self.assertEqual(
            captured.stdout.getvalue(),
            '''Enter issue URL or title of new issue: 
Issue description: 
What component in 'WebKit' should the bug be associated with?:
    1) SVG
    2) Scrolling
    3) Tables
    4) Text
: 
What version of 'WebKit SVG' should the bug be associated with?:
    1) Other
    2) Safari 15
    3) Safari Technology Preview
    4) WebKit Local Build
: 
Created 'rdar://4 [Area] New Issue'
Created the local development branch 'eng/Area-New-Issue'
''',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_to_branch_name(self):
        self.assertEqual(program.Branch.to_branch_name('something with spaces'), 'something-with-spaces')
        self.assertEqual(program.Branch.to_branch_name('[EWS] bug description'), 'EWS-bug-description')
        self.assertEqual(program.Branch.to_branch_name('[git-webkit] change'), 'git-webkit-change')
        self.assertEqual(program.Branch.to_branch_name('Add Terminal.open_url'), 'Add-Terminal-open_url')

    def test_existing_branch_failure(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []), MockTime:
            repo.commits['eng/1234'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/1234',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/1234",
                    timestamp=int(time.time()),
                    message='[Testing] Existing commit\n'
                )
            ]
            self.assertEqual(1, program.main(
                args=('branch', '-i', '1234', '-v', '--no-delete-existing'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'main')

        self.assertEqual(captured.root.log.getvalue(), "")
        self.assertEqual(captured.stdout.getvalue(), "")
        self.assertEqual(captured.stderr.getvalue(), "'eng/1234' already exists in this checkout\n")

    def test_existing_branch_rebase(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(
                self.path) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []), MockTime:
            repo.commits['eng/1234'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/1234',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/1234",
                    timestamp=int(time.time()),
                    message='[Testing] Existing commit\n'
                )
            ]
            self.assertEqual(0, program.main(
                args=('branch', '-i', '1234', '-v'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/1234')

        self.assertEqual(captured.root.log.getvalue(), "Rebasing existing branch 'eng/1234' instead of creating a new one\n")
        self.assertEqual(captured.stdout.getvalue(), "Rebased the local development branch 'eng/1234'\n")
        self.assertEqual(captured.stderr.getvalue(), "")

    def test_existing_branch_delete(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(
                self.path) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []), MockTime:
            repo.commits['eng/1234'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                    branch='eng/1234',
                    author=dict(name='Tim Contributor', emails=['tcontributor@example.com']),
                    identifier="5.1@eng/1234",
                    timestamp=int(time.time()),
                    message='[Testing] Existing commit\n'
                )
            ]
            self.assertEqual(0, program.main(
                args=('branch', '-i', '1234', '-v', '--delete-existing'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/1234')

        self.assertEqual(
            captured.root.log.getvalue(),
            "Locally deleting existing branch 'eng/1234'\n"
            "Creating the local development branch 'eng/1234'...\n",
        )
        self.assertEqual(captured.stdout.getvalue(), "Created the local development branch 'eng/1234'\n")
        self.assertEqual(captured.stderr.getvalue(), "")
