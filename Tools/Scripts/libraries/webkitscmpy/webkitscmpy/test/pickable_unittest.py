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
import time
import sys

from datetime import datetime

from webkitcorepy import OutputCapture, Terminal, testing
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import local, program, mocks, Commit


class TestPickable(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestPickable, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(1, program.main(
                args=('pickable', 'main'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), "Can only run 'pickable' on a native Git repository\n")

    def test_main_no_into(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(1, program.main(
                args=('pickable', 'main'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stderr.getvalue(),
            "Cannot merge 'main' into itself\n"
            "Specify branch to merge into with the --into flag\n"
        )

    def test_main(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(0, program.main(
                args=('pickable', 'main', '--into', 'branch-a'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '5@main | d8bce26fa65c | Patch Series\n'
            '4@main | bae5d1e90999 | 8th commit\n'
            '3@main | 1abe25b443e9 | 4th commit\n',
        )

    def test_scope(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(0, program.main(
                args=('pickable', 'main', '--into', 'branch-a', '--scope', 'some/path'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '5@main | d8bce26fa65c | Patch Series\n'
            '3@main | 1abe25b443e9 | 4th commit\n',
        )

    def test_main_git_svn(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(0, program.main(
                args=('pickable', 'main', '--into', 'branch-a'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '5@main | d8bce26fa65c, r9 | Patch Series\n'
            '4@main | bae5d1e90999, r8 | 8th commit\n'
            '3@main | 1abe25b443e9, r4 | 4th commit\n',
        )

    def test_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            repo.commits['safari-xxx-branch'] = [
                repo.commits['main'][3],
                Commit(
                    hash='6eedcf4492c3b14a97b886c4df59e8698f10539f',
                    author=repo.commits['main'][3].author,
                    timestamp=int(time.time()) - 1000,
                    branch='safari-xxx-branch',
                    message='Versioning.\n',
                    identifier='4.1@safari-xxx-branch',
                ), Commit(
                    hash='efbdb34cc1454d1772838a41e22df23d231567b9',
                    author=repo.commits['main'][3].author,
                    timestamp=int(time.time()) - 100,
                    branch='safari-xxx-branch',
                    message='[WebKit] Some change\n',
                    identifier='4.2@safari-xxx-branch',
                ), Commit(
                    hash='d10204abba135b5b9588936d7eec7cd001d07d0f',
                    author=repo.commits['main'][3].author,
                    timestamp=int(time.time()) - 10,
                    branch='safari-xxx-branch',
                    message='Cherry-pick d8bce26fa65c.\n    Patch Series\n',
                    identifier='4.3@safari-xxx-branch',
                ),
            ]
            self.assertEqual(0, program.main(
                args=('pickable', 'safari-xxx-branch'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '4.2@safari-xxx-branch | efbdb34cc145 | [WebKit] Some change\n',
        )

    def test_branch_none(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            repo.commits['safari-xxx-branch'] = [
                repo.commits['main'][3],
                Commit(
                    hash='6eedcf4492c3b14a97b886c4df59e8698f10539f',
                    author=repo.commits['main'][3].author,
                    timestamp=int(time.time()) - 1000,
                    branch='safari-xxx-branch',
                    message='Versioning.\n',
                    identifier='4.1@safari-xxx-branch',
                ),
            ]
            self.assertEqual(1, program.main(
                args=('pickable', 'safari-xxx-branch'),
                path=self.path,
            ))

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(
            captured.stderr.getvalue(),
            "No commits in specified range are 'pickable'\n",
        )

    def test_branch_include_versioning(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            repo.commits['safari-xxx-branch'] = [
                repo.commits['main'][3],
                Commit(
                    hash='6eedcf4492c3b14a97b886c4df59e8698f10539f',
                    author=repo.commits['main'][3].author,
                    timestamp=int(time.time()) - 1000,
                    branch='safari-xxx-branch',
                    message='Versioning.\n',
                    identifier='4.1@safari-xxx-branch',
                ),
            ]
            self.assertEqual(0, program.main(
                args=('pickable', 'safari-xxx-branch', '--exclude', 'Cherry-pick'),
                path=self.path,
            ))

        self.assertEqual(captured.stdout.getvalue(), '4.1@safari-xxx-branch | 6eedcf4492c3 | Versioning.\n')

    def test_branch_gardening_exclude(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            project_config = os.path.join(self.path, 'metadata', local.Git.GIT_CONFIG_EXTENSION)
            os.mkdir(os.path.dirname(project_config))
            with open(project_config, 'w') as f:
                f.write('[webkitscmpy "tests"]\n')
                f.write('    api-tests = Source\n')

            repo.commits['safari-xxx-branch'] = [
                repo.commits['main'][3],
                Commit(
                    hash='6eedcf4492c3b14a97b886c4df59e8698f10539f',
                    author=repo.commits['main'][3].author,
                    timestamp=int(time.time()) - 1000,
                    branch='safari-xxx-branch',
                    message='[GARDENING] Mark test/abc as failing\n',
                    identifier='4.1@safari-xxx-branch',
                ),
            ]
            self.assertEqual(1, program.main(
                args=('pickable', 'safari-xxx-branch'),
                path=self.path,
            ))

        self.assertEqual(captured.stderr.getvalue(), "No commits in specified range are 'pickable'\n")
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_branch_gardening_include(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            repo.commits['safari-xxx-branch'] = [
                repo.commits['main'][3],
                Commit(
                    hash='6eedcf4492c3b14a97b886c4df59e8698f10539f',
                    author=repo.commits['main'][3].author,
                    timestamp=int(time.time()) - 1000,
                    branch='safari-xxx-branch',
                    message='[GARDENING] Mark test/abc as failing\n',
                    identifier='4.1@safari-xxx-branch',
                ),
            ]
            self.assertEqual(0, program.main(
                args=('pickable', 'safari-xxx-branch'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '4.1@safari-xxx-branch | 6eedcf4492c3 | [GARDENING] Mark test/abc as failing\n',
        )
