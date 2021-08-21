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

import json
import os
import shutil
import tempfile

from datetime import datetime
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime, Terminal as MockTerminal
from webkitscmpy import local, program, mocks


class TestBranch(testing.PathTestCase):
    basepath = 'mock/repository'

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
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('branch', '-i', '1234'),
                path=self.path,
            ))
            self.assertEqual(local.Git(self.path).branch, 'eng/1234')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/1234'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Created the local development branch 'eng/1234'!\n")

    def test_prompt_git(self):
        with MockTerminal.input('eng/example'), OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(args=('branch',), path=self.path))
            self.assertEqual(local.Git(self.path).branch, 'eng/example')
        self.assertEqual(captured.root.log.getvalue(), "Creating the local development branch 'eng/example'...\n")
        self.assertEqual(captured.stdout.getvalue(), "Branch name: \nCreated the local development branch 'eng/example'!\n")

    def test_invalid_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('branch', '-i', 'reject_underscores'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "'eng/reject_underscores' is an invalid branch name, cannot create it\n")
