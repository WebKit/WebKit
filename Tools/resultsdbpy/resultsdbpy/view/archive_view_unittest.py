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
import json
import time

from fakeredis import FakeStrictRedis
from resultsdbpy.controller.commit import Commit
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.configuration_context_unittest import ConfigurationContextTest
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from resultsdbpy.view.view_routes_unittest import WebSiteTestCase
from selenium.webdriver.support.select import Select


class ArchiveViewUnittest(WebSiteTestCase):
    def register_archive(self, client):
        response = client.post(
            self.URL + '/api/upload/archive',
            data=dict(
                configuration=json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
                suite='layout-tests',
                commits=json.dumps([MockStashRepository.safari().commit_for_id('bb6bda5f'), MockSVNRepository.webkit().commit_for_id(236542)], cls=Commit.Encoder),
            ),
            files=dict(file=base64.b64decode(MockModelFactory.ARCHIVE_ZIP)),
        )
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
