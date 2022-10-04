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
import shutil
import tempfile

from mock import patch
from webkitbugspy import Tracker, radar, mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks


class TestCherryPick(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestCherryPick, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('cherry-pick', 'd8bce26fa65c'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_basic(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime:
            repo.head = repo.commits['branch-a'][-1]
            self.assertEqual(0, program.main(
                args=('cherry-pick', 'd8bce26fa65c'),
                path=self.path,
            ))
            self.assertEqual(repo.head.hash, '5848f06de77d306791b7410ff2197bf3dd82b9e9')
            self.assertEqual(repo.head.message, 'Cherry-pick 5@main (d8bce26fa65c). <bug>\n    Patch Series\n')

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_alternate_issue(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), bmocks.Radar(
            issues=bmocks.ISSUES,
        ), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), MockTime:
            repo.head = repo.commits['branch-a'][-1]
            self.assertEqual(0, program.main(
                args=('cherry-pick', 'd8bce26fa65c', '-i', '<rdar://problem/123>'),
                path=self.path,
            ))
            self.assertEqual(repo.head.hash, 'bae505f206a290592fd251b057874d2d9d931202')
            self.assertEqual(repo.head.message, 'Cherry-pick 5@main (d8bce26fa65c). rdar://123\n    Patch Series\n')

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
