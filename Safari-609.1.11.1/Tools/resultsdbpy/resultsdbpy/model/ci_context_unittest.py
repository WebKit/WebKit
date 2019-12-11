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

import contextlib
import json
import mock
import time
import unittest

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.ci_context import BuildbotURLFactory, CIContext, BuildbotEightURLFactory
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.mock_repository import MockSVNRepository
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class URLFactoryTest(WaitForDockerTestCase):

    class MockRequest(object):

        def __init__(self, text='', status_code=200):
            self.text = text
            self.status_code = status_code

        def json(self):
            return json.loads(self.text)

    BUILD_MASTER = 'build.webkit.org'

    BUILDERS = dict(
        builders=[
            dict(
                builderid=1,
                name='Mojave-Release-Builder',
                description=None,
                masterids=[1],
                tags=[
                    'Mojave',
                    'Release',
                ],
            ),
            dict(
                builderid=2,
                name='Mojave-Release-WK2-Tests',
                description=None,
                masterids=[1],
                tags=[
                    'Mojave',
                    'Release',
                    'Tests',
                    'WK2',
                ],
            ),
            dict(
                builderid=3,
                name='Mojave-Release-WK1-Tests',
                description=None,
                masterids=[1],
                tags=[
                    'Mojave',
                    'Release',
                    'Tests',
                    'WK1',
                ],
            ),
            dict(
                builderid=4,
                name='Catalina-Release-Builder',
                description=None,
                masterids=[1],
                tags=[
                    'Catalina',
                    'Release',
                ],
            ),
            dict(
                builderid=5,
                name='Catalina-Release-WK2-Tests',
                description=None,
                masterids=[1],
                tags=[
                    'Catalina',
                    'Release',
                    'Tests',
                    'WK2',
                ],
            ),
            dict(
                builderid=6,
                name='Catalina-Release-WK1-Tests',
                description=None,
                masterids=[1],
                tags=[
                    'Catalina',
                    'Release',
                    'Tests',
                    'WK1',
                ],
            ),
        ],
        meta=dict(total=6),
    )
    WORKERS = dict(
        workers=[
            dict(
                workerid=1,
                name='builder1',
                configured_on=[
                    dict(
                        builderid=1,
                        masterid=1,
                    ),
                ],
                connected_to=[dict(masterid=1)],
                graceful=False,
                paused=False,
                workerinfo=dict(
                    access_uri='ssh://buildbot@builder1',
                    admin='WebKit Operations <email@webkit.org>',
                    host='?',
                    version='1.1.1'
                ),
            ),
            dict(
                workerid=2,
                name='builder2',
                configured_on=[
                    dict(
                        builderid=4,
                        masterid=1,
                    ),
                ],
                connected_to=[dict(masterid=1)],
                graceful=False,
                paused=False,
                workerinfo=dict(
                    access_uri='ssh://buildbot@builder2',
                    admin='WebKit Operations <email@webkit.org>',
                    host='?',
                    version='1.1.1'
                ),
            ),
            dict(
                workerid=3,
                name='bot1',
                configured_on=[
                    dict(
                        builderid=2,
                        masterid=1,
                    ),
                    dict(
                        builderid=3,
                        masterid=1,
                    ),
                ],
                connected_to=[dict(masterid=1)],
                graceful=False,
                paused=False,
                workerinfo=dict(
                    access_uri='ssh://buildbot@bot1',
                    admin='WebKit Operations <email@webkit.org>',
                    host='?',
                    version='1.1.1'
                ),
            ),
            dict(
                workerid=4,
                name='bot2',
                configured_on=[
                    dict(
                        builderid=5,
                        masterid=1,
                    ),
                    dict(
                        builderid=6,
                        masterid=1,
                    ),
                ],
                connected_to=[dict(masterid=1)],
                graceful=False,
                paused=False,
                workerinfo=dict(
                    access_uri='ssh://buildbot@bot2',
                    admin='WebKit Operations <email@webkit.org>',
                    host='?',
                    version='1.1.1'
                ),
            ),
        ],
        meta=dict(total=4),
    )

    @classmethod
    def get(cls, url, *args, **kwargs):
        if url == f'https://{cls.BUILD_MASTER}/api/v2/builders':
            return cls.MockRequest(text=json.dumps(cls.BUILDERS))
        if url == f'https://{cls.BUILD_MASTER}/api/v2/workers':
            return cls.MockRequest(text=json.dumps(cls.WORKERS))
        return cls.MockRequest(status_code=500)

    @classmethod
    @contextlib.contextmanager
    def mock(cls):
        with mock.patch('requests.get', new=cls.get):
            yield

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_builder_url(self, redis=StrictRedis):
        with self.mock():
            factory = BuildbotURLFactory(master='build.webkit.org', redis=redis())
            self.assertIsNone(factory.url(builder_name='Mojave-Release-Builder'))
            self.assertEqual('https://build.webkit.org/#/builders/1', factory.url(builder_name='Mojave-Release-Builder', should_fetch=True))
            self.assertEqual('https://build.webkit.org/#/builders/4', factory.url(builder_name='Catalina-Release-Builder'))
            self.assertIsNone(factory.url(builder_name='HighSierra-Release-Builder', should_fetch=True))

            self.assertEqual('https://build.webkit.org/#/builders/4/builds/8', factory.url(builder_name='Catalina-Release-Builder', build_number=8))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_worker_url(self, redis=StrictRedis):
        with self.mock():
            factory = BuildbotURLFactory(master='build.webkit.org', redis=redis())
            self.assertIsNone(factory.url(worker_name='builder1'))
            self.assertEqual('https://build.webkit.org/#/workers/1', factory.url(worker_name='builder1', should_fetch=True))
            self.assertEqual('https://build.webkit.org/#/workers/3', factory.url(worker_name='bot1'))
            self.assertIsNone(factory.url(worker_name='bot3', should_fetch=True))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis)
    def test_invalid_combinations(self, redis=StrictRedis):
        with self.mock():
            factory = BuildbotURLFactory(master='build.webkit.org', redis=redis())
            self.assertIsNone(factory.url(should_fetch=True))
            self.assertEqual('https://build.webkit.org/#/builders/1', factory.url(builder_name='Mojave-Release-Builder', worker_name='builder1', should_fetch=True))
            self.assertEqual('https://build.webkit.org/#/builders/1/builds/1', factory.url(builder_name='Mojave-Release-Builder', build_number=1, worker_name='builder1', should_fetch=True))
            self.assertEqual('https://build.webkit.org/#/workers/1', factory.url(build_number=1, worker_name='builder1', should_fetch=True))

    def test_old_builder_url(self):
        factory = BuildbotEightURLFactory(master='build.webkit.org')
        self.assertIsNone(factory.url())
        self.assertEqual('https://build.webkit.org/builders/Apple%20Mojave%20Release%20WK2%20%28Tests%29', factory.url(builder_name='Apple Mojave Release WK2 (Tests)'))
        self.assertEqual('https://build.webkit.org/builders/Apple%20Mojave%20Release%20%28Build%29', factory.url(builder_name='Apple Mojave Release (Build)'))
        self.assertEqual('https://build.webkit.org/builders/Apple%20Mojave%20Release%20%28Build%29/builds/7170', factory.url(builder_name='Apple Mojave Release (Build)', build_number=7170))

    def test_old_worker_url(self):
        factory = BuildbotEightURLFactory(master='build.webkit.org')
        self.assertEqual('https://build.webkit.org/buildslaves/bot610', factory.url(worker_name='bot610'))
        self.assertEqual('https://build.webkit.org/buildslaves/bot611', factory.url(worker_name='bot611'))
        self.assertEqual('https://build.webkit.org/builders/Apple%20Mojave%20Release%20WK2%20%28Tests%29', factory.url(builder_name='Apple Mojave Release WK2 (Tests)', worker_name='bot610'))


class CIContextTest(WaitForDockerTestCase):
    KEYSPACE = 'suite_context_test_keyspace'

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext):
        with URLFactoryTest.mock():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
            self.model.ci_context.add_url_factory(BuildbotURLFactory(master='build.webkit.org', redis=self.model.redis))

            with self.model.upload_context:
                # Mock results are more complicated because we want to attach results to builders
                for configuration in [
                    Configuration(platform='Mac', version_name='Catalina', version='10.15.0', sdk='19A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk1'),
                    Configuration(platform='Mac', version_name='Catalina', version='10.15.0', sdk='19A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk2'),
                    Configuration(platform='Mac', version_name='Mojave', version='10.14.0', sdk='18A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk1'),
                    Configuration(platform='Mac', version_name='Mojave', version='10.14.0', sdk='18A500', is_simulator=False, architecture='x86_64', style='Release', flavor='wk2'),
                ]:
                    build_count = [1]

                    def callback(commits, model=self.model, configuration=configuration, count=build_count):
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

                    MockModelFactory.iterate_all_commits(self.model, callback)
                    MockModelFactory.process_results(self.model, configuration)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_builder_for_single_configuration(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        urls = self.model.ci_context.find_urls_by_queue(configurations=[Configuration(version_name='Mojave', flavor='wk2')], suite='layout-tests')
        self.assertEqual(sorted(urls.values()), ['https://build.webkit.org/#/builders/2'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_builder_for_multiple_configuration(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        urls = self.model.ci_context.find_urls_by_queue(configurations=[Configuration(version_name='Catalina')], suite='layout-tests')
        self.assertEqual(sorted(urls.values()), ['https://build.webkit.org/#/builders/5', 'https://build.webkit.org/#/builders/6'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_url_for_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        urls = self.model.ci_context.find_urls_by_commit(
            configurations=[Configuration(version_name='Mojave', flavor='wk2')],
            suite='layout-tests',
            begin=MockSVNRepository.webkit().commit_for_id(236542),
            end=MockSVNRepository.webkit().commit_for_id(236542),
        )

        self.assertEqual(next(iter(urls.values()))[0]['queue'], 'https://build.webkit.org/#/builders/2')
        self.assertEqual(next(iter(urls.values()))[0]['build'], 'https://build.webkit.org/#/builders/2/builds/3')
        self.assertEqual(next(iter(urls.values()))[0]['worker'], 'https://build.webkit.org/#/workers/3')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_time(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        urls = self.model.ci_context.find_urls_by_commit(
            configurations=[Configuration(version_name='Catalina')],
            suite='layout-tests',
            begin_query_time=(time.time() - 60 * 60),
        )
        self.assertEqual(len(urls), 2)
        for urls_for_config in urls.values():
            self.assertEqual(len(urls_for_config), 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_no_tests_by_time(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        urls = self.model.ci_context.find_urls_by_commit(
            configurations=[Configuration(version_name='Mojave')],
            suite='layout-tests',
            end_query_time=(time.time() - 60 * 60),
        )
        self.assertEqual(len(urls), 0)
