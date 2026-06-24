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

import base64
import io
import json
import zipfile
from unittest.mock import patch

import requests
from fakeredis import FakeStrictRedis
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.archive_context import ArchiveContext
from resultsdbpy.model.configuration_context_unittest import ConfigurationContextTest
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from resultsdbpy.view.view_routes_unittest import WebSiteTestCase
from webkitscmpy import Commit


class ArchiveViewUnittest(WebSiteTestCase):
    COMMITS = [
        dict(
            repository_id='safari',
            id='d8bce26fa65c6fc8f39c17927abb77f69fab82fc',
            branch='main',
            timestamp=1601668000,
            order=1,
            committer='jbedard@apple.com',
            message='Patch Series\n',
        ),
        dict(
            repository_id='webkit',
            id='6',
            branch='main',
            timestamp=1601665100,
            order=0,
            committer='jbedard@apple.com',
            message='6th commit\n',
        ),
    ]

    @classmethod
    def upload_archive(cls, client, content):
        # Uploads work differently in the flask client than in requests.
        meta = dict(
            configuration=json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
            suite='layout-tests',
            commits=json.dumps(cls.COMMITS, cls=Commit.Encoder),
        )
        url = cls.URL + '/api/upload/archive'
        if client == requests:
            return client.post(url, data=meta, files=dict(file=content))
        return client.post(url, data=dict(file=(io.BytesIO(content), 'archive.zip'), **meta))

    def register_archive(self, client):
        response = self.upload_archive(client, base64.b64decode(MockModelFactory.ARCHIVE_ZIP))
        self.assertEqual(response.status_code, 200)

    def register_archive_with_files(self, client, files):
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, 'w') as archive:
            archive.writestr(zipfile.ZipInfo('archive/'), '')
            for name, content in files.items():
                if isinstance(content, str):
                    content = content.encode('utf-8')
                archive.writestr(f'archive/{name}', content)
        response = self.upload_archive(client, buffer.getvalue())
        self.assertEqual(response.status_code, 200)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_directory(self, driver, client, **kwargs):
        self.register_archive(client)
        driver.get(self.URL + '/archive')

        titles = driver.find_elements_by_class_name('title')
        self.assertEqual(len(titles), 2)
        self.assertEqual(titles[1].text.strip(), '/')

        path_list = driver.find_element_by_id('paths')
        self.assertIsNotNone(path_list)
        files = path_list.find_elements_by_class_name('item')
        self.assertEqual(len(files), 2)
        self.assertEqual(files[0].text.strip(), 'file.txt')
        self.assertEqual(files[1].text.strip(), 'index.html')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_file(self, driver, client, **kwargs):
        self.register_archive(client)
        response = client.get(self.URL + '/archive/file.txt')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.text, 'data')
        self.assertEqual(response.headers.get('Cache-Control'), 'public,max-age=43200')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_html_rewrites_modern_link_template(self, client, **kwargs):
        template = b"<a href=\"' + encodeURI(path + suffix) + '\">x</a>"
        self.register_archive_with_files(client, {'index.html': template})
        response = client.get(f'{self.URL}/archive/index.html?platform=mac&suite=layout-tests')
        self.assertEqual(response.status_code, 200)
        self.assertIn(
            b"href=\"' + encodeURI(path + suffix) + '?platform=mac&suite=layout-tests' + '\"",
            response.data,
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_html_rewrites_legacy_link_template(self, client, **kwargs):
        template = b"<a href=\"' + testPrefix + suffix + '\">x</a>"
        self.register_archive_with_files(client, {'legacy.html': template})
        response = client.get(f'{self.URL}/archive/legacy.html?platform=mac&suite=layout-tests')
        self.assertEqual(response.status_code, 200)
        self.assertIn(
            b"href=\"' + testPrefix + suffix + '?platform=mac&suite=layout-tests' + '\"",
            response.data,
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_extract_uses_referer_when_query_missing(self, client, **kwargs):
        self.register_archive(client)
        response = client.get(
            f'{self.URL}/archive/file.txt',
            headers={'Referer': f'{self.URL}/archive/index.html?platform=mac&suite=layout-tests'},
        )
        self.assertEqual(response.status_code, 302)
        self.assertTrue(
            response.headers['Location'].endswith('/archive/file.txt?platform=mac&suite=layout-tests'),
            f'unexpected redirect target: {response.headers["Location"]}',
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_extract_ignores_cross_origin_referer(self, client, **kwargs):
        self.register_archive(client)
        response = client.get(
            f'{self.URL}/archive/file.txt',
            headers={'Referer': 'https://attacker.example/?platform=mac'},
        )
        self.assertNotEqual(response.status_code, 302)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_extract_listing_does_not_redirect_via_referer(self, client, **kwargs):
        self.register_archive(client)
        response = client.get(
            f'{self.URL}/archive/',
            headers={'Referer': f'{self.URL}/suites?platform=mac'},
        )
        self.assertNotEqual(response.status_code, 302)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_memory_cap_returns_400(self, client, **kwargs):
        self.register_archive(client)
        with patch.object(
            ArchiveContext, 'file',
            side_effect=RuntimeError('Hit soft-memory cap when fetching archives, aborting'),
        ):
            response = client.get(f'{self.URL}/archive/file.txt?platform=mac&suite=layout-tests')
        self.assertEqual(response.status_code, 400)
