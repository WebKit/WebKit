# Copyright (C) 2020 Apple Inc. All rights reserved.
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
import unittest

from datetime import datetime
from webkitcorepy import OutputCapture
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks


class TestFind(unittest.TestCase):
    path = '/mock/repository'

    def test_basic_git(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@main | bae5d1e90999\n')

    def test_basic_git_svn(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@main | bae5d1e90999, r6 | 6th commit\n')

    def test_basic_svn(self):
        with mocks.local.Git(), mocks.local.Svn(self.path), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@trunk | r6 | 6th commit\n')

    def test_branch_tilde(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'branch-a~1', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.1@branch-a | a30ce8494bf1, r3 | 3rd commit\n')

    def test_identifier_git(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', '2@main', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2@main | fff83bb2d917\n')

    def test_identifier_git_svn(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', '2@main', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2@main | fff83bb2d917\n')

    def test_identifier_svn(self):
        with mocks.local.Git(), mocks.local.Svn(self.path), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', '2@trunk', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2@trunk | r2 | 2nd commit\n')

    def test_hash(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', '3cd32e352410', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.2@branch-b | 3cd32e352410\n')

    def test_revision_svn(self):
        with mocks.local.Git(), mocks.local.Svn(self.path), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'r5', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.2@branch-b | r5 | 5th commit\n')

    def test_revision_git_svn(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'r5', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.2@branch-b | 3cd32e352410, r5 | 5th commit\n')

    def test_standard(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', '3@main'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '''Title: 4th commit
Author: Jonathan Bedard <jbedard@apple.com>
Identifier: 3@main
Date: {}
Revision: 4
Hash: 1abe25b443e9
'''.format(datetime.fromtimestamp(1601663000).strftime('%a %b %d %H:%M:%S %Y')),
        )

    def test_verbose(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', '3@main', '-v'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '''Title: 4th commit
Author: Jonathan Bedard <jbedard@apple.com>
Identifier: 3@main
Date: {}
Revision: 4
Hash: 1abe25b443e9
    4th commit
    svn-id: https://svn.webkit.orgrepository/repository/trunk@4 268f45cc-cd09-0410-ab3c-d52691b4dbfc
'''.format(datetime.fromtimestamp(1601663000).strftime('%a %b %d %H:%M:%S %Y')),
        )

    def test_json(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', '3@main', '--json'),
                path=self.path,
            ))

        decoded = json.loads(captured.stdout.getvalue())
        self.assertDictEqual(
            decoded, dict(
                identifier='3@main',
                hash='1abe25b443e985f93b90d830e4a7e3731336af4d',
                revision=4,
                author='jbedard@apple.com',
                timestamp=1601663000,
                branch='main',
                message='4th commit\nsvn-id: https://svn.webkit.orgrepository/repository/trunk@4 268f45cc-cd09-0410-ab3c-d52691b4dbfc',
            ))

    def test_tag_svn(self):
        with mocks.local.Git(), mocks.local.Svn(self.path), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'tag-1', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.3@tags/tag-1 | r9 | 9th commit\n')

    def test_tag_git(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            self.assertEqual(0, program.main(
                args=('find', 'tag-1', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.2@branch-a | 621652add7fc, r7 | 7th commit\n')
