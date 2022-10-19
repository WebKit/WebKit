# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks


class TestFind(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestFind, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_basic_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '5@main | d8bce26fa65c | Patch Series\n')

    def test_basic_git_svn(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '5@main | d8bce26fa65c, r9 | Patch Series\n')

    def test_basic_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@trunk | r6 | 6th commit\n')

    def test_basic_svn_remote(self):
        with OutputCapture() as captured, mocks.remote.Svn():
            self.assertEqual(0, program.main(
                args=('-C', 'https://svn.example.org/repository/webkit', 'find', 'HEAD', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@trunk | r6 | 6th commit\n')

    def test_basic_github_remote(self):
        with OutputCapture() as captured, mocks.remote.GitHub():
            self.assertEqual(0, program.main(
                args=('-C', 'https://github.example.com/WebKit/WebKit', 'find', 'bae5d1e90999', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@main | bae5d1e90999 | 8th commit\n')

    def test_basic_bitbucket_remote(self):
        with OutputCapture() as captured, mocks.remote.BitBucket():
            self.assertEqual(0, program.main(
                args=('-C', 'https://bitbucket.example.com/projects/WEBKIT/repos/webkit', 'find', 'bae5d1e90999', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@main | bae5d1e90999 | 8th commit\n')

        with OutputCapture() as captured, mocks.remote.BitBucket():
            self.assertEqual(0, program.main(
                args=('-C', 'https://bitbucket.example.com/WEBKIT/webkit', 'find', 'bae5d1e90999', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@main | bae5d1e90999 | 8th commit\n')

    def test_branch_tilde(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'branch-a~1', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.1@branch-a | a30ce8494bf1, r3 | 3rd commit\n')

    def test_identifier_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '2@main', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2@main | fff83bb2d917 | 2nd commit\n')

    def test_identifier_git_svn(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '2@main', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2@main | fff83bb2d917 | 2nd commit\n')

    def test_identifier_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '2@trunk', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2@trunk | r2 | 2nd commit\n')

    def test_hash(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '790725a6', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.3@branch-b | 790725a6d79e | 7th commit\n')

    def test_revision_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'r5', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.2@branch-b | r5 | 5th commit\n')

    def test_revision_git_svn(self):
        try:
            dirname = tempfile.mkdtemp()
            with OutputCapture() as captured, mocks.local.Git(dirname, git_svn=True, remote='git@example.org:{}'.format(self.path)), mocks.local.Svn(), MockTime:
                self.assertEqual(0, program.main(
                    args=('find', 'r5', '-q'),
                    path=dirname,
                ))
            self.assertEqual(captured.stdout.getvalue(), '2.2@branch-b | 3cd32e352410, r5 | 5th commit\n')
        finally:
            shutil.rmtree(dirname)

    def test_standard(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '3@main'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '''Title: 4th commit
Author: Jonathan Bedard <jbedard@apple.com>
Date: {}
Revision: 4
Hash: 1abe25b443e9
Identifier: 3@main
'''.format(datetime.fromtimestamp(1601663000).strftime('%a %b %d %H:%M:%S %Y')),
        )

    def test_standard_list(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '2@main..4@main'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '''Title: 8th commit
Author: Jonathan Bedard <jbedard@apple.com>
Date: {}
Revision: 8
Hash: bae5d1e90999
Identifier: 4@main
--------------------
Title: 4th commit
Author: Jonathan Bedard <jbedard@apple.com>
Date: {}
Revision: 4
Hash: 1abe25b443e9
Identifier: 3@main
'''.format(datetime.fromtimestamp(1601668000).strftime('%a %b %d %H:%M:%S %Y'), datetime.fromtimestamp(1601663000).strftime('%a %b %d %H:%M:%S %Y')),
        )

    def test_verbose(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '3@main', '-v'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '''Title: 4th commit
Author: Jonathan Bedard <jbedard@apple.com>
Date: {}
Revision: 4
Hash: 1abe25b443e9
Identifier: 3@main
    4th commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@4 268f45cc-cd09-0410-ab3c-d52691b4dbfc
'''.format(datetime.fromtimestamp(1601663000).strftime('%a %b %d %H:%M:%S %Y')),
        )

    def test_quiet(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '3@main', '-q'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '3@main | 1abe25b443e9, r4 | 4th commit\n',
        )

    def test_failed_list(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('find', '2@main...4@main', '-q'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stderr.getvalue(),
            "'find' sub-command only supports '..' notation\n",
        )

    def test_quiet_list(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '2@main..4@main', '-q'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '4@main | bae5d1e90999, r8 | 8th commit\n3@main | 1abe25b443e9, r4 | 4th commit\n',
        )

    def test_json(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
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
                author=dict(
                    name='Jonathan Bedard',
                    emails=['jbedard@apple.com'],
                ), timestamp=1601663000,
                order=0,
                branch='main',
                message='4th commit\ngit-svn-id: https://svn.example.org/repository/repository/trunk@4 268f45cc-cd09-0410-ab3c-d52691b4dbfc',
            ))

    def test_json_list(self):
        self.maxDiff = None
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '2@main..4@main', '--json'),
                path=self.path,
            ))

        decoded = json.loads(captured.stdout.getvalue())
        self.assertEqual(
            decoded, [dict(
                identifier='4@main',
                hash='bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                revision=8,
                author=dict(
                    name='Jonathan Bedard',
                    emails=['jbedard@apple.com'],
                ), timestamp=1601668000,
                order=0,
                branch='main',
                message='8th commit\ngit-svn-id: https://svn.example.org/repository/repository/trunk@8 268f45cc-cd09-0410-ab3c-d52691b4dbfc',
            ), dict(
                identifier='3@main',
                hash='1abe25b443e985f93b90d830e4a7e3731336af4d',
                revision=4,
                author=dict(
                    name='Jonathan Bedard',
                    emails=['jbedard@apple.com'],
                ), timestamp=1601663000,
                order=0,
                branch='main',
                message='4th commit\ngit-svn-id: https://svn.example.org/repository/repository/trunk@4 268f45cc-cd09-0410-ab3c-d52691b4dbfc',
            )])

    def test_tag_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'tag-1', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.3@tags/tag-1 | r9 | 9th commit\n')

    def test_tag_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True) as mock, mocks.local.Svn(), MockTime:
            mock.tags['tag-1'] = mock.commits['branch-a'][-1]

            self.assertEqual(0, program.main(
                args=('find', 'tag-1', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '2.2@branch-a | 621652add7fc, r6 | 7th commit\n')

    def test_no_log_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'trunk', '--no-log', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@trunk | r6\n')

    def test_no_log_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', 'main', '--no-log', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '5@main | d8bce26fa65c, r9\n')

    def test_timezone_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path, utc_offset='-0600'), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '3@trunk'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '''Title: 4th commit
Author: jbedard@apple.com <jbedard@apple.com>
Date: {}
Revision: 4
Identifier: 3@trunk
'''.format(datetime.fromtimestamp(1601684700).strftime('%a %b %d %H:%M:%S %Y')),
        )

    def test_scope_single(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(1, program.main(
                args=('find', 'HEAD', '--scope', 'some/dir'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), 'Scope argument invalid when only one commit specified\n')

    def test_scope_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('find', '1@main..HEAD', '--scope', 'some/dir', '-q'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '5@main | d8bce26fa65c | Patch Series\n3@main | 1abe25b443e9 | 4th commit\n',
        )

    def test_scope_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(1, program.main(
                args=('find', '1@main..HEAD', '--scope', 'some/dir'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "Can only use the '--scope' argument on a native Git repository\n")

    def test_scope_github_remote(self):
        with OutputCapture() as captured, mocks.remote.GitHub():
            self.assertEqual(1, program.main(
                args=('-C', 'https://github.example.com/WebKit/WebKit', 'find', '1@main..HEAD', '--scope', 'some/dir'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "Can only use the '--scope' argument on a native Git repository\n")

    def test_scope_bitbucket_remote(self):
        with OutputCapture() as captured, mocks.remote.BitBucket():
            self.assertEqual(1, program.main(
                args=('-C', 'https://bitbucket.example.com/projects/WEBKIT/repos/webkit', 'find', '1@main..HEAD', '--scope', 'some/dir'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "Can only use the '--scope' argument on a native Git repository\n")


class TestInfo(testing.TestCase):
    path = '/mock/repository'

    def test_basic_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('info', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '5@main | d8bce26fa65c | Patch Series\n')

    def test_basic_git_svn(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('info', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '5@main | d8bce26fa65c, r9 | Patch Series\n')

    def test_basic_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(0, program.main(
                args=('info', '-q'),
                path=self.path,
            ))
        self.assertEqual(captured.stdout.getvalue(), '4@trunk | r6 | 6th commit\n')
