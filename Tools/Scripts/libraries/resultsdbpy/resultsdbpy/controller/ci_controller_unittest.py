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
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.ci_context import BuildbotURLFactory
from resultsdbpy.model.ci_context_unittest import URLFactoryTest
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.test_context import Expectations
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class CIControllerTest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'ci_controller_test_keyspace'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        with URLFactoryTest.mock():
            cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
            model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=cls.KEYSPACE, create_keyspace=True))
            model.ci_context.add_url_factory(BuildbotURLFactory(master='build.webkit.org', redis=model.redis))
            app.register_blueprint(APIRoutes(model))

            with model.upload_context:
                # Mock results are more complicated because we want to attach results to builders
                for configuration in [
                    Configuration(platform='Mac', version_name='Catalina', version='10.15.0', sdk='19A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk1'),
                    Configuration(platform='Mac', version_name='Catalina', version='10.15.0', sdk='19A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk2'),
                    Configuration(platform='Mac', version_name='Mojave', version='10.14.0', sdk='18A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk1'),
                    Configuration(platform='Mac', version_name='Mojave', version='10.14.0', sdk='18A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk2'),
                ]:
                    build_count = [1]

                    def callback(commits, model=model, configuration=configuration, count=build_count):
                        results = MockModelFactory.layout_test_results()
                        results['details'] = {
                            'buildbot-master': URLFactoryTest.BUILD_MASTER,
                            'builder-name': f'{configuration.version_name}-{configuration.style}-{configuration.flavor.upper()}-Tests',
                            'build-number': str(count[0]),
                            'buildbot-worker': {
                                'Mojave': 'bot1',
                                'Catalina': 'bot2',
                            }.get(configuration.version_name, None),
                        }
                        model.upload_context.upload_test_results(configuration, commits, suite='layout-tests', timestamp=time.time(), test_results=results)
                        count[0] += 1

                    MockModelFactory.iterate_all_commits(model, callback)
                    MockModelFactory.process_results(model, configuration)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_all_queue_list(self, client, **kwargs):
        response = client.get(self.URL + '/api/urls/queue')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 4)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_single_queue_list(self, client, **kwargs):
        response = client.get(self.URL + '/api/urls/queue?version_name=Catalina&flavor=wk2')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [
            dict(
                configuration=dict(
                    is_simulator=False, platform='Mac',
                    architecture='x86_64', style='Release', flavor='wk2',
                    sdk='19A500', suite='layout-tests', version=10015000, version_name='Catalina',
                ),
                link='https://build.webkit.org/#/builders/5',
            )])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_all_builds_list(self, client, **kwargs):
        response = client.get(self.URL + '/api/urls')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 4)
        for data in response.json():
            self.assertEqual(len(data['urls']), 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_single_queue_list(self, client, **kwargs):
        response = client.get(self.URL + '/api/urls?version_name=Catalina&flavor=wk2&id=236542')
        self.assertEqual(response.status_code, 200)
        print(response.json()[0]['urls'])
        self.assertEqual(response.json()[0]['urls'][0]['build'], 'https://build.webkit.org/#/builders/5/builds/3')
        self.assertEqual(response.json()[0]['urls'][0]['queue'], 'https://build.webkit.org/#/builders/5')
        self.assertEqual(response.json()[0]['urls'][0]['worker'], 'https://build.webkit.org/#/workers/4')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_all_builds_list_by_time(self, client, **kwargs):
        response = client.get(self.URL + f'/api/urls?after_time={time.time() - 2}')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 4)
        for data in response.json():
            self.assertEqual(len(data['urls']), 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_no_builds_list_by_time(self, client, **kwargs):
        response = client.get(self.URL + f'/api/urls?after_time={time.time() + 2}')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(len(response.json()), 0)
