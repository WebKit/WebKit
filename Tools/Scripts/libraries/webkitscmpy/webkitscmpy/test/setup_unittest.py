# Copyright (C) 2021 Apple Inc. All rights reserved.
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

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime, Terminal as MockTerminal
from webkitscmpy import program, mocks


class TestSetup(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestSetup, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path):
            self.assertEqual(1, program.main(
                args=('setup',),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No setup required for {}\n'.format(self.path))

    def test_github(self):
        with OutputCapture() as captured, MockTerminal.input('y'), mocks.remote.GitHub() as remote:
            self.assertEqual(0, program.main(
                args=('-C', 'https://{}'.format(remote.remote), 'setup'),
                path=self.path,
            ))

        self.assertEqual(captured.stdout.getvalue(), "Create a private fork of 'WebKit' belonging to 'username' (Yes/No): \n")
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''Saving GitHub credentials in system credential store...
GitHub credentials saved via Keyring!
Verifying user owned fork...
Created a private fork of 'WebKit' belonging to 'username'!
''',
        )

    def test_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('setup', '--defaults'),
                path=self.path,
            ))

            config = repo.config()
            self.assertEqual('^[-+@a-zA-Z_].*$', config.get('diff.objcpp.xfuncname', ''))
            self.assertEqual('^[@a-zA-Z_].*$', config.get('diff.objcppheader.xfuncname', ''))
            self.assertEqual('auto', config.get('color.status', ''))
            self.assertEqual('auto', config.get('color.diff', ''))
            self.assertEqual('auto', config.get('color.branch', ''))
            self.assertEqual('true', config.get('pull.rebase', ''))

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''Setting git user email for {repository}...
Set git user email to 'tapple@webkit.org'
Setting git user name for {repository}...
Set git user name to 'Tim Apple'
Setting better Objective-C diffing behavior...
Set better Objective-C diffing behavior!
Using a rebase merge strategy
'''.format(repository=self.path),
        )

    def test_github_checkout(self):
        with OutputCapture() as captured, mocks.remote.GitHub() as remote, \
            MockTerminal.input('n', 'committer@webkit.org', 'n', 'Committer', 'n', 'y'), \
            mocks.local.Git(self.path, remote='https://{}'.format(remote.remote)) as repo:

            self.assertEqual(0, program.main(
                args=('setup',),
                path=self.path,
            ))

            config = repo.config()
            self.assertNotIn('color.status', config)
            self.assertEqual('Committer', config.get('user.name', ''))
            self.assertEqual('committer@webkit.org', config.get('user.email', ''))

        self.assertEqual(
            captured.stdout.getvalue(),
            '''Set 'tapple@webkit.org' as the git user email (Yes/No): 
Git user email: 
Set 'Tim Apple' as the git user name (Yes/No): 
Git user name: 
Auto-color status, diff, and branch? (Yes/No): 
Create a private fork of 'WebKit' belonging to 'username' (Yes/No): 
''')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.maxDiff = None
        self.assertEqual(
            captured.root.log.getvalue(),
            '''Setting git user email for {repository}...
Set git user email to 'committer@webkit.org'
Setting git user name for {repository}...
Set git user name to 'Committer'
Setting better Objective-C diffing behavior...
Set better Objective-C diffing behavior!
Using a rebase merge strategy
Saving GitHub credentials in system credential store...
GitHub credentials saved via Keyring!
Verifying user owned fork...
Created a private fork of 'WebKit' belonging to 'username'!
Adding forked remote as 'username' and 'fork'...
Added remote 'username'
Added remote 'fork'
Fetching 'https://github.example.com/username/WebKit.git'
'''.format(repository=self.path),
        )
