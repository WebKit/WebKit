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
import time
from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class UploadControllerPostTest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'upload_controller_test_keyspace'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
            model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=cls.KEYSPACE, create_keyspace=True))
            model.upload_context.register_upload_callback('python-tests', lambda **kwargs: dict(status='ok'))
            app.register_blueprint(APIRoutes(model))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload(self, client, **kwargs):
        upload_dict = dict(
            suite='layout-tests',
            commits=[
                dict(repository_id='safari', hash='d8bce26fa65c6fc8f39c17927abb77f69fab82fc'),
                dict(repository_id='webkit', hash='75eaef1c9242f92a8d7694e8ccd310f69cf9683b'),
            ], configuration=Configuration.Encoder().default(Configuration(
                platform='Mac', version='10.14.0', sdk='18A391',
                is_simulator=False, architecture='x86_64',
                style='Release', flavor='wk2',
            )),
            test_results=MockModelFactory.layout_test_results(),
            timestamp=int(time.time()),
        )
        response = client.post(self.URL + '/api/upload', data=str(json.dumps(upload_dict)))
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()['status'], 'ok')
        self.assertEqual(response.json()['processing']['python-tests'], dict(status='ok'))

        response = client.get(self.URL + '/api/upload')
        self.assertEqual(response.status_code, 200)

        retrieved = response.json()[0]
        retrieved['commits'] = [dict(repository_id=commit['repository_id'], hash=commit.get('hash', commit.get('revision'))) for commit in retrieved['commits']]
        self.assertEqual(retrieved, upload_dict)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload_partial(self, client, **kwargs):
        upload_dict = dict(
            suite='layout-tests',
            commits=[dict(repository_id='safari', id='d8bce26fa65c6fc8f39c17927abb77f69fab82fc'), dict(repository_id='webkit', id='6')],
            configuration=Configuration.Encoder().default(Configuration(
                platform='iOS', version='12.0.0', sdk='16A404',
                is_simulator=True, architecture='x86_64',
                style='Release', flavor='wk2',
            )),
            test_results=MockModelFactory.layout_test_results(),
            timestamp=int(time.time()),
        )
        response = client.post(self.URL + '/api/upload', data=str(json.dumps(upload_dict)))
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()['status'], 'ok')
        self.assertEqual(response.json()['processing']['python-tests'], dict(status='ok'))

        response = client.get(self.URL + '/api/upload')
        self.assertEqual(response.status_code, 200)
        for key in ['suite', 'configuration', 'test_results', 'timestamp']:
            self.assertEqual(response.json()[0][key], upload_dict[key])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_no_download(self, client, **kwargs):
        response = client.get(self.URL + '/api/upload')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_no_process(self, client, **kwargs):
        response = client.post(self.URL + '/api/process')
        self.assertEqual(response.status_code, 404)


class UploadControllerTest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'upload_controller_test_keyspace'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
            model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=cls.KEYSPACE, create_keyspace=True))
            model.upload_context.register_upload_callback('python-tests', lambda **kwargs: dict(status='ok'))
            MockModelFactory.add_mock_results(model)
            app.register_blueprint(APIRoutes(model))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_suites(self, client, **kwargs):
        response = client.get(self.URL + '/api/suites')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(12, len(response.json()))
        for element in response.json():
            self.assertEqual(1, len(element[1]))
            self.assertEqual('layout-tests', element[1][0])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_suites_with_filter(self, client, **kwargs):
        response = client.get(self.URL + '/api/suites?platform=Mac&style=Release&flavor=wk2')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(1, len(response.json()[0][1]))
        self.assertEqual('layout-tests', response.json()[0][1][0])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_suites_with_suite_filter(self, client, **kwargs):
        response = client.get(self.URL + '/api/suites?suite=layout-tests')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(12, len(response.json()))

        response = client.get(self.URL + '/api/suites?suite=api-tests')
        self.assertEqual(response.status_code, 404)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_download_with_filter(self, client, **kwargs):
        response = client.get(self.URL + '/api/upload?platform=Mac&style=Release&flavor=wk2')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(5, len(response.json()))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_download_with_duel_filter(self, client, **kwargs):
        response = client.get(self.URL + '/api/upload?platform=Mac&style=Release&flavor=wk1&flavor=wk2')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(10, len(response.json()))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_download_with_limit(self, client, **kwargs):
        response = client.get(self.URL + '/api/upload?platform=Mac&style=Release&flavor=wk2&limit=2')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(2, len(response.json()))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_download_with_range(self, client, **kwargs):
        response = client.get(self.URL + '/api/upload?platform=Mac&style=Release&flavor=wk2&after_id=1abe25b443e9&before_id=d8bce26fa65c')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(3, len(response.json()))
        self.assertEqual(sorted([
            'd8bce26fa65c6fc8f39c17927abb77f69fab82fc',
            'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
            '1abe25b443e985f93b90d830e4a7e3731336af4d',
        ]), sorted([result['commits'][1]['hash'] for result in response.json()]))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_download_for_commit(self, client, **kwargs):
        response = client.get(self.URL + '/api/upload?platform=Mac&style=Release&flavor=wk2&id=d8bce26fa65c')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(1, len(response.json()))
        self.assertEqual(['d8bce26fa65c6fc8f39c17927abb77f69fab82fc'], [result['commits'][1]['hash'] for result in response.json()])

        response = client.get(self.URL + '/api/upload?platform=Mac&style=Release&flavor=wk2&id=6')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(5, len(response.json()))
        self.assertEqual([6] * 5, [result['commits'][0]['revision'] for result in response.json()])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_process(self, client, **kwargs):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            response = client.post(self.URL + '/api/upload/process?platform=Mac&style=Release&flavor=wk2')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(5, len(response.json()))
            self.assertEqual([dict(status='ok')] * 5, [element['processing']['python-tests'] for element in response.json()])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_process_commit(self, client, **kwargs):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            response = client.post(self.URL + '/api/upload/process?platform=Mac&style=Release&flavor=wk2&id=d8bce26fa65c')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(1, len(response.json()))
            self.assertEqual(['d8bce26fa65c6fc8f39c17927abb77f69fab82fc'], [result['commits'][1]['hash'] for result in response.json()])
            self.assertEqual([dict(status='ok')], [element['processing']['python-tests'] for element in response.json()])
