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

from redis import StrictRedis
from fakeredis import FakeStrictRedis
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from resultsdbpy.model.repository import StashRepository, WebKitRepository


class CommitContextTest(WaitForDockerTestCase):
    KEYSPACE = 'commit_mapping_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext):
        redis_instance = redis()

        self.stash_repository = StashRepository('https://bitbucket.example.com/projects/SAFARI/repos/safari')
        self.svn_repository = WebKitRepository()

        cassandra.drop_keyspace(keyspace=self.KEYSPACE)
        self.database = CommitContext(
            redis=redis_instance,
            cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True),
        )
        self.database.register_repository(self.stash_repository)
        self.database.register_repository(self.svn_repository)

    def add_all_commits_to_database(self):
        with MockModelFactory.safari() as safari, MockModelFactory.webkit() as webkit:
            with self.database, self.database.cassandra.batch_query_context():
                for key, repository in dict(safari=safari, webkit=webkit).items():
                    for branch, commits in repository.commits.items():
                        for commit in commits:
                            self.database.register_partial_commit(
                                key, hash=commit.hash, revision=commit.revision, fast=False,
                            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_verify_table(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            CommitContext(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_by_id(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            self.assertEqual(
                [self.stash_repository.commit(ref='d8bce26fa65c')],
                self.database.find_commits_by_ref(repository_id='safari', ref='d8bce26fa65c'),
            )

            self.assertEqual(
                [self.svn_repository.commit(ref=6)],
                self.database.find_commits_by_ref(repository_id='webkit', ref=6),
            )
            self.assertEqual(0, len(self.database.find_commits_by_ref(repository_id='webkit', ref='1234')))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_by_uuid(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            self.assertEqual(
                [self.stash_repository.commit(ref='1abe25b443e9')],
                self.database.find_commits_by_uuid(repository_id='safari', branch='main', uuid=160166300000),
            )
            self.assertEqual(
                [self.svn_repository.commit(ref=6)],
                self.database.find_commits_by_uuid(repository_id='webkit', branch='main', uuid=160163990000),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_by_timestamp(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            self.assertEqual(
                [self.stash_repository.commit(ref='1abe25b443e9')],
                self.database.find_commits_by_timestamp(repository_id='safari', branch='main', timestamp=1601663000),
            )
            self.assertEqual(
                [self.svn_repository.commit(ref=6)],
                self.database.find_commits_by_timestamp(repository_id='webkit', branch='main', timestamp=1601639900),
            )
            self.assertEqual(2, len(self.database.find_commits_by_timestamp(repository_id='safari', branch='main', timestamp=1601668000)))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_all_commits_stash(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()
            self.assertEqual(5, len(self.database.find_commits_in_range(repository_id='safari', branch='main')))
            self.assertEqual(
                [self.stash_repository.commit(ref='d8bce26fa65c'), self.stash_repository.commit(ref='bae5d1e90999')],
                self.database.find_commits_in_range(repository_id='safari', branch='main', limit=2),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_all_commits_svn(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()
            self.assertEqual(4, len(self.database.find_commits_in_range(repository_id='webkit', branch='main')))
            self.assertEqual(
                [self.svn_repository.commit(ref=6), self.svn_repository.commit(ref=4)],
                self.database.find_commits_in_range(repository_id='webkit', branch='main', limit=2),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stash_commits_in_range(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()
            self.assertEqual(
                [self.stash_repository.commit(ref='bae5d1e90999'), self.stash_repository.commit(ref='1abe25b443e9')],
                self.database.find_commits_in_range(repository_id='safari', branch='main', begin=1601663000, end=1601668000),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_svn_commits_in_range(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()
            self.assertEqual(
                [self.svn_repository.commit(ref=6), self.svn_repository.commit(ref=4)],
                self.database.find_commits_in_range(repository_id='webkit', branch='main', begin=1601637900, end=1601639900),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stash_commits_between(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            commits = [
                self.stash_repository.commit(ref='1abe25b443e9'),
                self.stash_repository.commit(ref='fff83bb2d917'),
                self.stash_repository.commit(ref='9b8311f25a77'),
            ]
            self.assertEqual(commits, self.database.find_commits_in_range(repository_id='safari', branch='main',  begin=commits[-1], end=commits[0]))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_svn_commits_between(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            commits = [
                self.svn_repository.commit(ref=6),
                self.svn_repository.commit(ref=4),
                self.svn_repository.commit(ref=2),
            ]
            self.assertEqual(commits, self.database.find_commits_in_range(repository_id='webkit', branch='main', begin=commits[-1], end=commits[0]))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stash_commits_before(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            commits = [
                self.stash_repository.commit(ref='1abe25b443e9'),
                self.stash_repository.commit(ref='fff83bb2d917'),
                self.stash_repository.commit(ref='9b8311f25a77'),
            ]
            self.assertEqual(commits, self.database.find_commits_in_range(repository_id='safari', branch='main', end=commits[0], limit=3))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_svn_commits_before(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            commits = [
                self.svn_repository.commit(ref=6),
                self.svn_repository.commit(ref=4),
                self.svn_repository.commit(ref=2),
            ]
            self.assertEqual(commits, self.database.find_commits_in_range(repository_id='webkit', branch='main', end=commits[0], limit=3))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_stash_commits_after(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            commits = [
                self.stash_repository.commit(ref='1abe25b443e9'),
                self.stash_repository.commit(ref='fff83bb2d917'),
                self.stash_repository.commit(ref='9b8311f25a77'),
            ]
            self.assertEqual(commits, self.database.find_commits_in_range(repository_id='safari', branch='main', begin=commits[-1], limit=3))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_svn_commits_after(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            commits = [
                self.svn_repository.commit(ref=6),
                self.svn_repository.commit(ref=4),
                self.svn_repository.commit(ref=2),
            ]
            self.assertEqual(commits, self.database.find_commits_in_range(repository_id='webkit', branch='main', begin=commits[-1], limit=3))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_from_stash_repo(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.database.register_partial_commit('safari', hash='d8bce26fa65c', fast=False)
            self.assertEqual(
                [self.stash_repository.commit(ref='d8bce26fa65c')],
                self.database.find_commits_by_ref(repository_id='safari', ref='d8bce26fa65c'),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_from_svn_repo(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.database.register_partial_commit('webkit', revision=6, fast=False)
            self.assertEqual(
                [self.svn_repository.commit(ref=6)],
                self.database.find_commits_by_ref(repository_id='webkit', ref=6),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_branches(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()
            self.assertEqual(['branch-a', 'branch-b', 'eng/squash-branch', 'main'], self.database.branches(repository_id='safari'))
            self.assertEqual(
                ['branch-a', 'branch-b', 'main'],
                self.database.branches(repository_id='webkit'),
            )
            self.assertEqual(['branch-a', 'branch-b'], self.database.branches(repository_id='safari', branch='branch'))
            self.assertEqual(['branch-a'], self.database.branches(repository_id='webkit', branch='branch-a'))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_next_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            self.assertEqual(
                self.database.next_commit(self.svn_repository.commit(ref=4)),
                self.svn_repository.commit(ref=6),
            )
            self.assertEqual(
                self.database.next_commit(self.stash_repository.commit(ref='bae5d1e90999')),
                self.stash_repository.commit(ref='d8bce26fa65c'),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_previous_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            self.assertEqual(
                self.svn_repository.commit(ref=4),
                self.database.previous_commit(self.svn_repository.commit(ref=6)),
            )
            self.assertEqual(
                self.stash_repository.commit(ref='bae5d1e90999'),
                self.database.previous_commit(self.stash_repository.commit(ref='d8bce26fa65c')),
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_sibling_commits(self, redis=StrictRedis, cassandra=CassandraContext):
        self.maxDiff = None
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.add_all_commits_to_database()

            self.assertEqual(
                self.database.sibling_commits(self.svn_repository.commit(ref=6), ['safari']),
                {'safari': [
                    self.stash_repository.commit(ref='d8bce26fa65c'),
                    self.stash_repository.commit(ref='bae5d1e90999'),
                    self.stash_repository.commit(ref='1abe25b443e9'),
                    self.stash_repository.commit(ref='fff83bb2d917'),
                    self.stash_repository.commit(ref='9b8311f25a77'),
                ]},
            )
            self.assertEqual(
                self.database.sibling_commits(self.stash_repository.commit(ref='d8bce26fa65c'), ['webkit']),
                {'webkit': [self.svn_repository.commit(ref=6)]},
            )
            self.assertEqual(
                self.database.sibling_commits(self.svn_repository.commit(ref=1), ['safari']),
                {'safari': []},
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_uuid_for_commits(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            uuid = CommitContext.uuid_for_commits([
                self.stash_repository.commit(ref='bae5d1e90999'),
                self.svn_repository.commit(ref=6),
            ])
            self.assertEqual(uuid, 160166800000)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_branch_keys_for_commits(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            branches = self.database.branch_keys_for_commits([
                self.stash_repository.commit(ref='d8bce26fa65c'),
                self.svn_repository.commit(ref=6),
            ])
            self.assertEqual(branches, ['default'])

            branches = self.database.branch_keys_for_commits([
                self.stash_repository.commit(ref='621652add7fc'),
                self.svn_repository.commit(ref=7),
            ])
            self.assertEqual(branches, ['branch-a'])

            branches = self.database.branch_keys_for_commits([
                self.stash_repository.commit(ref='790725a6d79e'),
                self.svn_repository.commit(ref=8),
            ])
            self.assertEqual(branches, ['branch-b'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_commit_url(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            self.init_database(redis=redis, cassandra=cassandra)
            self.assertEqual(
                'https://bitbucket.example.com/projects/SAFARI/repos/safari/commits/d8bce26fa65c6fc8f39c17927abb77f69fab82fc',
                self.database.url(self.stash_repository.commit(ref='d8bce26fa65c')),
            )
            self.assertEqual(
                'https://commits.webkit.org/4@main',
                self.database.url(self.svn_repository.commit(ref=6)),
            )
