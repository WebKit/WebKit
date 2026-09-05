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

import os
from contextlib import contextmanager
from typing import Iterator, NamedTuple, Optional
from unittest.mock import patch

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import ProcessCompletion, Time as MockTime

from webkitscmpy import Commit, local, mocks, program
from webkitscmpy.program import stack_metadata

CONTRIBUTOR = {'name': 'Tim Contributor', 'emails': ['tcontributor@example.com']}


class MockCommit(NamedTuple):
    hash: str
    timestamp: int


COMMITS = {
    'eng/parent': MockCommit('06de5d56554e693db72313f4ca1fb969c30b8ccb', 1601668000),
    'eng/child': MockCommit('b8b921baaad2fd10bc9d0cc9e97f8fa1d6e5f4a1', 1601669000),
    'eng/sibling': MockCommit('2f0c1cbb7e5b6f2a3ba0a4c1e0f79c1c7d4a3b12', 1601669500),
    'eng/other': MockCommit('9d1a3f6c2b8e4d5a7c0f1e2b3a4d5c6e7f809123', 1601669500),
    'eng/grandchild': MockCommit('7c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f60718293', 1601670000),
}


class TestStack(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self) -> None:
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    @contextmanager
    def checkout(self) -> Iterator[tuple[mocks.local.Git, OutputCapture]]:
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            yield repo, captured

    @classmethod
    def add_branch(cls, repo: mocks.local.Git, branch: str, on: Optional[str] = None) -> mocks.local.Git:
        mock = COMMITS[branch]
        base = repo.commits[on or repo.default_branch][-1]
        repo.commits[branch] = [
            base,
            Commit(
                hash=mock.hash,
                branch=branch,
                author=CONTRIBUTOR,
                identifier=base.identifier + 1 if base.branch_point else 1,
                branch_point=base.branch_point or base.identifier,
                timestamp=mock.timestamp,
                message=f"[Testing] {branch.split('/')[-1].capitalize()} change\n",
            ),
        ]
        return repo

    @classmethod
    def add_stack(cls, repo: mocks.local.Git) -> mocks.local.Git:
        cls.add_branch(repo, 'eng/parent')
        cls.add_branch(repo, 'eng/child', on='eng/parent')
        repo.head = repo.commits['eng/child'][-1]
        return repo

    def test_set_parent(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            # Recording the same parent again is not an error
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))

            config = local.Git(self.path).config()
            self.assertEqual(config.get('branch.eng/child.stack-parent'), 'eng/parent')

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/child' is stacked on 'eng/parent'\n" * 2,
        )

    def test_set_parent_missing(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/missing'), path=self.path))

        self.assertEqual(
            captured.stderr.getvalue(),
            "Could not find 'eng/missing' as a branch in this checkout\n",
        )

    def test_set_parent_production(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(1, program.main(args=('stack', '--on', 'main'), path=self.path))
            self.assertIsNone(local.Git(self.path).config().get('branch.eng/child.stack-parent'))

        self.assertEqual(
            captured.stderr.getvalue(),
            "'main' is not a development branch, a branch cannot be stacked on it\n",
        )

    def test_set_parent_self(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/child'), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), "'eng/child' cannot be stacked on itself\n")

    def test_set_parent_self_with_child(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))

            # 'eng/parent' already has 'eng/child' stacked on it, which must not be reported
            # in place of the branch being stacked on itself
            repo.head = repo.commits['eng/parent'][-1]
            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))

        self.assertEqual(
            captured.stderr.getvalue(),
            "'eng/parent' cannot be stacked on itself\n",
        )

    def test_set_parent_cycle(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            repo.head = repo.commits['eng/parent'][-1]
            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/child'), path=self.path))

        self.assertEqual(
            captured.stderr.getvalue(),
            "'eng/child' is stacked on 'eng/parent,' stacking 'eng/parent' on it would create a cycle\n",
        )

    def test_set_parent_refuses_a_cycle_above(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_branch(repo, 'eng/parent')
            self.add_branch(repo, 'eng/child', on='eng/parent')
            self.add_branch(repo, 'eng/other')
            repo.head = repo.commits['eng/parent'][-1]

            # Hand-written config where two branches are stacked on each other. The walk up
            # from 'eng/parent' finds the cycle before the walk down from it ever runs.
            repo.edit_config('branch.eng/child.stack-parent', 'eng/parent')
            repo.edit_config('branch.eng/parent.stack-parent', 'eng/child')

            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/other'), path=self.path))

        self.assertIn('is part of a cycle of stacked branches\n', captured.stderr.getvalue())

    def test_a_parent_git_refuses_to_forget_is_reported(self) -> None:
        real = stack_metadata.run

        def refuse(command: list, **kwargs: object) -> object:
            if '--unset' in command and 'stack-parent' in ' '.join(command):
                return ProcessCompletion(returncode=1)
            return real(command, **kwargs)

        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))

            # A branch whose parent was not forgotten is still stacked on it, whatever it was told
            with patch.object(stack_metadata, 'run', refuse):
                self.assertEqual(1, program.main(args=('stack', '--unstack'), path=self.path))

        self.assertIn(
            "Failed to forget which branch 'eng/child' is stacked on",
            captured.stderr.getvalue(),
        )

    def test_set_parent_branching(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            self.add_branch(repo, 'eng/sibling', on='eng/parent')
            repo.head = repo.commits['eng/sibling'][-1]

            # Two branches may be stacked on the same branch
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            self.assertEqual(0, program.main(args=('stack',), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/child' is stacked on 'eng/parent'\n"
            "'eng/sibling' is stacked on 'eng/parent'\n"
            'Stacked pull requests, bottom of the stack first:\n'
            '- eng/parent\n'
            '    - eng/child\n'
            '    - eng/sibling (this pull request)\n',
        )

    def test_unstack(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            self.assertEqual(0, program.main(args=('stack', '--unstack'), path=self.path))
            self.assertIsNone(local.Git(self.path).config().get('branch.eng/child.stack-parent'))

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/child' is stacked on 'eng/parent'\n"
            "'eng/child' is no longer stacked on another branch\n",
        )

    def test_parent_deleted(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            del repo.commits['eng/parent']
            self.assertEqual(0, program.main(args=('stack',), path=self.path))

        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/child' is stacked on 'eng/parent'\n"
            "'eng/child' is not part of a stack\n",
        )

    def test_listing(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            self.assertEqual(0, program.main(args=('stack',), path=self.path))

            # The whole stack is listed from either end, only the marked branch moves
            repo.head = repo.commits['eng/parent'][-1]
            self.assertEqual(0, program.main(args=('stack',), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/child' is stacked on 'eng/parent'\n"
            'Stacked pull requests, bottom of the stack first:\n'
            '- eng/parent\n'
            '    - eng/child (this pull request)\n'
            'Stacked pull requests, bottom of the stack first:\n'
            '- eng/parent (this pull request)\n'
            '    - eng/child\n',
        )

    def test_listing_not_stacked(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_branch(repo, 'eng/parent')
            repo.head = repo.commits['eng/parent'][-1]
            self.assertEqual(0, program.main(args=('stack',), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.stdout.getvalue(), "'eng/parent' is not part of a stack\n")

    def test_listing_nests_a_deeper_sibling(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            self.add_branch(repo, 'eng/grandchild', on='eng/child')
            self.add_branch(repo, 'eng/sibling', on='eng/parent')
            repo.edit_config('branch.eng/child.stack-parent', 'eng/parent')
            repo.edit_config('branch.eng/grandchild.stack-parent', 'eng/child')
            repo.edit_config('branch.eng/sibling.stack-parent', 'eng/parent')
            repo.head = repo.commits['eng/parent'][-1]

            self.assertEqual(0, program.main(args=('stack',), path=self.path))

        # 'eng/grandchild' is deeper than 'eng/sibling', so it has to follow the branch it is
        # stacked on rather than whichever branch was listed immediately before it
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            'Stacked pull requests, bottom of the stack first:\n'
            '- eng/parent (this pull request)\n'
            '    - eng/child\n'
            '        - eng/grandchild\n'
            '    - eng/sibling\n',
        )

    def test_listing_refuses_a_cycle_above(self) -> None:
        with self.checkout() as (repo, captured):
            self.add_stack(repo)
            # Hand-written config can stack two branches on each other
            repo.edit_config('branch.eng/child.stack-parent', 'eng/parent')
            repo.edit_config('branch.eng/parent.stack-parent', 'eng/child')

            self.assertEqual(1, program.main(args=('stack',), path=self.path))

        self.assertIn('is part of a cycle of stacked branches\n', captured.stderr.getvalue())
