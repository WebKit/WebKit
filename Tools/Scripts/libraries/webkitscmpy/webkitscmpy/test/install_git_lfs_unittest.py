# Copyright (C) 2022 Apple Inc. All rights reserved.
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
from webkitcorepy import OutputCapture, testing, mocks as wkmocks
from webkitscmpy import local, program, mocks


class TestInstallGitLFS(testing.PathTestCase):
    basepath = 'mock/repository'
    ZIP_CONTENTS = b"PK\x03\x04\x14\x00\x00\x00\x00\x00!\x7f\x17W\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0e\x00 " \
                   b"\x00git-lfs-3.4.0/UT\r\x00\x07\xbe\x8e\xe6d\xbe\x8e\xe6d\xbe\x8e\xe6dux\x0b\x00\x01\x04\xf5\x01\x00\x00" \
                   b"\x04\x14\x00\x00\x00PK\x03\x04\x14\x00\x08\x00\x08\x00QIxT\x00\x00\x00\x00\x00\x00\x00\x00\x1b\x00\x00" \
                   b"\x00\x18\x00 \x00git-lfs-3.4.0/install.shUT\r\x00\x07\xfb\x97<b\xc4\x8e\xe6d\xc4\x8e\xe6dux\x0b\x00\x01" \
                   b"\x04\xf5\x01\x00\x00\x04\x14\x00\x00\x00SV\xd4/-.\xd2O\xca\xcc\xd3O\xcd+S(\xce\xe0\xe2J\xad\xc8,Q0\xe0" \
                   b"\xe2\x02\x00PK\x07\x08\xc2\x15&\xc4\x1d\x00\x00\x00\x1b\x00\x00\x00PK\x03\x04\x14\x00\x08\x00\x08\x00QIxT" \
                   b"\x00\x00\x00\x00\x00\x00\x00\x00\xb0\x00\x00\x00#\x00 \x00__MACOSX/git-lfs-3.4.0/._install.shUT\r\x00\x07" \
                   b"\xfb\x97<b\xc4\x8e\xe6d\xcc\x8e\xe6dux\x0b\x00\x01\x04\xf5\x01\x00\x00\x04\x14\x00\x00\x00c`\x15cg`b`\xf0MLV" \
                   b"\xf0\x0fV\x88P\x80\x02\x90\x18\x03'\x10\x1b\x01q\x1d\x10\x83\xf8\x1b\x18\x88\x02\x8e!!AP&H\xc7\x02 \x16@S\xc2" \
                   b"\x88\x10\x97J\xce\xcf\xd5K,(\xc8I\xd5\xcbI,.)-NMII,IU\x0e\x08\x06)<\xd2\xf7,\x05l\xd0\x83\x0e)\x10\r\x00PK\x07" \
                   b"\x08\x11\xbff\xca\\\x00\x00\x00\xb0\x00\x00\x00PK\x01\x02\x14\x03\x14\x00\x00\x00\x00\x00!\x7f\x17W\x00\x00\x00" \
                   b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0e\x00 \x00\x00\x00\x00\x00\x00\x00\x00\x00\xedA\x00\x00\x00\x00git-lfs-3.4.0/UT\r" \
                   b"\x00\x07\xbe\x8e\xe6d\xbe\x8e\xe6d\xbe\x8e\xe6dux\x0b\x00\x01\x04\xf5\x01\x00\x00\x04\x14\x00\x00\x00PK\x01" \
                   b"\x02\x14\x03\x14\x00\x08\x00\x08\x00QIxT\xc2\x15&\xc4\x1d\x00\x00\x00\x1b\x00\x00\x00\x18\x00 \x00\x00\x00" \
                   b"\x00\x00\x00\x00\x00\x00\xa4\x81L\x00\x00\x00git-lfs-3.4.0/install.shUT\r\x00\x07\xfb\x97<b\xc4\x8e\xe6d\xc4" \
                   b"\x8e\xe6dux\x0b\x00\x01\x04\xf5\x01\x00\x00\x04\x14\x00\x00\x00PK\x01\x02\x14\x03\x14\x00\x08\x00\x08\x00QIxT" \
                   b"\x11\xbff\xca\\\x00\x00\x00\xb0\x00\x00\x00#\x00 \x00\x00\x00\x00\x00\x00\x00\x00\x00\xa4\x81\xcf\x00\x00" \
                   b"\x00__MACOSX/git-lfs-3.4.0/._install.shUT\r\x00\x07\xfb\x97<b\xc4\x8e\xe6d\xcc\x8e\xe6dux\x0b\x00\x01\x04" \
                   b"\xf5\x01\x00\x00\x04\x14\x00\x00\x00PK\x05\x06\x00\x00\x00\x00\x03\x00\x03\x003\x01\x00\x00\x9c\x01\x00\x00\x00\x00"

    def setUp(self):
        super(TestInstallGitLFS, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path):
            self.assertEqual(1, program.main(
                args=('install-git-lfs',),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "`git lfs` only supported from git repositories\n")

    def test_remote_git(self):
        with OutputCapture() as captured, mocks.remote.GitHub():
            self.assertEqual(1, program.main(
                args=('-C', 'https://github.example.com/WebKit/WebKit', 'install-git-lfs'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'Cannot install `git lfs` from a remote repository\n')

    def test_url(self):
        with patch('platform.system', lambda: 'Windows'):
            self.assertIsNone(program.InstallGitLFS.url())

        with patch('platform.system', lambda: 'Darwin'), patch('platform.machine', lambda: 'x86_64'):
            self.assertEqual(
                program.InstallGitLFS.url(),
                'https://github.com/git-lfs/git-lfs/releases/download/v3.4.0/git-lfs-darwin-amd64-v3.4.0.zip',
            )

        with patch('platform.system', lambda: 'Darwin'), patch('platform.machine', lambda: 'arm64'), patch('whichcraft.which', lambda name: '/bin/{}'.format(name)):
            self.assertEqual(
                program.InstallGitLFS.url(),
                'https://github.com/git-lfs/git-lfs/releases/download/v3.4.0/git-lfs-darwin-arm64-v3.4.0.zip',
            )

        with patch('platform.system', lambda: 'Darwin'), patch('platform.machine', lambda: 'arm64'), patch('whichcraft.which', lambda name: None):
            self.assertEqual(
                program.InstallGitLFS.url(),
                'https://github.com/git-lfs/git-lfs/releases/download/v3.4.0/git-lfs-darwin-arm64-v3.4.0.zip',
            )

    def test_install(self):
        with OutputCapture() as captured, mocks.remote.GitHub('github.com/git-lfs/git-lfs', releases={
            'v3.4.0/git-lfs-darwin-arm64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
            'v3.4.0/git-lfs-darwin-amd64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
        }), mocks.local.Git(self.path), mocks.local.Svn(), patch('platform.system', lambda: 'Darwin'), patch('platform.machine', lambda: 'x86_64'):
            self.assertIsNone(local.Git(self.path).config().get('lfs.repositoryformatversion'))
            self.assertEqual(0, program.main(
                args=('install-git-lfs',),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).config()['lfs.repositoryformatversion'], '0')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Installing `git lfs` version 3.4\n'
            'Git LFS initialized.\n'
            "Configured `git lfs` for '{}'\n".format(self.path),
        )

    def test_configure(self):
        with OutputCapture() as captured, mocks.remote.GitHub('github.com/git-lfs/git-lfs', releases={
            'v3.4.0/git-lfs-darwin-arm64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
            'v3.4.0/git-lfs-darwin-amd64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
        }), mocks.local.Git(self.path) as mocked, mocks.local.Svn(), patch('platform.system', lambda: 'Darwin'), patch('platform.machine', lambda: 'x86_64'):
            mocked.has_git_lfs = True

            self.assertIsNone(local.Git(self.path).config().get('lfs.repositoryformatversion'))
            self.assertEqual(0, program.main(
                args=('install-git-lfs',),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).config()['lfs.repositoryformatversion'], '0')

        self.assertEqual(
            captured.stdout.getvalue(),
            '`git lfs` version 3.4 is already installed\n'
            "Configured `git lfs` for '{}'\n".format(self.path),
        )

    def test_no_op(self):
        with OutputCapture() as captured, mocks.remote.GitHub('github.com/git-lfs/git-lfs', releases={
            'v3.4.0/git-lfs-darwin-arm64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
            'v3.4.0/git-lfs-darwin-amd64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
        }), mocks.local.Git(self.path) as mocked, mocks.local.Svn(), patch('platform.system', lambda: 'Darwin'), patch('platform.machine', lambda: 'x86_64'):
            mocked.has_git_lfs = True
            mocked.edit_config('lfs.repositoryformatversion', '0')

            self.assertEqual(0, program.main(
                args=('install-git-lfs',),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).config()['lfs.repositoryformatversion'], '0')

        self.assertEqual(
            captured.stdout.getvalue(),
            '`git lfs` version 3.4 is already installed\n'
            "`git lfs` is already configured for '{}'\n".format(self.path),
        )

    def test_no_repo(self):
        with OutputCapture() as captured, mocks.remote.GitHub('github.com/git-lfs/git-lfs', releases={
            'v3.4.0/git-lfs-darwin-arm64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
            'v3.4.0/git-lfs-darwin-amd64-v3.4.0.zip': wkmocks.Response(status_code=200, content=self.ZIP_CONTENTS),
        }), mocks.local.Git(), mocks.local.Svn(), patch('platform.system', lambda: 'Darwin'), patch('platform.machine', lambda: 'x86_64'):
            self.assertEqual(0, program.main(
                args=('install-git-lfs',),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Installing `git lfs` version 3.4\n'
            'Git LFS initialized.\n'
            "No repository provided, skipping configuring `git lfs`\n",
        )
