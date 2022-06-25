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

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.model import Model
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from resultsdbpy.model.repository import StashRepository, WebKitRepository
from webkitcorepy import OutputCapture
from webkitscmpy import Commit


class CommitControllerTest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'commit_controller_test_keyspace'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            redis_instance = redis()
            safari = StashRepository('https://bitbucket.example.com/projects/SAFARI/repos/safari')
            webkit = WebKitRepository()

            cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
            cassandra_instance = cassandra(keyspace=cls.KEYSPACE, create_keyspace=True)

            app.register_blueprint(APIRoutes(Model(redis=redis_instance, cassandra=cassandra_instance, repositories=[safari, webkit])))

    def register_all_commits(self, client):
        with MockModelFactory.safari() as safari, MockModelFactory.webkit() as webkit:
            for name, mock_repository in dict(safari=safari, webkit=webkit).items():
                for commit_list in mock_repository.commits.values():
                    for commit in commit_list:
                        self.assertEqual(200, client.post(
                            self.URL + '/api/commits/register',
                            data=dict(id=commit.hash or commit.revision, repository_id=name)
                        ).status_code)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_repositories(self, client, **kwargs):
        response = client.get(self.URL + '/api/commits/repositories')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(['safari', 'webkit'], sorted(response.json()))

        self.assertEqual(400, client.get(self.URL + '/api/commits/repositories?repository_id=safari').status_code)
        self.assertEqual(405, client.post(self.URL + '/api/commits/repositories').status_code)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_branches(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits/branches')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(dict(
            webkit=['branch-a', 'branch-b', 'main'],
            safari=['branch-a', 'branch-b', 'eng/squash-branch', 'main']), response.json())

        response = client.get(self.URL + '/api/commits/branches?branch=branch')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(dict(webkit=['branch-a', 'branch-b'], safari=['branch-a', 'branch-b']), response.json())

        response = client.get(self.URL + '/api/commits/branches?repository_id=safari')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(dict(safari=['branch-a', 'branch-b', 'eng/squash-branch', 'main']), response.json())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_register_errors(self, client, **kwargs):
        self.assertEqual(400, client.post(self.URL + '/api/commits/register').status_code)
        self.assertEqual(405, client.get(self.URL + '/api/commits/register').status_code)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_register_with_partial_commit(self, client, **kwargs):
        with MockModelFactory.safari(), MockModelFactory.webkit(), OutputCapture():
            self.assertEqual(200, client.post(self.URL + '/api/commits', data=dict(repository_id='safari', id='d8bce26fa65c')).status_code)
            response = client.get(self.URL + '/api/commits?repository_id=safari')
            self.assertEqual(200, response.status_code)
            self.assertEqual(
                Commit.from_json(response.json()[0]).message,
                'Patch Series\n',
            )
            self.assertEqual(404, client.post(self.URL + '/api/commits', data=dict(repository_id='safari', id='aaaaaaaaaaaaa')).status_code)

            self.assertEqual(200, client.post(self.URL + '/api/commits', data=dict(repository_id='webkit', id='6')).status_code)
            response = client.get(self.URL + '/api/commits?repository_id=webkit')
            self.assertEqual(200, response.status_code)
            self.assertEqual(
                Commit.from_json(response.json()[0]).message,
                '6th commit',
            )
            self.assertEqual(404, client.post(self.URL + '/api/commits', data=dict(repository_id='webkit', id='1234')).status_code)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_register_with_full_commit(self, client, **kwargs):
        git_commit = Commit(
            repository_id='safari',
            branch='main',
            hash='8eba280e32daaf5fddbbb65aea37932ea9ad85df',
            timestamp=1601650100,
            order=0,
            author='jbedard@webkit.org',
            message='custom commit',
        )
        print(Commit.Encoder().default(git_commit))
        self.assertEqual(200, client.post(self.URL + '/api/commits', json=Commit.Encoder().default(git_commit)).status_code)
        response = client.get(self.URL + '/api/commits?repository_id=safari')
        self.assertEqual(200, response.status_code)
        self.assertEqual(Commit.from_json(response.json()[0]), git_commit)

        svn_commit = Commit(
            repository_id='webkit',
            branch='main',
            revision='1234',
            timestamp=1601650100,
            order=0,
            author='jbedard@webkit.org',
            message='custom commit',
        )
        self.assertEqual(200, client.post(self.URL + '/api/commits', json=Commit.Encoder().default(svn_commit)).status_code)
        response = client.get(self.URL + '/api/commits?repository_id=webkit')
        self.assertEqual(200, response.status_code)
        self.assertEqual(Commit.from_json(response.json()[0]), svn_commit)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_no_commit(self, client, **kwargs):
        self.register_all_commits(client)
        self.assertEqual(404, client.get(self.URL + '/api/commits?repository_id=safari&id=0').status_code)
        self.assertEqual(404, client.get(self.URL + '/api/commits?repository_id=webkit&id=0').status_code)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_id(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?id=d8bce26fa65c')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]).hash, 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc')

        response = client.get(self.URL + '/api/commits?id=6')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]).revision, 6)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_timestamp(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?timestamp=1601668000')
        self.assertEqual(200, response.status_code)
        self.assertEqual(2, len(response.json()))
        self.assertEqual([Commit.from_json(element).hash for element in response.json()], [
            'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
            'd8bce26fa65c6fc8f39c17927abb77f69fab82fc',
        ])

        response = client.get(self.URL + '/api/commits?timestamp=1601639900')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]).revision, 6)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_uuid(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?uuid=160166800001')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]).hash, 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc')

        response = client.get(self.URL + '/api/commits?uuid=160163990000')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]).revision, 6)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_range_id(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?after_id=6&before_id=fff83bb2d917')
        self.assertEqual(200, response.status_code)
        self.assertEqual([Commit.from_json(element).revision or Commit.from_json(element).hash for element in response.json()], [
            6, '9b8311f25a77ba14923d9d5a6532103f54abefcb', 'fff83bb2d9171b4d9196e977eb0508fd57e7a08d',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_range_timestamp(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?after_timestamp=1601637900&before_timestamp=1601639900')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            [Commit.from_json(element).revision for element in response.json()],
            [4, 6],
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_range_uuid(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?after_uuid=160166800000&before_uuid=160166800001')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            [Commit.from_json(element).hash for element in response.json()], [
                'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                'd8bce26fa65c6fc8f39c17927abb77f69fab82fc',
            ],
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_next(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits/next?id=bae5d1e90999')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual('d8bce26fa65c6fc8f39c17927abb77f69fab82fc', Commit.from_json(response.json()[0]).hash)

        response = client.get(self.URL + '/api/commits/next?id=4')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(6, Commit.from_json(response.json()[0]).revision)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_previous(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits/previous?id=d8bce26fa65c')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual('bae5d1e90999d4f916a8a15810ccfa43f37a2fd6', Commit.from_json(response.json()[0]).hash)

        response = client.get(self.URL + '/api/commits/previous?id=6')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(4, Commit.from_json(response.json()[0]).revision)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_siblings(self, client, **kwargs):
        self.register_all_commits(client)

        response = client.get(self.URL + '/api/commits/siblings?repository_id=webkit&id=6')
        self.assertEqual(200, response.status_code)
        commits = {key: [Commit.from_json(element).hash for element in values] for key, values in response.json().items()}
        self.assertEqual(
            commits, dict(safari=[
                'd8bce26fa65c6fc8f39c17927abb77f69fab82fc',
                'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                '1abe25b443e985f93b90d830e4a7e3731336af4d',
                'fff83bb2d9171b4d9196e977eb0508fd57e7a08d',
                '9b8311f25a77ba14923d9d5a6532103f54abefcb',
            ]),
        )

        response = client.get(self.URL + '/api/commits/siblings?repository_id=safari&id=d8bce26fa65c')
        self.assertEqual(200, response.status_code)
        commits = {key: [Commit.from_json(element).revision for element in values] for key, values in response.json().items()}
        self.assertEqual(commits, dict(webkit=[6]))
