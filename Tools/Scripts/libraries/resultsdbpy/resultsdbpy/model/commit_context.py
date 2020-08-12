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

import calendar
import json
import time

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model
from datetime import datetime
from resultsdbpy.controller.commit import Commit
from resultsdbpy.model.repository import Repository, SCMException


class CommitContext(object):

    class CommitModel(Model):
        repository_id = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        committer = columns.Text()
        message = columns.Text()

        def to_commit(self):
            return Commit(
                repository_id=self.repository_id, branch=self.branch,
                id=self.commit_id,
                timestamp=self.uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER,
                order=self.uuid % Commit.TIMESTAMP_TO_UUID_MULTIPLIER,
                committer=self.committer, message=self.message,
            )

    class CommitByID(CommitModel):
        __table_name__ = 'commits_id_to_timestamp_uuid'
        commit_id = columns.Text(primary_key=True, required=True)
        uuid = columns.BigInt(required=True)

    class CommitByUuidAscending(CommitModel):
        __table_name__ = 'commits_timestamp_uuid_to_id_ascending'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='ASC')
        commit_id = columns.Text(required=True)

    class CommitByUuidDescending(CommitModel):
        __table_name__ = 'commits_timestamp_uuid_to_id_descending'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        commit_id = columns.Text(required=True)

    class Branches(Model):
        __table_name__ = 'commit_branches'
        repository_id = columns.Text(partition_key=True, required=True)
        branch = columns.Text(primary_key=True, required=True)

    DEFAULT_BRANCH_KEY = 'default'

    def __init__(self, redis, cassandra, cache_timeout=60 * 60 * 24 * 2):
        assert redis
        assert cassandra

        self.redis = redis
        self.cassandra = cassandra
        self.repositories = {}
        self.cache_timeout = cache_timeout

        with self:
            self.cassandra.create_table(self.CommitByID)
            self.cassandra.create_table(self.CommitByUuidAscending)
            self.cassandra.create_table(self.CommitByUuidDescending)
            self.cassandra.create_table(self.Branches)

    def __enter__(self):
        self.cassandra.__enter__()

    def __exit__(self, *args, **kwargs):
        self.cassandra.__exit__(*args, **kwargs)

    @classmethod
    def timestamp_to_uuid(cls, timestamp=None):
        if timestamp is None:
            return int(time.time()) * Commit.TIMESTAMP_TO_UUID_MULTIPLIER
        elif isinstance(timestamp, datetime):
            return calendar.timegm(timestamp.timetuple()) * Commit.TIMESTAMP_TO_UUID_MULTIPLIER
        return int(timestamp) * Commit.TIMESTAMP_TO_UUID_MULTIPLIER

    @classmethod
    def convert_to_uuid(cls, value, default=0):
        if value is None:
            return default
        elif isinstance(value, Commit):
            return value.uuid
        else:
            return cls.timestamp_to_uuid(value)

    @classmethod
    def uuid_for_commits(cls, commits):
        return max([commit.uuid for commit in commits])

    def branch_keys_for_commits(self, commits):
        branches = set()

        # This covers a mostly theoretical edge case where a multiple commits are provided on different branches. In
        # this case, track branches which are not the default branch independently.
        for commit in commits:
            repo = self.repositories.get(commit.repository_id, Repository(key=commit.repository_id))
            if commit.branch != repo.DEFAULT_BRANCH:
                branches.add(commit.branch)
        if len(branches) == 0:
            branches.add(self.DEFAULT_BRANCH_KEY)
        return list(branches)

    def run_function_through_redis_cache(self, key, function):
        result = self.redis.get('commit_mapping:' + key)
        if result:
            try:
                result = [Commit.from_json(element) for element in json.loads(result)]
            except ValueError:
                result = None
        if result is None:
            result = function()
        if result:
            self.redis.set('commit_mapping:' + key, json.dumps(result, cls=Commit.Encoder), ex=self.cache_timeout)
        return result

    def register_repository(self, repository):
        if not isinstance(repository, Repository):
            raise TypeError(f'Expected type {Repository}, got {type(repository)}')
        self.repositories[repository.key] = repository

    def find_commits_by_id(self, repository_id, branch, commit_id, limit=100):
        if branch is None:
            branch = self.repositories[repository_id].DEFAULT_BRANCH

        def callback(commit_id=commit_id):
            with self:
                if isinstance(commit_id, int) or commit_id.isdigit():
                    return [model.to_commit() for model in self.cassandra.select_from_table(
                        self.CommitByID.__table_name__, limit=limit,
                        repository_id=repository_id, branch=branch, commit_id=str(commit_id),
                    )]

                # FIXME: SASI indecies are the canoical way to solve this problem, but require Cassandra 3.4 which
                # hasn't been deployed to our datacenters yet. This works for commits, but is less transparent.
                return [model.to_commit() for model in self.cassandra.select_from_table(
                    self.CommitByID.__table_name__, limit=limit,
                    repository_id=repository_id, branch=branch, commit_id__gte=commit_id.lower(), commit_id__lte=(commit_id.lower() + 'g'),
                )]

        return self.run_function_through_redis_cache(
            f'repository_id={repository_id}:branch={branch}:commit_id={str(commit_id).lower()}',
            callback,
        )

    def find_commits_by_uuid(self, repository_id, branch, uuid, limit=100):
        if branch is None:
            branch = self.repositories[repository_id].DEFAULT_BRANCH

        def callback():
            with self:
                return [model.to_commit() for model in self.cassandra.select_from_table(
                    self.CommitByUuidDescending.__table_name__, limit=limit,
                    repository_id=repository_id, branch=branch, uuid=uuid,
                )]

        return self.run_function_through_redis_cache(
            f'repository_id={repository_id}:branch={branch}:uuid={uuid}',
            callback,
        )

    def find_commits_by_timestamp(self, repository_id, branch, timestamp, limit=100):
        if branch is None:
            branch = self.repositories[repository_id].DEFAULT_BRANCH

        if isinstance(timestamp, datetime):
            timestamp = calendar.timegm(timestamp.timetuple())
        else:
            timestamp = int(timestamp)

        def callback():
            with self.cassandra:
                return [model.to_commit() for model in self.cassandra.select_from_table(
                    self.CommitByUuidDescending.__table_name__, limit=limit,
                    repository_id=repository_id, branch=branch,
                    uuid__gte=self.timestamp_to_uuid(timestamp),
                    uuid__lt=self.timestamp_to_uuid(timestamp + 1),
                )]

        return self.run_function_through_redis_cache(
            f'repository_id={repository_id}:branch={branch}:timestamp={timestamp}',
            callback,
        )

    def find_commits_in_range(self, repository_id, branch=None, begin=None, end=None, limit=100):
        if branch is None:
            branch = self.repositories[repository_id].DEFAULT_BRANCH

        begin = self.convert_to_uuid(begin)
        end = self.convert_to_uuid(end, self.timestamp_to_uuid())

        with self:
            return [model.to_commit() for model in self.cassandra.select_from_table(
                self.CommitByUuidDescending.__table_name__, limit=limit,
                repository_id=repository_id, branch=branch,
                uuid__gte=begin,
                uuid__lte=end,
            )]

    def _adjacent_commit(self, commit, ascending=True):
        if not isinstance(commit, Commit):
            raise TypeError(f'Expected type {Commit}, got {type(commit)}')

        table = self.CommitByUuidAscending.__table_name__ if ascending else self.CommitByUuidDescending.__table_name__

        with self.cassandra:
            adjacent_commits = [model.to_commit() for model in self.cassandra.select_from_table(
                table, limit=1,
                repository_id=commit.repository_id, branch=commit.branch,
                uuid__lt=self.timestamp_to_uuid() if ascending else commit.uuid,
                uuid__gt=commit.uuid if ascending else 0,
            )]
            if len(adjacent_commits) == 0:
                return None
            return adjacent_commits[0]

    def next_commit(self, commit):
        return self._adjacent_commit(commit, ascending=True)

    def previous_commit(self, commit):
        return self._adjacent_commit(commit, ascending=False)

    def sibling_commits(self, commit, repository_ids):
        if not isinstance(commit, Commit):
            raise TypeError(f'Expected type {Commit}, got {type(commit)}')

        begin_timestamp_uuid = self.timestamp_to_uuid(commit.timestamp)
        end_timestamp_uuid = self.timestamp_to_uuid()

        with self.cassandra:
            next_commit = self.next_commit(commit)
            if next_commit:
                end_timestamp_uuid = self.timestamp_to_uuid(next_commit.timestamp)

            siblings = {}
            for id in repository_ids:
                siblings[id] = []
                if id not in self.repositories:
                    continue

                if commit.branch == self.repositories[commit.repository_id].DEFAULT_BRANCH or not self.branches(id, commit.branch):
                    branch = self.repositories[id].DEFAULT_BRANCH
                else:
                    branch = commit.branch

                siblings[id] = [model.to_commit() for model in self.cassandra.select_from_table(
                    self.CommitByUuidDescending.__table_name__,
                    repository_id=id, branch=branch,
                    uuid__gt=begin_timestamp_uuid,
                    uuid__lt=end_timestamp_uuid,
                )]
                previous = [model.to_commit() for model in self.cassandra.select_from_table(
                    self.CommitByUuidDescending.__table_name__, limit=1,
                    repository_id=id, branch=branch,
                    uuid__lte=begin_timestamp_uuid,
                )]
                if previous:
                    siblings[id].append(previous[0])
            return siblings

    def branches(self, repository_id, branch=None, limit=100):
        with self:
            if branch:
                # FIXME: SASI indecies are the canoical way to solve this problem, but require Cassandra 3.4 which
                # hasn't been deployed to our datacenters yet. This works for branches, but is less transparent.
                return [model.branch for model in self.cassandra.select_from_table(
                    self.Branches.__table_name__, limit=limit,
                    repository_id=repository_id, branch__gte=branch, branch__lte=(branch + '~'),
                )]

            return [model.branch for model in self.cassandra.select_from_table(
                self.Branches.__table_name__, limit=limit, repository_id=repository_id,
            )]

    def register_commit(self, commit):
        if not isinstance(commit, Commit):
            raise TypeError(f'Expected type {Commit}, got {type(commit)}')

        with self:
            if commit.repository_id not in self.repositories:
                self.repositories[commit.repository_id] = Repository(key=commit.repository_id)

            for table in [self.CommitByID, self.CommitByUuidAscending, self.CommitByUuidDescending]:
                self.cassandra.insert_row(
                    table.__table_name__,
                    repository_id=commit.repository_id, branch=commit.branch,
                    commit_id=str(commit.id).lower(), uuid=commit.uuid,
                    committer=commit.committer, message=commit.message,
                )
            self.cassandra.insert_row(
                self.Branches.__table_name__,
                repository_id=commit.repository_id, branch=commit.branch,
            )
            return commit

    def register_commit_with_repo_and_id(self, repository_id, branch, commit_id):
        if branch is None:
            branch = self.repositories[repository_id].DEFAULT_BRANCH
        if repository_id not in self.repositories:
            raise RuntimeError('{} is not a recognized repository')

        with self:
            commits = self.find_commits_by_id(repository_id=repository_id, branch=branch, commit_id=commit_id)
            if len(commits) > 1:
                raise SCMException(f'Multiple commits with the id {commit_id} exist in {repository_id} on {branch}')
            if commits:
                return commits[0]
            commit = self.repositories[repository_id].commit_for_id(commit_id, branch)
            return self.register_commit(commit)

    def url(self, commit):
        if not isinstance(commit, Commit):
            raise TypeError(f'Expected type {Commit}, got {type(commit)}')

        repo = self.repositories.get(commit.repository_id)
        if repo:
            return repo.url_for_commit(commit.id)
        return None
