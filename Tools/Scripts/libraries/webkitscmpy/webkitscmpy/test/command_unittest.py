# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import abc
import os
import sys

from unittest.mock import patch

from webkitcorepy import OutputCapture, Terminal, testing
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks


class TestFilteredCommandBase(testing.PathTestCase, abc.ABC):
    basepath = 'mock/repository'

    @property
    @abc.abstractmethod
    def representation(self) -> str:
        ...

    def setUp(self):
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def _run_log(self, git, ref, args=()):
        with OutputCapture() as captured, git, mocks.local.Svn(), MockTime, patch('time.timezone', 0), Terminal.override_atty(sys.stdin, isatty=False):
            program.main(args=('log', ref, self.representation) + args, path=self.path)
        return captured.stdout.getvalue()


class TestFilteredCommandHash(TestFilteredCommandBase):
    representation = '--hash'

    def test_output(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        self.assertEqual(
            'commit 9b8311f25a77 (1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    1st commit\n',
            self._run_log(git, ref=root.hash),
        )

    def test_abbrev_commit(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        self.assertEqual(
            'commit 9b8311f (1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    1st commit\n',
            self._run_log(git, ref=root.hash, args=('--abbrev-commit',)),
        )

    def test_oneline(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        self.assertEqual(
            '9b8311f (1@main) 1st commit\n',
            self._run_log(git, ref=root.hash, args=('--oneline',)),
        )

    def test_decorate(self):
        git = mocks.local.Git(self.path)
        tip = git.commits['main'][-1]
        output = self._run_log(git, ref=tip.hash, args=('--decorate',))
        lines = output.splitlines(keepends=True)
        self.assertEqual(lines[0], 'commit d8bce26fa65c (5@main) (main)\n')
        self.assertEqual(lines[1], 'Author: Jonathan Bedard <jbedard@apple.com>\n')
        self.assertEqual(lines[2], 'Date:   Fri Oct 02 19:46:40 2020 +0000\n')

    def test_decorate_multiple_branches(self):
        git = mocks.local.Git(self.path)
        git.commits['test-branch'] = git.commits['main'][:]
        tip = git.commits['main'][-1]
        output = self._run_log(git, ref=tip.hash, args=('--decorate',))
        lines = output.splitlines(keepends=True)
        self.assertEqual(lines[0], 'commit d8bce26fa65c (5@main) (main, test-branch)\n')

    def test_decorate_non_tip_commit(self):
        git = mocks.local.Git(self.path)
        non_tip = git.commits['main'][0]
        output = self._run_log(git, ref=non_tip.hash, args=('--decorate',))
        lines = output.splitlines(keepends=True)
        self.assertEqual(lines[0], 'commit 9b8311f25a77 (1@main)\n')

    def test_identifier_in_body_transformed(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also 5@main for context.\n'
        self.assertEqual(
            'commit 9b8311f25a77 (1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also 5@main (d8bce26fa65c) for context.\n',
            self._run_log(git, ref=root.hash),
        )

    def test_hash_in_body_not_transformed(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also 9b8311f25a77ba14923d9d5a6532103f54abefcb for context.\n'
        self.assertEqual(
            'commit 9b8311f25a77 (1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also 9b8311f25a77ba14923d9d5a6532103f54abefcb for context.\n',
            self._run_log(git, ref=root.hash),
        )

    def test_revision_in_body_transformed(self):
        git = mocks.local.Git(self.path, git_svn=True)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also r1 for context.\n'
        self.assertEqual(
            'commit 9b8311f25a77 (1@main, r1)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also r1 (9b8311f25a77) for context.\n'
            '    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n',
            self._run_log(git, ref=root.hash),
        )

    def test_canonical_link_hyphen_not_transformed(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nCanonical-link: https://commits.webkit.org/5@main\n'
        self.assertEqual(
            'commit 9b8311f25a77 (1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    Canonical-link: https://commits.webkit.org/5@main\n',
            self._run_log(git, ref=root.hash),
        )

    def test_canonical_link_space_not_transformed(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nCanonical link: https://commits.webkit.org/5@main\n'
        self.assertEqual(
            'commit 9b8311f25a77 (1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    Canonical link: https://commits.webkit.org/5@main\n',
            self._run_log(git, ref=root.hash),
        )

    def test_git_svn_id_not_transformed(self):
        git = mocks.local.Git(self.path, git_svn=True)
        root = git.commits['main'][0]
        self.assertEqual(
            'commit 9b8311f25a77 (1@main, r1)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    1st commit\n'
            '    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n',
            self._run_log(git, ref=root.hash),
        )


class TestFilteredCommandIdentifier(TestFilteredCommandBase):
    representation = '--identifier'

    def test_output(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        self.assertEqual(
            'commit 1@main (9b8311f25a77ba14923d9d5a6532103f54abefcb)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    1st commit\n',
            self._run_log(git, ref=root.hash),
        )

    def test_hash_in_body_transformed(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also d8bce26fa65c6fc8f39c17927abb77f69fab82fc for context.\n'
        self.assertEqual(
            'commit 1@main (9b8311f25a77ba14923d9d5a6532103f54abefcb)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also d8bce26fa65c6fc8f39c17927abb77f69fab82fc (5@main) for context.\n',
            self._run_log(git, ref=root.hash),
        )

    def test_identifier_in_body_not_transformed(self):
        git = mocks.local.Git(self.path)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also 1@main for context.\n'
        self.assertEqual(
            'commit 1@main (9b8311f25a77ba14923d9d5a6532103f54abefcb)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also 1@main for context.\n',
            self._run_log(git, ref=root.hash),
        )

    def test_revision_in_body_transformed(self):
        git = mocks.local.Git(self.path, git_svn=True)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also r1 for context.\n'
        self.assertEqual(
            'commit 1@main (9b8311f25a77ba14923d9d5a6532103f54abefcb, r1)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also r1 (1@main) for context.\n'
            '    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n',
            self._run_log(git, ref=root.hash),
        )


class TestFilteredCommandRevision(TestFilteredCommandBase):
    representation = '--revision'

    def test_output(self):
        git = mocks.local.Git(self.path, git_svn=True)
        root = git.commits['main'][0]
        self.assertEqual(
            'commit r1 (9b8311f25a77ba14923d9d5a6532103f54abefcb, 1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    1st commit\n'
            '    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n',
            self._run_log(git, ref=root.hash),
        )

    def test_identifier_in_body_transformed(self):
        git = mocks.local.Git(self.path, git_svn=True)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also 5@main for context.\n'
        self.assertEqual(
            'commit r1 (9b8311f25a77ba14923d9d5a6532103f54abefcb, 1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also 5@main (r9) for context.\n'
            '    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n',
            self._run_log(git, ref=root.hash),
        )

    def test_hash_in_body_transformed(self):
        git = mocks.local.Git(self.path, git_svn=True)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also d8bce26fa65c6fc8f39c17927abb77f69fab82fc for context.\n'
        self.assertEqual(
            'commit r1 (9b8311f25a77ba14923d9d5a6532103f54abefcb, 1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also d8bce26fa65c6fc8f39c17927abb77f69fab82fc (r9) for context.\n'
            '    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n',
            self._run_log(git, ref=root.hash),
        )

    def test_revision_in_body_not_transformed(self):
        git = mocks.local.Git(self.path, git_svn=True)
        root = git.commits['main'][0]
        root.message = 'Patch Series\n\nSee also r1 for context.\n'
        self.assertEqual(
            'commit r1 (9b8311f25a77ba14923d9d5a6532103f54abefcb, 1@main)\n'
            'Author: Jonathan Bedard <jbedard@apple.com>\n'
            'Date:   Fri Oct 02 17:33:20 2020 +0000\n'
            '\n'
            '    Patch Series\n'
            '\n'
            '    See also r1 for context.\n'
            '    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n',
            self._run_log(git, ref=root.hash),
        )
