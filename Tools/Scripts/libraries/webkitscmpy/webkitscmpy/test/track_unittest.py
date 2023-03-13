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
import time
import sys

from datetime import datetime

from webkitcorepy import OutputCapture, Terminal, testing
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import local, program, mocks, Commit


class TestTrack(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestTrack, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(1, program.main(
                args=('track', 'main'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_main(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(0, program.main(
                args=('track', 'main'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Tracking 'main' via 'remotes/origin'\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, remotes={
            'origin': 'git@github.example.com:WebKit/WebKit.git',
            'fork': 'git@github.example.com:Contributor/WebKit.git',
            'security': 'git@github.example.com:WebKit/WebKit-security.git',
            'security-fork': 'git@github.example.com:Contributor/WebKit-security.git',
        }) as mocked, mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            mocked.remotes['security/hidden-branch'] = [mocked.commits[mocked.branch][-1]]
            mocked.remotes['security-fork/hidden-branch'] = [mocked.commits[mocked.branch][-1]]

            project_config = os.path.join(self.path, 'metadata', local.Git.GIT_CONFIG_EXTENSION)
            os.mkdir(os.path.dirname(project_config))
            with open(project_config, 'w') as f:
                f.write('[webkitscmpy "remotes.origin"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit.git\n')
                f.write('    security-level = 0\n')
                f.write('[webkitscmpy "remotes.security"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit-security.git\n')
                f.write('    security-level = 1\n')

            self.assertEqual(0, program.main(
                args=('track', 'hidden-branch'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Tracking 'hidden-branch' via 'remotes/security'\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_eng_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, remotes={
            'origin': 'git@github.example.com:WebKit/WebKit.git',
            'fork': 'git@github.example.com:Contributor/WebKit.git',
            'security': 'git@github.example.com:WebKit/WebKit-security.git',
            'security-fork': 'git@github.example.com:Contributor/WebKit-security.git',
        }) as mocked, mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            mocked.remotes['security/hidden-branch'] = [mocked.commits[mocked.branch][-1]]
            mocked.remotes['security-fork/hidden-branch'] = [mocked.commits[mocked.branch][-1]]
            mocked.remotes['security-fork/eng/hidden'] = [mocked.commits[mocked.branch][-1]]

            project_config = os.path.join(self.path, 'metadata', local.Git.GIT_CONFIG_EXTENSION)
            os.mkdir(os.path.dirname(project_config))
            with open(project_config, 'w') as f:
                f.write('[webkitscmpy "remotes.origin"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit.git\n')
                f.write('    security-level = 0\n')
                f.write('[webkitscmpy "remotes.security"]\n')
                f.write('    url = git@github.example.com:WebKit/WebKit-security.git\n')
                f.write('    security-level = 1\n')

            self.assertEqual(0, program.main(
                args=('track', 'eng/hidden'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            "Tracking 'eng/hidden' via 'remotes/security-fork'\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
