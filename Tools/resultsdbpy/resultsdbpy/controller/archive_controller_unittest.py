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

import base64
import io
import json
import requests

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.controller.commit import Commit
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.configuration_context_unittest import ConfigurationContextTest
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.model import Model
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class ArchiveControllerUnittest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'archive_controller_test_keyspace'
    ARCHIVE_API_ENDPOINT = 'api/upload/archive'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        redis_instance = redis()
        safari = MockStashRepository.safari(redis_instance)
        webkit = MockSVNRepository.webkit(redis_instance)

        cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
        cassandra_instance = cassandra(keyspace=cls.KEYSPACE, create_keyspace=True)

        app.register_blueprint(APIRoutes(Model(
            redis=redis_instance,
            cassandra=cassandra_instance,
            repositories=[safari, webkit],
            default_ttl_seconds=None,
            archive_ttl_seconds=None,
        )))

    @classmethod
    def upload_file(cls, client, url, meta_data, content):
        # Uploads work differently in the flask client than in requests
        if client == requests:
            return client.post(
                url,
                data=meta_data,
                files=dict(file=content),
            )
        else:
            return client.post(
                url,
                data=dict(
                    file=(io.BytesIO(content), 'archive.zip'),
                    **meta_data
                )
            )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload(self, client, **kwargs):
        upload_content = 'bad data'.encode('ascii')
        upload_meta = dict(
            configuration=json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
            suite='layout-tests',
            commits=json.dumps([MockStashRepository.safari().commit_for_id('bb6bda5f'), MockSVNRepository.webkit().commit_for_id(236542)], cls=Commit.Encoder),
        )

        response = self.upload_file(client, f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', upload_meta, upload_content)
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'Archive is not a zipfile')

        response = client.post(f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', data=str(json.dumps(upload_meta)))
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'No archive provided')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload_and_download(self, client, **kwargs):
        upload_content = base64.b64decode(MockModelFactory.ARCHIVE_ZIP)
        upload_meta = dict(
            configuration=json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
            suite='layout-tests',
            commits=json.dumps([MockStashRepository.safari().commit_for_id('bb6bda5f'), MockSVNRepository.webkit().commit_for_id(236542)], cls=Commit.Encoder),
        )

        response = self.upload_file(client, f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', upload_meta, upload_content)
        self.assertEqual(response.status_code, 200)

        response = client.get(f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.content, base64.b64decode(MockModelFactory.ARCHIVE_ZIP))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_no_archive_available(self, client, **kwargs):
        response = client.get(f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}')
        self.assertEqual(response.status_code, 404)
        self.assertEqual(response.json()['description'], 'No archives matching the specified criteria')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_metadata(self, client, **kwargs):
        upload_content = base64.b64decode(MockModelFactory.ARCHIVE_ZIP)
        response = self.upload_file(client, f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', dict(
            configuration=json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
            commits=json.dumps([MockStashRepository.safari().commit_for_id('bb6bda5f'), MockSVNRepository.webkit().commit_for_id(236542)], cls=Commit.Encoder),
        ), upload_content)
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'No test suite specified')

        response = self.upload_file(client, f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', dict(
            suite='layout-tests',
            commits=json.dumps([MockStashRepository.safari().commit_for_id('bb6bda5f'), MockSVNRepository.webkit().commit_for_id(236542)], cls=Commit.Encoder),
        ), upload_content)
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'Cannot register a partial configuration')

        response = self.upload_file(client, f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', dict(
            configuration=json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
            suite='layout-tests',
        ), upload_content)
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'No commits provided')

        response = self.upload_file(client, f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', dict(
            configuration='Invalid meta-data',
            suite='layout-tests',
            commits=json.dumps([MockStashRepository.safari().commit_for_id('bb6bda5f'), MockSVNRepository.webkit().commit_for_id(236542)], cls=Commit.Encoder),
        ), upload_content)
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'Invalid configuration, error: Expecting value: line 1 column 1 (char 0)')

        response = self.upload_file(client, f'{self.URL}/{self.ARCHIVE_API_ENDPOINT}', dict(
            configuration=json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
            suite='layout-tests',
            commits='Invalid meta-data',
        ), upload_content)
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'Expected commit meta-data to be json')
