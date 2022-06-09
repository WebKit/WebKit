# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
from webkitscmpy import program, mocks, local, Commit, Contributor


class TestCanonicalize(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestCanonicalize, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_invalid(self):
        with OutputCapture(), mocks.local.Git(), mocks.local.Svn(self.path), MockTime:
            self.assertEqual(1, program.main(
                args=('canonicalize',),
                path=self.path,
            ))

    def test_no_commits(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime:
            self.assertEqual(0, program.main(
                args=('canonicalize',),
                path=self.path,
            ))

        self.assertEqual(captured.stdout.getvalue(), 'No local commits to be edited\n')

    def test_formated_identifier(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as mock, mocks.local.Svn(), MockTime:
            contirbutors = Contributor.Mapping()
            contirbutors.create('\u017dan Dober\u0161ek', 'zdobersek@igalia.com')

            mock.commits[mock.default_branch].append(Commit(
                hash='38ea50d28ae394c9c8b80e13c3fb21f1c262871f',
                branch=mock.default_branch,
                author=Contributor('\u017dan Dober\u0161ek', emails=['zdobersek@igalia.com']),
                identifier=mock.commits[mock.default_branch][-1].identifier + 1,
                timestamp=1601669000,
                message='New commit\n',
            ))

            self.assertEqual(0, program.main(
                args=('canonicalize', '-v',),
                path=self.path,
                contributors=contirbutors,
                identifier_template='Canonical link: https://commits.webkit.org/{}',
            ))

            commit = local.Git(self.path).commit(branch=mock.default_branch)
            self.assertEqual(commit.author, contirbutors['zdobersek@igalia.com'])
            self.assertEqual(commit.message, 'New commit\n\nCanonical link: https://commits.webkit.org/6@main')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite 38ea50d28ae394c9c8b80e13c3fb21f1c262871f (1/1) (--- seconds passed, remaining --- predicted)\n'
            'Overwriting 38ea50d28ae394c9c8b80e13c3fb21f1c262871f\n'
            '1 commit successfully canonicalized!\n',
        )

    def test_existing_identifier(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as mock, mocks.local.Svn(), MockTime:
            contirbutors = Contributor.Mapping()
            contirbutors.create('Jonathan Bedard', 'jbedard@apple.com')

            mock.commits[mock.default_branch].append(Commit(
                hash='38ea50d28ae394c9c8b80e13c3fb21f1c262871f',
                branch=mock.default_branch,
                author=Contributor('Jonathan Bedard', emails=['jbedard@apple.com']),
                identifier=mock.commits[mock.default_branch][-1].identifier + 1,
                timestamp=1601668000,
                message='New commit\n\nIdentifier: {}@{}'.format(
                    mock.commits[mock.default_branch][-1].identifier + 1,
                    mock.default_branch,
                ),
            ))

            self.assertEqual(0, program.main(
                args=('canonicalize', '-v',),
                path=self.path,
                contributors=contirbutors,
            ))

            commit = local.Git(self.path).commit(branch=mock.default_branch)
            self.assertEqual(commit.author, contirbutors['jbedard@apple.com'])
            self.assertEqual(commit.message, 'New commit\n\nIdentifier: 6@main')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite 38ea50d28ae394c9c8b80e13c3fb21f1c262871f (1/1) (--- seconds passed, remaining --- predicted)\n'
            'Overwriting 38ea50d28ae394c9c8b80e13c3fb21f1c262871f\n'
            '1 commit successfully canonicalized!\n',
        )

    def test_git_svn(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True) as mock, mocks.local.Svn(), MockTime:
            contirbutors = Contributor.Mapping()
            contirbutors.create('Jonathan Bedard', 'jbedard@apple.com')

            mock.commits[mock.default_branch].append(Commit(
                hash='766609276fe201e7ce2c69994e113d979d2148ac',
                branch=mock.default_branch,
                author=Contributor('jbedard@apple.com', emails=['jbedard@apple.com']),
                identifier=mock.commits[mock.default_branch][-1].identifier + 1,
                timestamp=1601668000,
                revision=9,
                message='New commit\n',
            ))

            self.assertEqual(0, program.main(
                args=('canonicalize', '-vv'),
                path=self.path,
                contributors=contirbutors,
            ))

            commit = local.Git(self.path).commit(branch=mock.default_branch)
            self.assertEqual(commit.author, contirbutors['jbedard@apple.com'])
            self.assertEqual(
                commit.message,
                'New commit\n\n'
                'Identifier: 6@main\n'
                'git-svn-id: https://svn.example.org/repository/repository/trunk@9 268f45cc-cd09-0410-ab3c-d52691b4dbfc',
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite 766609276fe201e7ce2c69994e113d979d2148ac (1/1) (--- seconds passed, remaining --- predicted)\n'
            'Overwriting 766609276fe201e7ce2c69994e113d979d2148ac\n'
            '    GIT_AUTHOR_NAME=Jonathan Bedard\n'
            '    GIT_AUTHOR_EMAIL=jbedard@apple.com\n'
            '    GIT_COMMITTER_NAME=Jonathan Bedard\n'
            '    GIT_COMMITTER_EMAIL=jbedard@apple.com\n'
            '1 commit successfully canonicalized!\n',
        )

    def test_git_svn_existing(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True) as mock, mocks.local.Svn(), MockTime:
            contirbutors = Contributor.Mapping()
            contirbutors.create('Jonathan Bedard', 'jbedard@apple.com')

            mock.commits[mock.default_branch].append(Commit(
                hash='766609276fe201e7ce2c69994e113d979d2148ac',
                branch=mock.default_branch,
                author=Contributor('jbedard@apple.com', emails=['jbedard@apple.com']),
                identifier=mock.commits[mock.default_branch][-1].identifier + 1,
                timestamp=1601668000,
                revision=9,
                message='New commit\nIdentifier: 6@main\n\n',
            ))

            self.assertEqual(0, program.main(
                args=('canonicalize', '-vv'),
                path=self.path,
                contributors=contirbutors,
            ))

            commit = local.Git(self.path).commit(branch=mock.default_branch)
            self.assertEqual(commit.author, contirbutors['jbedard@apple.com'])
            self.assertEqual(
                commit.message,
                'New commit\n\n'
                'Identifier: 6@main\n'
                'git-svn-id: https://svn.example.org/repository/repository/trunk@9 268f45cc-cd09-0410-ab3c-d52691b4dbfc',
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite 766609276fe201e7ce2c69994e113d979d2148ac (1/1) (--- seconds passed, remaining --- predicted)\n'
            'Overwriting 766609276fe201e7ce2c69994e113d979d2148ac\n'
            '    GIT_AUTHOR_NAME=Jonathan Bedard\n'
            '    GIT_AUTHOR_EMAIL=jbedard@apple.com\n'
            '    GIT_COMMITTER_NAME=Jonathan Bedard\n'
            '    GIT_COMMITTER_EMAIL=jbedard@apple.com\n'
            '1 commit successfully canonicalized!\n',
        )

    def test_branch_commits(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as mock, mocks.local.Svn(), MockTime:
            contirbutors = Contributor.Mapping()
            contirbutors.create('Jonathan Bedard', 'jbedard@apple.com')

            local.Git(self.path).checkout('branch-a')
            mock.commits['branch-a'].append(Commit(
                hash='f93138e3bf1d5ecca25fc0844b7a2a78b8e00aae',
                branch='branch-a',
                author=Contributor('jbedard@apple.com', emails=['jbedard@apple.com']),
                branch_point=mock.commits['branch-a'][-1].branch_point,
                identifier=mock.commits['branch-a'][-1].identifier + 1,
                timestamp=1601668000,
                message='New commit 1\n',
            ))
            mock.commits['branch-a'].append(Commit(
                hash='0148c0df0faf248aa133d6d5ad911d7cb1b56a5b',
                branch='branch-a',
                author=Contributor('jbedard@apple.com', emails=['jbedard@apple.com']),
                branch_point=mock.commits['branch-a'][-1].branch_point,
                identifier=mock.commits['branch-a'][-1].identifier + 1,
                timestamp=1601669000,
                message='New commit 2\n',
            ))

            self.assertEqual(0, program.main(
                args=('canonicalize', ),
                path=self.path,
                contributors=contirbutors,
            ))

            commit_a = local.Git(self.path).commit(branch='branch-a~1')
            self.assertEqual(commit_a.author, contirbutors['jbedard@apple.com'])
            self.assertEqual(commit_a.message, 'New commit 1\n\nIdentifier: 2.3@branch-a')

            commit_b = local.Git(self.path).commit(branch='branch-a')
            self.assertEqual(commit_b.author, contirbutors['jbedard@apple.com'])
            self.assertEqual(commit_b.message, 'New commit 2\n\nIdentifier: 2.4@branch-a')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite f93138e3bf1d5ecca25fc0844b7a2a78b8e00aae (1/2) (--- seconds passed, remaining --- predicted)\n'
            'Rewrite 0148c0df0faf248aa133d6d5ad911d7cb1b56a5b (2/2) (--- seconds passed, remaining --- predicted)\n'
            '2 commits successfully canonicalized!\n',
        )

    def test_number(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as mock, mocks.local.Svn(), MockTime:
            contirbutors = Contributor.Mapping()
            contirbutors.create('Jonathan Bedard', 'jbedard@apple.com')

            self.assertEqual(0, program.main(
                args=('canonicalize', '--number', '3'),
                path=self.path,
                contributors=contirbutors,
            ))

            self.assertEqual(local.Git(self.path).commit(identifier='5@main').message, 'Patch Series\n\nIdentifier: 5@main')
            self.assertEqual(local.Git(self.path).commit(identifier='4@main').message, '8th commit\n\nIdentifier: 4@main')
            self.assertEqual(local.Git(self.path).commit(identifier='3@main').message, '4th commit\n\nIdentifier: 3@main')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Rewrite 1abe25b443e985f93b90d830e4a7e3731336af4d (1/3) (--- seconds passed, remaining --- predicted)\n'
            'Rewrite bae5d1e90999d4f916a8a15810ccfa43f37a2fd6 (2/3) (--- seconds passed, remaining --- predicted)\n'
            'Rewrite d8bce26fa65c6fc8f39c17927abb77f69fab82fc (3/3) (--- seconds passed, remaining --- predicted)\n'
            '3 commits successfully canonicalized!\n',
        )
