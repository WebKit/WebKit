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

import time

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.test_context import Expectations
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class TestControllerTest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'test_controller_test_keyspace'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
        model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=cls.KEYSPACE, create_keyspace=True))
        app.register_blueprint(APIRoutes(model))

        MockModelFactory.add_mock_results(model)
        MockModelFactory.process_results(model)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_all(self, client, **kwargs):
        response = client.get(self.URL + '/api/layout-tests/tests')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [
            'fast/encoding/css-cached-bom.html',
            'fast/encoding/css-charset-default.xhtml',
            'fast/encoding/css-charset.html',
            'fast/encoding/css-link-charset.html',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_all(self, client, **kwargs):
        response = client.get(self.URL + '/api/layout-tests/tests?limit=2')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [
            'fast/encoding/css-cached-bom.html',
            'fast/encoding/css-charset-default.xhtml',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_partial(self, client, **kwargs):
        response = client.get(self.URL + '/api/layout-tests/tests?test=fast/encoding/css-charset')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [
            'fast/encoding/css-charset-default.xhtml',
            'fast/encoding/css-charset.html',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_results(self, client, **kwargs):
        response = client.get(self.URL + '/api/results/layout-tests/fast/encoding/css-charset.html?platform=iOS&style=Debug')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 2)

        for i in range(2):
            self.assertEqual(len(response.json()[i]['results']), 5)
            for result in response.json()[i]['results']:
                self.assertEqual(result['actual'], Expectations.PASS)
                self.assertEqual(result['expected'], Expectations.PASS)
                self.assertEqual(result['time'], 1.2)
                self.assertEqual(result['modifiers'], '')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_results_by_sdk(self, client, **kwargs):
        response = client.get(self.URL + '/api/results/layout-tests/fast/encoding/css-cached-bom.html?platform=iOS&style=Debug&is_simulator=True&recent=False')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 2)

        response = client.get(self.URL + '/api/results/layout-tests/fast/encoding/css-cached-bom.html?platform=iOS&style=Debug&is_simulator=True&recent=False&sdk=15A432')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 1)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_results_by_commit(self, client, **kwargs):
        response = client.get(self.URL + '/api/results/layout-tests/fast/encoding/css-link-charset.html?platform=iOS&style=Debug&after_id=336610a84&before_id=236542')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 2)
        for i in range(2):
            self.assertEqual(len(response.json()[i]['results']), 3)
            last_udid = 0
            for result in response.json()[i]['results']:
                self.assertGreaterEqual(result['uuid'], last_udid)
                last_udid = result['uuid']

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_results_by_time(self, client, **kwargs):
        response = client.get(f'{self.URL}/api/results/layout-tests/fast/encoding/css-link-charset.html?platform=iOS&style=Debug&recent=False&after_time={time.time() - 60 * 60}')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 2)
        for i in range(2):
            self.assertEqual(len(response.json()[i]['results']), 5)
            last_start_time = 0
            for result in response.json()[i]['results']:
                self.assertGreaterEqual(result['start_time'], last_start_time)
                last_start_time = result['start_time']

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_no_results_by_time(self, client, **kwargs):
        response = client.get(self.URL + f'/api/results/layout-tests/fast/encoding/css-link-charset.html?platform=iOS&style=Debug&recent=False&after_time={time.time() + 1}')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 0)
