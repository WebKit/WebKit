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

import logging
import os
import time
import sys

from datetime import datetime

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Terminal as MockTerminal
from webkitscmpy import program, mocks


class TestPublish(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestPublish, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path):
            self.assertEqual(1, program.main(
                args=('publish', 'branch-b'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), "Can only 'publish' branches in a git checkout\n")

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('publish', 'branch-b'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_git(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTerminal.input('y'):
            self.assertEqual(0, program.main(
                args=('publish', 'branch-b', '-v'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Pushing the branches at the following commits to origin:\n'
            '    branch-b @ 2.3@branch-b\n\n'
            "Are you sure you would like to publish to 'origin' remote? (Yes/[No]): \n"
            'Pushing branches to origin...\n'
            'Publication succeeded!\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                '    branch-b is a branch, referencing 2.3@branch-b',
                'User specified 1 branch and 0 tags, request comprises 1 commit',
                "Inferred 'origin' as the target remote",
                "Invoking '/usr/bin/git push --atomic origin 790725a6d79e:refs/heads/branch-b'",
            ],
        )

    def test_git_cancel(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTerminal.input('n'):
            self.assertEqual(1, program.main(
                args=('publish', 'branch-b'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Pushing the branches at the following commits to origin:\n'
            '    branch-b @ 2.3@branch-b\n\n'
            "Are you sure you would like to publish to 'origin' remote? (Yes/[No]): \n",
        )
        self.assertEqual(captured.stderr.getvalue(), 'Publication canceled\n')

    def test_git_exclude(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTerminal.input('n'):
            self.assertEqual(1, program.main(
                args=('publish', 'branch-b', '--exclude', 'branch-b'),
                path=self.path,
            ))

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(
            captured.stderr.getvalue(),
            "'branch-b' has been explicitly excluded from publication\n"
            "No branches or tags to publish found\n",
        )
