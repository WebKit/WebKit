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

from redis import StrictRedis
from fakeredis import FakeStrictRedis
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class CommitContextTest(WaitForDockerTestCase):
    KEYSPACE = 'commit_mapping_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext):
        redis_instance = redis()

        self.stash_repository = MockStashRepository.safari(redis_instance)
        self.svn_repository = MockSVNRepository.webkit(redis_instance)

        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.database = CommitContext(
            redis=redis_instance,
            cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True),
        )
        self.database.register_repository(self.stash_repository)
        self.database.register_repository(self.svn_repository)

    def add_all_commits_to_database(self):
        for mock_repository in [self.stash_repository, self.svn_repository]:
            for commit_list in mock_repository.commits.values():
                for commit in commit_list:
                    self.database.register_commit(commit)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_verify_table(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        CommitContext(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_by_id(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        self.assertEqual(
            [self.stash_repository.commit_for_id(id='bb6bda5f44', branch='master')],
            self.database.find_commits_by_id(repository_id='safari', branch='master', commit_id='bb6bda5f44'),
        )
        self.assertEqual(2, len(self.database.find_commits_by_id(repository_id='safari', branch='master', commit_id='336610a')))

        self.assertEqual(
            [self.svn_repository.commit_for_id(id=236544, branch='trunk')],
            self.database.find_commits_by_id(repository_id='webkit', branch='trunk', commit_id=236544),
        )
        self.assertEqual(0, len(self.database.find_commits_by_id(repository_id='webkit', branch='trunk', commit_id='23654')))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_by_uuid(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        self.assertEqual(
            [self.stash_repository.commit_for_id(id='7be4084258', branch='master')],
            self.database.find_commits_by_uuid(repository_id='safari', branch='master', uuid=153755068501),
        )
        self.assertEqual(
            [self.svn_repository.commit_for_id(id=236540, branch='trunk')],
            self.database.find_commits_by_uuid(repository_id='webkit', branch='trunk', uuid=153802947900),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_by_timestamp(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        self.assertEqual(
            [self.stash_repository.commit_for_id(id='336610a84f', branch='master')],
            self.database.find_commits_by_timestamp(repository_id='safari', branch='master', timestamp=1537809818),
        )
        self.assertEqual(
            [self.svn_repository.commit_for_id(id=236540, branch='trunk')],
            self.database.find_commits_by_timestamp(repository_id='webkit', branch='trunk', timestamp=1538029479),
        )
        self.assertEqual(2, len(self.database.find_commits_by_timestamp(repository_id='safari', branch='master', timestamp=1537550685)))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_all_commits_stash(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()
        self.assertEqual(5, len(self.database.find_commits_in_range(repository_id='safari', branch='master')))
        self.assertEqual(
            [self.stash_repository.commit_for_id(id='bb6bda5f44', branch='master'), self.stash_repository.commit_for_id(id='336610a84f', branch='master')],
            self.database.find_commits_in_range(repository_id='safari', branch='master', limit=2),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_all_commits_svn(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()
        self.assertEqual(5, len(self.database.find_commits_in_range(repository_id='webkit', branch='trunk')))
        self.assertEqual(
            [self.svn_repository.commit_for_id(id=236544, branch='trunk'), self.svn_repository.commit_for_id(id=236543, branch='trunk')],
            self.database.find_commits_in_range(repository_id='webkit', branch='trunk', limit=2),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stash_commits_in_range(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()
        self.assertEqual(
            [self.stash_repository.commit_for_id(id='bb6bda5f44', branch='master'), self.stash_repository.commit_for_id(id='336610a84f', branch='master')],
            self.database.find_commits_in_range(repository_id='safari', branch='master', begin=1537809818, end=1537810281),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_svn_commits_in_range(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()
        self.assertEqual(
            [self.svn_repository.commit_for_id(id=236544, branch='trunk'), self.svn_repository.commit_for_id(id=236543, branch='trunk')],
            self.database.find_commits_in_range(repository_id='webkit', branch='trunk', begin=1538050458, end=1538052408),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stash_commits_between(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        commits = [
            self.stash_repository.commit_for_id(id='bb6bda5f', branch='master'),
            self.stash_repository.commit_for_id(id='336610a8', branch='master'),
            self.stash_repository.commit_for_id(id='336610a4', branch='master'),
        ]
        self.assertEqual(commits, self.database.find_commits_in_range(repository_id='safari', branch='master',  begin=commits[-1], end=commits[0]))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_svn_commits_between(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        commits = [
            self.svn_repository.commit_for_id(id=236544, branch='trunk'),
            self.svn_repository.commit_for_id(id=236543, branch='trunk'),
            self.svn_repository.commit_for_id(id=236542, branch='trunk'),
        ]
        self.assertEqual(commits, self.database.find_commits_in_range(repository_id='webkit', branch='trunk', begin=commits[-1], end=commits[0]))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_from_stash_repo(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.database.register_commit_with_repo_and_id('safari', 'master', 'bb6bda5f44')
        self.assertEqual(
            [self.stash_repository.commit_for_id(id='bb6bda5f44', branch='master')],
            self.database.find_commits_by_id(repository_id='safari', branch='master', commit_id='bb6bda5f44'),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_from_svn_repo(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.database.register_commit_with_repo_and_id('webkit', 'trunk', 236544)
        self.assertEqual(
            [self.svn_repository.commit_for_id(id=236544, branch='trunk')],
            self.database.find_commits_by_id(repository_id='webkit', branch='trunk', commit_id=236544),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_branches(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()
        self.assertEqual(['master', 'safari-606-branch'], self.database.branches(repository_id='safari'))
        self.assertEqual(['safari-606-branch', 'trunk'], self.database.branches(repository_id='webkit'))
        self.assertEqual(['safari-606-branch'], self.database.branches(repository_id='safari', branch='safari'))
        self.assertEqual(['safari-606-branch'], self.database.branches(repository_id='webkit', branch='safari-606-branch'))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_next_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        self.assertEqual(
            self.database.next_commit(self.svn_repository.commit_for_id(id=236542)),
            self.svn_repository.commit_for_id(id=236543),
        )
        self.assertEqual(
            self.database.next_commit(self.stash_repository.commit_for_id(id='336610a40c3fecb728871e12ca31482ca715b383')),
            self.stash_repository.commit_for_id(id='336610a84fdcf14ddcf1db65075af95480516fda'),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_previous_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        self.assertEqual(
            self.svn_repository.commit_for_id(id=236542),
            self.database.previous_commit(self.svn_repository.commit_for_id(id=236543)),
        )
        self.assertEqual(
            self.stash_repository.commit_for_id(id='336610a40c3fecb728871e12ca31482ca715b383'),
            self.database.previous_commit(self.stash_repository.commit_for_id(id='336610a84fdcf14ddcf1db65075af95480516fda')),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_sibling_commits(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.add_all_commits_to_database()

        self.assertEqual(
            self.database.sibling_commits(self.svn_repository.commit_for_id(id=236542), ['safari']),
            {'safari': [self.stash_repository.commit_for_id(id='bb6bda5f44dd24d0b54539b8ff6e8c17f519249a')]},
        )
        self.assertEqual(
            self.database.sibling_commits(self.stash_repository.commit_for_id(id='bb6bda5f44dd24d0b54539b8ff6e8c17f519249a'), ['webkit']),
            {'webkit': [
                self.svn_repository.commit_for_id(id=236544),
                self.svn_repository.commit_for_id(id=236543),
                self.svn_repository.commit_for_id(id=236542),
                self.svn_repository.commit_for_id(id=236541),
                self.svn_repository.commit_for_id(id=236540),
            ]},
        )
        self.assertEqual(
            self.database.sibling_commits(self.stash_repository.commit_for_id(id='336610a84fdcf14ddcf1db65075af95480516fda'), ['webkit']),
            {'webkit': []},
        )

    def test_uuid_for_commits(self):
        uuid = CommitContext.uuid_for_commits([MockStashRepository.safari().commit_for_id(id='bb6bda5f'), MockSVNRepository.webkit().commit_for_id(id=236544)])
        self.assertEqual(uuid, 153805240800)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_branch_keys_for_commits(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        branches = self.database.branch_keys_for_commits([
            MockStashRepository.safari().commit_for_id(id='bb6bda5f'),
            MockSVNRepository.webkit().commit_for_id(id=236544),
        ])
        self.assertEqual(branches, ['default'])

        branches = self.database.branch_keys_for_commits([
            MockStashRepository.safari().commit_for_id(id='79256c32', branch='safari-606-branch'),
            MockSVNRepository.webkit().commit_for_id(id=236544),
        ])
        self.assertEqual(branches, ['safari-606-branch'])

        branches = self.database.branch_keys_for_commits([
            MockStashRepository.safari().commit_for_id(id='79256c32', branch='safari-606-branch'),
            MockSVNRepository.webkit().commit_for_id(id=236335, branch='safari-606-branch'),
        ])
        self.assertEqual(branches, ['safari-606-branch'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_url(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.assertEqual(
            'https://fake-stash-instance.apple.com/projects/BROWSER/repos/safari/commits/bb6bda5f44dd24d0b54539b8ff6e8c17f519249a',
            self.database.url(MockStashRepository.safari().commit_for_id(id='bb6bda5f')),
        )
        self.assertEqual(
            'https://trac.webkit.org/changeset/236544/webkit',
            self.database.url(MockSVNRepository.webkit().commit_for_id(id=236544)),
        )
