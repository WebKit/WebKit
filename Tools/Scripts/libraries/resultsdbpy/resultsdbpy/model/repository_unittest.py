# Copyright (C) 2019 Apple Inc. All rights reserved.
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

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.commit import Commit
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.repository import SCMException
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class RepositoryTest(WaitForDockerTestCase):

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_svn(self, redis=StrictRedis):
        svn_repo = MockSVNRepository.webkit(redis=redis())
        self.assertTrue('webkit', svn_repo.key)
        commit = svn_repo.commit_for_id(236544, branch='trunk')
        self.assertEqual(commit.uuid, 153805240800)
        self.assertEqual(commit.message, 'Change 1 description.\n')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_branch_svn(self, redis=StrictRedis):
        svn_repo = MockSVNRepository.webkit(redis=redis())
        with self.assertRaises(SCMException):
            svn_repo.commit_for_id(236335, branch='trunk')
        commit = svn_repo.commit_for_id(236335, branch='safari-606-branch')
        self.assertEqual(commit.uuid, 153802948000)
        self.assertEqual(commit.message, 'Integration 1.\n')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_basic_svn(self, redis=StrictRedis):
        svn_repo = MockSVNRepository(url='svn.webkit.org/repository/webkit/', name='webkit', redis=redis())
        svn_repo.add_commit(Commit(
            repository_id=svn_repo.name, branch='trunk', id=236544,
            timestamp=1538052408, order=0,
            committer='person1@webkit.org',
            message='Change 1 description.',
        ))
        commit = svn_repo.commit_for_id(236544, branch='trunk')
        self.assertEqual(commit.uuid, 153805240800)
        self.assertIsNone(commit.message, None)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_stash(self, redis=StrictRedis):
        git_repo = MockStashRepository.safari(redis=redis())
        self.assertEqual('safari', git_repo.key)
        commit = git_repo.commit_for_id('bb6bda5f44dd24d', branch='master')
        self.assertEqual(commit.uuid, 153781028100)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_colliding_timestamps_stash(self, redis=StrictRedis):
        git_repo = MockStashRepository.safari(redis=redis())
        commit1 = git_repo.commit_for_id('e64810a40', branch='master')
        commit2 = git_repo.commit_for_id('7be408425', branch='master')

        self.assertEqual(commit1.timestamp, commit2.timestamp)
        self.assertNotEqual(commit1.uuid, commit2.uuid)
        self.assertTrue(commit1 < commit2)
        self.assertEqual(commit1.order, 0)
        self.assertEqual(commit2.order, 1)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_branch_stash(self, redis=StrictRedis):
        git_repo = MockStashRepository.safari(redis=redis())
        with self.assertRaises(SCMException):
            git_repo.commit_for_id('d85222d9407fd', branch='master')
        commit = git_repo.commit_for_id('d85222d9407fd', branch='safari-606-branch')
        self.assertEqual(commit.uuid, 153756338300)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_stash_already_cached(self, redis=StrictRedis):
        redis_instance = redis()
        git_repo = MockStashRepository.safari(redis=redis_instance)
        redis_instance.set(
            'repository:' + git_repo.url + '/commits?since=336610a40c3fecb728871e12ca31482ca715b383&until=refs%2Fheads%2Fmaster&limit=1',
            json.dumps(dict(values=[], size=0, isLastPage=True)),
        )

        self.assertEqual('safari', git_repo.key)
        commit = git_repo.commit_for_id('336610a4', branch='master')
        self.assertEqual(commit.uuid, 153756638600)
