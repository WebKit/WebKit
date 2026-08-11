# Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.repository import StashRepository, WebKitRepository
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class RepositoryTest(WaitForDockerTestCase):

    def test_svn(self):
        with MockModelFactory.webkit():
            svn_repo = WebKitRepository()
            self.assertTrue('webkit', svn_repo.key)
            commit = svn_repo.commit(revision=6)
            self.assertEqual(commit.uuid, 160163990000)
            self.assertEqual(commit.message, '6th commit')
            self.assertEqual(commit.branch, 'main')

    def test_ref_svn(self):
        with MockModelFactory.webkit():
            svn_repo = WebKitRepository()
            commit = svn_repo.commit(ref=7)
            self.assertEqual(commit.uuid, 160164090000)
            self.assertEqual(commit.message, '7th commit')
            self.assertEqual(commit.branch, 'branch-a')

    def test_stash(self):
        with MockModelFactory.safari() as safari:
            git_repo = StashRepository('https://{}'.format(safari.remote))
            self.assertEqual('safari', git_repo.key)
            commit = git_repo.commit(hash='1abe25b443e9')
            self.assertEqual(commit.uuid, 160166300000)
            self.assertEqual(commit.branch, 'main')

    def test_ref_stash(self):
        with MockModelFactory.safari() as safari:
            git_repo = StashRepository('https://{}'.format(safari.remote))
            self.assertEqual('safari', git_repo.key)
            commit = git_repo.commit(ref='1abe25b443e9')
            self.assertEqual(commit.uuid, 160166300000)
            self.assertEqual(commit.branch, 'main')

    def test_colliding_timestamps_stash(self):
        with MockModelFactory.safari() as safari:
            git_repo = StashRepository('https://{}'.format(safari.remote))
            commit1 = git_repo.commit(hash='bae5d1e90999')
            commit2 = git_repo.commit(hash='d8bce26fa65c')

            self.assertEqual(commit1.timestamp, commit2.timestamp)
            self.assertNotEqual(commit1.uuid, commit2.uuid)
            self.assertTrue(commit1 < commit2)
            self.assertEqual(commit1.order, 0)
            self.assertEqual(commit2.order, 1)

    def test_branch_stash(self):
        with MockModelFactory.safari() as safari:
            git_repo = StashRepository('https://{}'.format(safari.remote))
            commit = git_repo.commit(hash='621652add7fc')
            self.assertEqual(commit.uuid, 160166600000)
            self.assertEqual(commit.branch, 'branch-a')
