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

from webkitcorepy import testing

from webkitscmpy import local, mocks
from webkitscmpy.program.stack_metadata import StackMetadata


class TestStackMetadata(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self) -> None:
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_key_for(self) -> None:
        self.assertEqual(StackMetadata.key_for('eng/child', 'parent'), 'branch.eng/child.stack-parent')

    def test_a_branch_which_is_not_stacked_records_nothing(self) -> None:
        with mocks.local.Git(self.path):
            metadata = StackMetadata(local.Git(self.path), 'eng/child')
            self.assertEqual(metadata.recorded, {})
            self.assertIsNone(metadata.parent)

    def test_write_records_a_parent(self) -> None:
        with mocks.local.Git(self.path):
            metadata = StackMetadata(local.Git(self.path), 'eng/child')
            self.assertEqual(0, metadata.write({'parent': 'eng/parent'}))
            self.assertEqual(metadata.recorded, {'parent': 'eng/parent'})
            self.assertEqual(metadata.parent, 'eng/parent')

    def test_a_recorded_parent_outlives_the_object_which_wrote_it(self) -> None:
        with mocks.local.Git(self.path):
            self.assertEqual(0, StackMetadata(local.Git(self.path), 'eng/child').write({'parent': 'eng/parent'}))
            self.assertEqual(
                local.Git(self.path).config().get('branch.eng/child.stack-parent'),
                'eng/parent',
            )

    def test_a_branch_recorded_on_itself_has_no_parent(self) -> None:
        with mocks.local.Git(self.path):
            metadata = StackMetadata(local.Git(self.path), 'eng/child')
            self.assertEqual(0, metadata.write({'parent': 'eng/child'}))
            self.assertEqual(metadata.recorded, {'parent': 'eng/child'})
            self.assertIsNone(metadata.parent)

    def test_clear_forgets_the_parent(self) -> None:
        with mocks.local.Git(self.path):
            metadata = StackMetadata(local.Git(self.path), 'eng/child')
            self.assertEqual(0, metadata.write({'parent': 'eng/parent'}))
            self.assertEqual(0, metadata.clear())
            self.assertEqual(metadata.recorded, {})

    def test_stacked_names_every_branch_with_a_recorded_parent(self) -> None:
        with mocks.local.Git(self.path):
            git = local.Git(self.path)
            self.assertEqual(0, StackMetadata(git, 'eng/child').write({'parent': 'eng/parent'}))
            self.assertEqual(0, StackMetadata(git, 'eng/grandchild').write({'parent': 'eng/child'}))
            self.assertEqual(
                dict(StackMetadata.stacked(git)),
                {'eng/child': 'eng/parent', 'eng/grandchild': 'eng/child'},
            )

    def test_stacked_ignores_branch_config_which_is_not_a_stack(self) -> None:
        with mocks.local.Git(self.path) as repo:
            repo.edit_config('branch.eng/child.merge', 'refs/heads/main')
            git = local.Git(self.path)
            self.assertEqual(0, StackMetadata(git, 'eng/other').write({'parent': 'eng/parent'}))
            self.assertEqual(dict(StackMetadata.stacked(git)), {'eng/other': 'eng/parent'})

    def test_a_detached_head_cannot_carry_a_stack(self) -> None:
        with mocks.local.Git(self.path):
            self.assertIsNone(StackMetadata.for_branch(local.Git(self.path), None))

    def test_a_checkout_which_is_not_git_cannot_carry_a_stack(self) -> None:
        with mocks.local.Svn(self.path):
            self.assertIsNone(StackMetadata.for_branch(local.Svn(self.path), 'eng/child'))
