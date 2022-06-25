# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks


class TestSetupGitSvn(testing.PathTestCase):
    git_remote = 'git@example.org:Example'
    svn_remote = 'https://svn.example.org/repository/example'
    basepath = 'mock/repository'

    def setUp(self):
        super(TestSetupGitSvn, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path, remote=self.svn_remote), MockTime:
            self.assertEqual(1, program.main(
                args=('setup-git-svn',),
                path=self.path,
                subversion=self.svn_remote,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'Cannot setup git-svn on Subversion repository\n')

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('setup-git-svn',),
                path=self.path,
                subversion=self.svn_remote,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_empty(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, remote=self.git_remote), mocks.local.Svn(), mocks.remote.Svn(remote=self.svn_remote.split('://')[1]), MockTime:
            self.assertEqual(0, program.main(
                args=('setup-git-svn',),
                path=self.path,
                subversion=self.svn_remote,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            'Adding svn-remote to git config\n' +
            'Populating svn commit mapping (will take a few minutes)...\n',
        )

        with open(os.path.join(self.path, '.git/config')) as config:
            self.assertEqual(
                config.read(),
                '[core]\n'
                '\trepositoryformatversion = 0\n'
                '\tfilemode = true\n'
                '\tbare = false\n'
                '\tlogallrefupdates = true\n'
                '\tignorecase = true\n'
                '\tprecomposeunicode = true\n'
                '[pull]\n'
	            '\trebase = true\n'
                '[remote "origin"]\n'
                '\turl = {git_remote}\n'
                '\tfetch = +refs/heads/*:refs/remotes/origin/*\n'
                '[branch "main"]\n'
                '\tremote = origin\n'
                '\tmerge = refs/heads/main\n'
                '[svn-remote "svn"]\n'
                '\turl = {svn_remote}\n'
                '\tfetch = trunk:refs/remotes/origin/main\n'.format(
                    git_remote=self.git_remote,
                    svn_remote=self.svn_remote,
                ),
            )

    def test_add(self):
        with OutputCapture(), mocks.local.Git(self.path, remote=self.git_remote), mocks.local.Svn(), mocks.remote.Svn(remote=self.svn_remote.split('://')[1]), MockTime:
            self.assertEqual(0, program.main(
                args=('setup-git-svn',),
                path=self.path,
                subversion=self.svn_remote,
            ))

        with OutputCapture() as captured, mocks.local.Git(self.path, remote=self.git_remote), mocks.local.Svn(), mocks.remote.Svn(remote=self.svn_remote.split('://')[1]), MockTime:
            self.assertEqual(0, program.main(
                args=('setup-git-svn', '--all-branches'),
                path=self.path,
                subversion=self.svn_remote,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            'Adding svn-remote to git config\n' +
            'Populating svn commit mapping (will take a few minutes)...\n',
        )

        with open(os.path.join(self.path, '.git/config')) as config:
            self.assertEqual(
                config.read(),
                '[core]\n'
                '\trepositoryformatversion = 0\n'
                '\tfilemode = true\n'
                '\tbare = false\n'
                '\tlogallrefupdates = true\n'
                '\tignorecase = true\n'
                '\tprecomposeunicode = true\n'
                '[pull]\n'
	            '\trebase = true\n'
                '[remote "origin"]\n'
                '\turl = {git_remote}\n'
                '\tfetch = +refs/heads/*:refs/remotes/origin/*\n'
                '[branch "main"]\n'
                '\tremote = origin\n'
                '\tmerge = refs/heads/main\n'
                '[svn-remote "svn"]\n'
                '\turl = {svn_remote}\n'
                '\tfetch = trunk:refs/remotes/origin/main\n'
                '\tfetch = branches/branch-a:refs/remotes/origin/branch-a\n'
                '\tfetch = branches/branch-b:refs/remotes/origin/branch-b\n'.format(
                    git_remote=self.git_remote,
                    svn_remote=self.svn_remote,
                ),
            )
