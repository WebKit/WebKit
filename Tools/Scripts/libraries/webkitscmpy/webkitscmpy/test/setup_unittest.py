# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
import sys

from webkitcorepy import Editor, OutputCapture, testing, mocks as wkmocks
from webkitcorepy.mocks import Terminal as MockTerminal
from webkitscmpy import local, program, mocks


class TestSetup(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestSetup, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn(self.path):
            self.assertEqual(1, program.main(
                args=('setup', '-v'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No setup required for {}\n'.format(self.path))

    def test_none(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('setup', '-v'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No setup required for ?\n')

    def test_github(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('y'), mocks.remote.GitHub() as remote:
            self.assertEqual(0, program.main(
                args=('-C', 'https://{}'.format(remote.remote), 'setup', '-v'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Create a private fork of 'WebKit/WebKit' named 'WebKit' belonging to 'username' ([Yes]/No): \n"
            'Setup succeeded!\n',
        )
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
        self.maxDiff = None
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path) as repo, \
            mocks.local.Svn(), wkmocks.Environment(EMAIL_ADDRESS='', SVN_LOG_EDITOR='xed -w'):

            self.assertEqual(0, program.main(
                args=('setup', '--defaults', '-v'),
                path=self.path,
            ))

            config = repo.config()
            self.assertEqual('xed -w', config.get('core.editor', ''))
            self.assertEqual('^[-+@a-zA-Z_].*$', config.get('diff.objcpp.xfuncname', ''))
            self.assertEqual('^[@a-zA-Z_].*$', config.get('diff.objcppheader.xfuncname', ''))
            self.assertEqual('auto', config.get('color.status', ''))
            self.assertEqual('auto', config.get('color.diff', ''))
            self.assertEqual('auto', config.get('color.branch', ''))
            self.assertEqual('true', config.get('pull.rebase', ''))

        self.assertEqual(
            captured.stdout.getvalue(),
            'For detailed information about the options configured by this script, please see:\n'
            'https://github.com/WebKit/WebKit/wiki/Git-Config#Configuration-Options\n\n\n'
            'Setup succeeded!\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            '''Setting git user email for {repository}...
Skipped setting email to 'tapple@webkit.org', it's already set for this repository
Setting git user name for {repository}...
Skipped setting name to 'Tim Apple', it's already set for this repository
No project git config found, continuing
Setting better Objective-C diffing behavior for this repository...
Set better Objective-C diffing behavior for this repository!
Using a rebase merge strategy for this repository
Setting git editor for {repository}...
Setting contents of 'SVN_LOG_EDITOR' as editor
Set git editor to 'SVN_LOG_EDITOR' for this repository
'''.format(repository=self.path),
        )

    def test_github_checkout(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub() as remote, \
            MockTerminal.input('n', 'n', 'committer@webkit.org', 'n', 'Committer', 's', 'overwrite', 'y', 'disabled', '1', 'y'), \
            mocks.local.Git(self.path, remote='https://{}.git'.format(remote.remote)) as repo, \
            wkmocks.Environment(EMAIL_ADDRESS='', SVN_LOG_EDITOR=''):

            self.assertEqual('https://github.example.com/WebKit/WebKit.git', local.Git(self.path).url())

            self.assertEqual(0, program.main(
                args=('setup', '-v', '-a'),
                path=self.path,
            ))

            config = repo.config()
            self.assertNotIn('color.status', config)
            self.assertEqual('Committer', config.get('user.name', ''))
            self.assertEqual('committer@webkit.org', config.get('user.email', ''))
            self.assertEqual('!f()', config.get('credential.https://github.example.com.helper', '').split()[0])
            self.assertEqual('https://github.example.com/WebKit/WebKit.git', local.Git(self.path).url())

        programs = ['default'] + [p.name for p in Editor.programs()]
        self.assertEqual(
            captured.stdout.getvalue(),
            '''For detailed information about the options configured by this script, please see:
https://github.com/WebKit/WebKit/wiki/Git-Config#Configuration-Options
Would you like to open this URL in your browser? ([Yes]/No): 


Set 'tapple@webkit.org' as the git user email for this repository ([Yes]/No): 
Enter git user email for this repository: 
Set 'Tim Apple' as the git user name for this repository ([Yes]/No): 
Enter git user name for this repository: 
Auto-color status, diff, and branch for this repository? ([Yes]/Skip): 
Would you like to automatically rebase your branch when creating or
updating a pull request? (Yes/[No]/Later): 
Would you like to create new branches to retain history when you overwrite
a pull request branch? ([when-user-owned]/disabled/always/never): 
Pick a commit message editor for this repository:
    {}
: 
Create a private fork of 'WebKit/WebKit' named 'WebKit' belonging to 'username' ([Yes]/No): 
Setup succeeded!
'''.format('\n    '.join([
            '{}) {}'.format(
                count + 1, programs[count] if count else '[{}]'.format(programs[count]),
            ) for count in range(len(programs))])))
        self.assertEqual(captured.stderr.getvalue(), '')

        self.assertEqual(
            captured.root.log.getvalue(),
            '''Setting git user email for {repository}...
Set git user email to 'committer@webkit.org' for this repository
Setting git user name for {repository}...
Set git user name to 'Committer' for this repository
No project git config found, continuing
Setting better Objective-C diffing behavior for this repository...
Set better Objective-C diffing behavior for this repository!
Using a rebase merge strategy for this repository
Setting auto update on PR creation...
Disabled auto update on PR creation
Setting git editor for {repository}...
Using the default git editor for this repository
Saving GitHub credentials in system credential store...
GitHub credentials saved via Keyring!
Verifying user owned fork...
Created a private fork of 'WebKit' belonging to 'username'!
Adding forked remote as 'fork'...
Added remote 'fork'
Fetching 'fork'
'''.format(repository=self.path),
        )

    def test_commit_message(self):
        with OutputCapture(level=logging.INFO), mocks.local.Git(self.path) as git, mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('setup', '--defaults', '-v'),
                path=self.path,
                hooks=os.path.join(os.path.abspath(os.path.dirname(__file__)), 'hooks')
            ))
            pcm = os.path.join(self.path, '.git', 'hooks', 'prepare-commit-msg')
            self.assertTrue(os.path.isfile(pcm))
            os.rename(pcm, os.path.join(os.path.dirname(pcm), 'prepare_commit_msg.py'))

            sys.path.insert(0, os.path.dirname(pcm))

            try:
                from prepare_commit_msg import main
                self.assertEqual(
                    main(os.path.join(self.path, 'COMMIT_MESSAGE')),
                    0,
                )
                with open(os.path.join(self.path, 'COMMIT_MESSAGE'), 'r') as file:
                    self.assertEqual(
                        file.read(),
                        '''Generated commit message
# Please populate the above commit message. Lines starting
# with '#' will be ignored

# 'On branch main
# Your branch is up to date with 'origin/main'.
# 
# nothing to commit, working tree clean
''')
            finally:
                sys.path.remove(os.path.dirname(pcm))
