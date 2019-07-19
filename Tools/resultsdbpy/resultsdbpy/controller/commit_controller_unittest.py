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

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.controller.commit import Commit
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.model import Model
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class CommitControllerTest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'commit_controller_test_keyspace'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        redis_instance = redis()
        safari = MockStashRepository.safari(redis_instance)
        webkit = MockSVNRepository.webkit(redis_instance)

        cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
        cassandra_instance = cassandra(keyspace=cls.KEYSPACE, create_keyspace=True)

        app.register_blueprint(APIRoutes(Model(redis=redis_instance, cassandra=cassandra_instance, repositories=[safari, webkit])))

    def register_all_commits(self, client):
        for mock_repository in [MockStashRepository.safari(), MockSVNRepository.webkit()]:
            for commit_list in mock_repository.commits.values():
                for commit in commit_list:
                    self.assertEqual(200, client.post(self.URL + '/api/commits/register', data=Commit.Encoder().default(commit)).status_code)

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
        self.assertEqual(dict(webkit=['safari-606-branch', 'trunk'], safari=['master', 'safari-606-branch']), response.json())

        response = client.get(self.URL + '/api/commits/branches?branch=safari')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(dict(webkit=['safari-606-branch'], safari=['safari-606-branch']), response.json())

        response = client.get(self.URL + '/api/commits/branches?repository_id=safari')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(dict(safari=['master', 'safari-606-branch']), response.json())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_register_errors(self, client, **kwargs):
        self.assertEqual(400, client.post(self.URL + '/api/commits/register').status_code)
        self.assertEqual(405, client.get(self.URL + '/api/commits/register').status_code)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_register_via_post(self, client, **kwargs):
        self.assertEqual(200, client.post(self.URL + '/api/commits', data=dict(repository_id='safari', id='bb6bda5f44dd2')).status_code)
        response = client.get(self.URL + '/api/commits?repository_id=safari')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            Commit.from_json(response.json()[0]),
            MockStashRepository.safari().commit_for_id('bb6bda5f44dd2'),
        )

        self.assertEqual(200, client.post(self.URL + '/api/commits', data=dict(repository_id='webkit', id='236544')).status_code)
        response = client.get(self.URL + '/api/commits?repository_id=webkit')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            Commit.from_json(response.json()[0]),
            MockSVNRepository.webkit().commit_for_id('236544'),
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_register_with_partial_commit(self, client, **kwargs):
        self.assertEqual(200, client.post(self.URL + '/api/commits', data=dict(repository_id='safari', id='bb6bda5f44dd2')).status_code)
        response = client.get(self.URL + '/api/commits?repository_id=safari')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            Commit.from_json(response.json()[0]),
            MockStashRepository.safari().commit_for_id('bb6bda5f44dd2'),
        )
        self.assertEqual(404, client.post(self.URL + '/api/commits', data=dict(repository_id='safari', id='aaaaaaaaaaaaa')).status_code)

        self.assertEqual(200, client.post(self.URL + '/api/commits', data=dict(repository_id='webkit', id='236544')).status_code)
        response = client.get(self.URL + '/api/commits?repository_id=webkit')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            Commit.from_json(response.json()[0]),
            MockSVNRepository.webkit().commit_for_id('236544'),
        )
        self.assertEqual(404, client.post(self.URL + '/api/commits', data=dict(repository_id='webkit', id='0')).status_code)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_register_with_full_commit(self, client, **kwargs):
        git_commit = MockStashRepository.safari().commit_for_id('bb6bda5f44dd2')
        self.assertEqual(200, client.post(self.URL + '/api/commits', data=Commit.Encoder().default(git_commit)).status_code)
        response = client.get(self.URL + '/api/commits?repository_id=safari')
        self.assertEqual(200, response.status_code)
        self.assertEqual(Commit.from_json(response.json()[0]), git_commit)

        svn_commit = MockSVNRepository.webkit().commit_for_id('236544')
        self.assertEqual(200, client.post(self.URL + '/api/commits', data=Commit.Encoder().default(svn_commit)).status_code)
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
        response = client.get(self.URL + '/api/commits?id=336610a8')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]), MockStashRepository.safari().commit_for_id(id='336610a8'))

        response = client.get(self.URL + '/api/commits?id=236540')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]), MockSVNRepository.webkit().commit_for_id(id=236540))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_timestamp(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?timestamp=1537550685')
        self.assertEqual(200, response.status_code)
        self.assertEqual(2, len(response.json()))
        self.assertEqual([Commit.from_json(element) for element in response.json()], [
            MockStashRepository.safari().commit_for_id(id='e64810a4'),
            MockStashRepository.safari().commit_for_id(id='7be40842'),
        ])

        response = client.get(self.URL + '/api/commits?timestamp=1538041791.8')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]), MockSVNRepository.webkit().commit_for_id(id=236541))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_uuid(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?uuid=153755068501')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]), MockStashRepository.safari().commit_for_id(id='7be40842'))

        response = client.get(self.URL + '/api/commits?uuid=153804179200')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(Commit.from_json(response.json()[0]), MockSVNRepository.webkit().commit_for_id(id=236541))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_range_id(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?after_id=336610a8&before_id=236540')
        self.assertEqual(200, response.status_code)
        self.assertEqual([Commit.from_json(element) for element in response.json()], [
            MockStashRepository.safari().commit_for_id(id='336610a8'),
            MockStashRepository.safari().commit_for_id(id='bb6bda5f'),
            MockSVNRepository.webkit().commit_for_id(id=236540),
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_range_timestamp(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?after_timestamp=1538041792.3&before_timestamp=1538049108')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            [Commit.from_json(element) for element in response.json()],
            [MockSVNRepository.webkit().commit_for_id(id=236541), MockSVNRepository.webkit().commit_for_id(id=236542)],
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_range_uuid(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits?after_uuid=153755068501&before_uuid=153756638602')
        self.assertEqual(200, response.status_code)
        self.assertEqual(
            [Commit.from_json(element) for element in response.json()],
            [MockStashRepository.safari().commit_for_id(id='7be40842'), MockStashRepository.safari().commit_for_id(id='336610a4')],
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_next(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits/next?id=336610a4')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(MockStashRepository.safari().commit_for_id(id='336610a8'), Commit.from_json(response.json()[0]))

        response = client.get(self.URL + '/api/commits/next?id=236542')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(MockSVNRepository.webkit().commit_for_id(id=236543), Commit.from_json(response.json()[0]))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_previous(self, client, **kwargs):
        self.register_all_commits(client)
        response = client.get(self.URL + '/api/commits/previous?id=336610a4')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(MockStashRepository.safari().commit_for_id(id='7be40842'), Commit.from_json(response.json()[0]))

        response = client.get(self.URL + '/api/commits/previous?id=236542')
        self.assertEqual(200, response.status_code)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(MockSVNRepository.webkit().commit_for_id(id=236541), Commit.from_json(response.json()[0]))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_siblings(self, client, **kwargs):
        self.register_all_commits(client)

        response = client.get(self.URL + '/api/commits/siblings?repository_id=webkit&id=236542')
        self.assertEqual(200, response.status_code)
        commits = {key: [Commit.from_json(element) for element in values] for key, values in response.json().items()}
        self.assertEqual(commits, {'safari': [MockStashRepository.safari().commit_for_id(id='bb6bda5f44dd24')]})

        response = client.get(self.URL + '/api/commits/siblings?repository_id=safari&id=bb6bda5f44dd24')
        self.assertEqual(200, response.status_code)
        commits = {key: [Commit.from_json(element) for element in values] for key, values in response.json().items()}
        self.assertEqual(commits, {'webkit': [
            MockSVNRepository.webkit().commit_for_id(id=236544),
            MockSVNRepository.webkit().commit_for_id(id=236543),
            MockSVNRepository.webkit().commit_for_id(id=236542),
            MockSVNRepository.webkit().commit_for_id(id=236541),
            MockSVNRepository.webkit().commit_for_id(id=236540),
        ]})
