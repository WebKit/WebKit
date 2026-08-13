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
from unittest.mock import patch

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Time as MockTime

from webkitscmpy import Commit, local, mocks, program

CONTRIBUTOR = {'name': 'Tim Contributor', 'emails': ['tcontributor@example.com']}


class TestStack(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    @classmethod
    def add_parent(cls, repo):
        repo.commits['eng/parent'] = [
            repo.commits[repo.default_branch][-1],
            Commit(
                hash='06de5d56554e693db72313f4ca1fb969c30b8ccb',
                branch='eng/parent',
                author=CONTRIBUTOR,
                identifier='5.1@eng/parent',
                timestamp=1601668000,
                message='[Testing] Parent change\n',
            ),
        ]
        return repo

    @classmethod
    def add_stack(cls, repo):
        cls.add_parent(repo)
        repo.commits['eng/child'] = [
            repo.commits['eng/parent'][-1],
            Commit(
                hash='b8b921baaad2fd10bc9d0cc9e97f8fa1d6e5f4a1',
                branch='eng/child',
                author=CONTRIBUTOR,
                identifier='5.2@eng/child',
                timestamp=1601669000,
                message='[Testing] Child change\n',
            ),
        ]
        repo.head = repo.commits['eng/child'][-1]
        return repo

    def test_set_parent(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            # Recording the same parent again is not an error
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            self.assertEqual(
                local.Git(self.path).config().get('branch.eng/child.stack-parent'),
                'eng/parent',
            )

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/child' is stacked on 'eng/parent'\n" * 2,
        )

    def test_set_parent_missing(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/missing'), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), "'eng/missing' does not exist in this checkout\n")

    def test_set_parent_production(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            self.assertEqual(1, program.main(args=('stack', '--on', 'main'), path=self.path))
            self.assertIsNone(local.Git(self.path).config().get('branch.eng/child.stack-parent'))

        self.assertEqual(
            captured.stderr.getvalue(),
            "'main' is not a development branch, a branch cannot be stacked on it\n",
        )

    def test_set_parent_self(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/child'), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), "'eng/child' cannot be stacked on itself\n")

    def test_set_parent_self_with_child(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
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

    def test_set_parent_cycle(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            repo.head = repo.commits['eng/parent'][-1]
            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/child'), path=self.path))

        self.assertEqual(
            captured.stderr.getvalue(),
            "'eng/child' is stacked on 'eng/parent,' stacking 'eng/parent' on it would create a cycle\n",
        )

    def test_set_parent_refuses_a_cycle_above(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_parent(repo)
            repo.commits['eng/child'] = [
                repo.commits['eng/parent'][-1],
                Commit(
                    hash='b8b921baaad2fd10bc9d0cc9e97f8fa1d6e5f4a1',
                    branch='eng/child',
                    author=CONTRIBUTOR,
                    identifier='5.2@eng/child',
                    timestamp=1601669000,
                    message='[Testing] Child change\n',
                ),
            ]
            repo.commits['eng/other'] = [
                repo.commits[repo.default_branch][-1],
                Commit(
                    hash='9d1a3f6c2b8e4d5a7c0f1e2b3a4d5c6e7f809123',
                    branch='eng/other',
                    author=CONTRIBUTOR,
                    identifier='5.1@eng/other',
                    timestamp=1601669500,
                    message='[Testing] Other change\n',
                ),
            ]
            repo.head = repo.commits['eng/parent'][-1]

            # Hand-written config where two branches are stacked on each other. The walk up
            # from 'eng/parent' finds the cycle before the walk down from it ever runs.
            repo.edit_config('branch.eng/child.stack-parent', 'eng/parent')
            repo.edit_config('branch.eng/parent.stack-parent', 'eng/child')

            self.assertEqual(1, program.main(args=('stack', '--on', 'eng/other'), path=self.path))

        self.assertIn('is part of a cycle of stacked branches\n', captured.stderr.getvalue())

    def test_set_parent_branching(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            repo.commits['eng/sibling'] = [
                repo.commits['eng/parent'][-1],
                Commit(
                    hash='2f0c1cbb7e5b6f2a3ba0a4c1e0f79c1c7d4a3b12',
                    branch='eng/sibling',
                    author=CONTRIBUTOR,
                    identifier='5.2@eng/sibling',
                    timestamp=1601669500,
                    message='[Testing] Sibling change\n',
                ),
            ]
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

    def test_unstack(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
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

    def test_parent_deleted(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            self.assertEqual(0, program.main(args=('stack', '--on', 'eng/parent'), path=self.path))
            del repo.commits['eng/parent']
            self.assertEqual(0, program.main(args=('stack',), path=self.path))

        self.assertEqual(
            captured.stdout.getvalue(),
            "'eng/child' is stacked on 'eng/parent'\n"
            "'eng/child' is not part of a stack\n",
        )

    def test_listing(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
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

    def test_listing_not_stacked(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_parent(repo)
            repo.head = repo.commits['eng/parent'][-1]
            self.assertEqual(0, program.main(args=('stack',), path=self.path))

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.stdout.getvalue(), "'eng/parent' is not part of a stack\n")

    def test_listing_nests_a_deeper_sibling(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            repo.commits['eng/grandchild'] = [
                repo.commits['eng/child'][-1],
                Commit(
                    hash='7c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f60718293',
                    branch='eng/grandchild',
                    author=CONTRIBUTOR,
                    identifier='5.3@eng/grandchild',
                    timestamp=1601670000,
                    message='[Testing] Grandchild change\n',
                ),
            ]
            repo.commits['eng/sibling'] = [
                repo.commits['eng/parent'][-1],
                Commit(
                    hash='2f0c1cbb7e5b6f2a3ba0a4c1e0f79c1c7d4a3b12',
                    branch='eng/sibling',
                    author=CONTRIBUTOR,
                    identifier='5.2@eng/sibling',
                    timestamp=1601669500,
                    message='[Testing] Sibling change\n',
                ),
            ]
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

    def test_listing_refuses_a_cycle_above(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), \
                patch('webkitbugspy.Tracker._trackers', []), MockTime:
            self.add_stack(repo)
            # Hand-written config can stack two branches on each other
            repo.edit_config('branch.eng/child.stack-parent', 'eng/parent')
            repo.edit_config('branch.eng/parent.stack-parent', 'eng/child')

            self.assertEqual(1, program.main(args=('stack',), path=self.path))

        self.assertIn('is part of a cycle of stacked branches\n', captured.stderr.getvalue())
