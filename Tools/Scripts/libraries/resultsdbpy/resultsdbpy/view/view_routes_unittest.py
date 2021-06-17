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

import time

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.model import Model
from resultsdbpy.model.repository import StashRepository, WebKitRepository
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from resultsdbpy.view.view_routes import ViewRoutes


class WebSiteTestCase(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'web_site_testcase_keyspace'

    def toggle_drawer(self, driver, assert_displayed=None):
        sidebar = driver.find_element_by_class_name('sidebar')
        sidebar.find_elements_by_tag_name('button')[0].click()
        time.sleep(.5)
        if assert_displayed:
            self.assertNotIn('hidden', sidebar.get_attribute('class'))
        elif assert_displayed is False:
            self.assertIn('hidden', sidebar.get_attribute('class'))

    def find_input_with_name(self, driver, name):
        control = [label.parent for label in driver.find_element_by_class_name('sidebar').find_elements_by_tag_name('div') if label.find_element_by_tag_name('label').text == name][0]
        self.assertIsNotNone(control)
        return control

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
            redis_instance = redis()

            model = Model(
                redis=redis_instance, cassandra=cassandra(keyspace=cls.KEYSPACE, create_keyspace=True),
                repositories=[
                    WebKitRepository(),
                    StashRepository('https://bitbucket.example.com/projects/SAFARI/repos/safari'),
                ],
            )
            api_routes = APIRoutes(model=model, import_name=__name__)
            view_routes = ViewRoutes(model=model, controller=api_routes, import_name=__name__)

            app.register_blueprint(api_routes)
            app.register_blueprint(view_routes)

    @classmethod
    def decorator(cls):
        return FlaskTestCase.run_with_selenium()


class WebSiteUnittest(WebSiteTestCase):
    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_title(self, driver, **kwargs):
        driver.get(self.URL)
        self.assertEqual(driver.title, 'Results Database')
        title = driver.find_element_by_class_name('title').find_element_by_tag_name('div')
        self.assertEqual(title.text.lstrip().rstrip(), 'Results Database')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_main(self, driver, **kwargs):
        driver.get(self.URL)
        content = driver.find_element_by_id('app').find_element_by_class_name('content')
        self.assertIsNotNone(content)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @WebSiteTestCase.decorator()
    def test_commit_link(self, driver, **kwargs):
        driver.get(self.URL)
        actions = driver.find_element_by_class_name('actions')
        links = actions.find_elements_by_tag_name('a')

        commit_link = None
        for link in links:
            if 'Commits' in link.text:
                commit_link = link
                break
        self.assertIsNotNone(commit_link)

        commit_link.click()
        time.sleep(.1)
        self.assertEqual(driver.title, 'Results Database: Commits')
