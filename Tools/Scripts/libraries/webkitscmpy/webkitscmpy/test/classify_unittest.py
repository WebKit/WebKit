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

import os

from webkitcorepy import OutputCapture, Terminal, testing
from webkitscmpy import program, mocks, CommitClassifier


class TestClassify(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestClassify, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_no_classes(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(255, program.main(
                args=('classify',),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), 'Repository does not specify any commit classifications\n')

    def test_list_classes(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('classify', '-l'),
                path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(
                    name='Versioning',
                    headers=[r"Versioning\.$", r'^Revert "?[Vv]ersioning\.?"?$'],
                )])
            ))
        self.assertEqual(captured.stdout.getvalue(), 'Versioning\n')
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_header_success(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('classify', 'HEAD'),
                path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(
                    name='Series',
                    headers=[r'^Patch Series'],
                )])
            ))
        self.assertEqual(captured.stdout.getvalue(), 'Series\n')
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_header_failure(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('classify', 'HEAD'),
                path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(
                    name='Series',
                    headers=[r'^\?'],
                )])
            ))
        self.assertEqual(captured.stdout.getvalue(), 'None\n')
        self.assertEqual(
            captured.stderr.getvalue(),
            'Provided commit does not match a known class in this repository\n',
        )

    def test_path_success(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(0, program.main(
                args=('classify', 'HEAD'),
                path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(
                    name='Change',
                    paths=['Source'],
                )])
            ))
        self.assertEqual(captured.stdout.getvalue(), 'Change\n')
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_path_failure(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('classify', 'HEAD'),
                path=self.path,
                classifier=CommitClassifier([CommitClassifier.CommitClass(
                    name='Tools',
                    paths=['Tests', 'metadata'],
                )])
            ))
        self.assertEqual(captured.stdout.getvalue(), 'None\n')
        self.assertEqual(
            captured.stderr.getvalue(),
            'Provided commit does not match a known class in this repository\n',
        )
