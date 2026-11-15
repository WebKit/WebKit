# Copyright (C) 2026 Apple Inc. All rights reserved.
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
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.sub_suite_context import SubSuiteContext
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase


class SubSuiteContextTest(WaitForDockerTestCase):
    KEYSPACE = 'sub_suite_context_test_keyspace'
    CONFIGURATION = Configuration(platform='mac', style='Release', flavor='wk1')
    DAY = 24 * 60 * 60

    @classmethod
    def api_test_results(cls):
        return dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={
                'TestWebKitAPI.HSTS.ThirdPartyFetch': dict(time=104),
                'TestWebKitAPI.DownloadProgress.CancelDownloadWhenProgressIsCanceled': dict(actual='TIMEOUT', time=33301),
                'TestWebKitAPI.TextExtractionTests': {
                    'invalidURLsAreSkipped()': dict(actual='FAIL', time=33308),
                    'replacementStrings(for:)': dict(time=80, flakiness_num_tries=2, flakiness_num_passes=1),
                },
                'TestWTF:WTF_Deque.InitializerList': dict(actual='FAIL', expected='FAIL', time=1),
                'TestWebCore.FontCascadeTest.ComplexTextRetainsSystemFallbackFonts': dict(actual='CRASH', time=5),
                'TestAuthentication:': {'webkit': {'Authentication': {
                    'authentication-cancel': dict(actual='TIMEOUT', expected='TIMEOUT FAIL', time=2),
                }}},
            },
        )

    @classmethod
    def stats(cls, run, failed=0, timedout=0, crashed=0, unexpected_failed=0, unexpected_timedout=0, unexpected_crashed=0):
        return dict(
            tests_run=run, tests_failed=failed, tests_timedout=timedout, tests_crashed=crashed,
            tests_unexpected_failed=unexpected_failed, tests_unexpected_timedout=unexpected_timedout, tests_unexpected_crashed=unexpected_crashed,
        )

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, suite='layout-tests', test_results=None):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
            MockModelFactory.add_mock_results(self.model, suite=suite, test_results=test_results)
            MockModelFactory.process_results(self.model, suite=suite)

    def runs_for(self, sub_suite, suite='api-tests', configuration=CONFIGURATION, **kwargs):
        results = self.model.sub_suite_context.find_by_commit(configurations=[configuration], suite=suite, sub_suite=sub_suite, **kwargs)
        return next(iter(results.values()), [])

    def newest_upload(self, suite):
        with MockModelFactory.safari(), MockModelFactory.webkit(), self.model.upload_context:
            configuration, uploads = next(iter(self.model.upload_context.find_test_results(configurations=[self.CONFIGURATION], suite=suite).items()))
            return configuration, max(uploads, key=lambda upload: self.model.commit_context.uuid_for_commits(upload['commits']))

    def test_directories(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('layout-tests', 'fast/canvas/hdr/float16-canvas-drawimage-image-accelerated.html'),
            ['fast', 'fast/canvas', 'fast/canvas/hdr'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('javascriptcore-tests', 'ChakraCore.yaml/ChakraCore/test/Array/ArrayBtreeBadCodeGen.js.default'),
            ['ChakraCore.yaml', 'ChakraCore.yaml/ChakraCore', 'ChakraCore.yaml/ChakraCore/test', 'ChakraCore.yaml/ChakraCore/test/Array'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('layout-tests', 'imported/w3c/web-platform-tests/css/css-backgrounds/box-shadow-radius-generated.html?width=200&height=40&spread=50&radius=100px%20/%2020px'),
            ['imported', 'imported/w3c', 'imported/w3c/web-platform-tests', 'imported/w3c/web-platform-tests/css', 'imported/w3c/web-platform-tests/css/css-backgrounds'],
        )

    def test_folders_of_any_suite(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('safer-cpp-checks', 'WebCore/dom/Node.cpp/UncountedCallArgsChecker'),
            ['WebCore', 'WebCore/dom', 'WebCore/dom/Node.cpp'],
        )
        self.assertEqual(SubSuiteContext.sub_suites_for('media-tests', 'youtube/58118028'), ['youtube'])
        self.assertEqual(SubSuiteContext.sub_suites_for('canary-upload-check', 'canary-test'), [])

    def test_no_folder_inside_brackets(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('webdriver-tests', 'imported/w3c/webdriver/tests/bidi/browser/create_user_context/invalid.py/test_params_proxy_http_proxy_invalid_value[http://foo]'),
            ['imported', 'imported/w3c', 'imported/w3c/webdriver', 'imported/w3c/webdriver/tests', 'imported/w3c/webdriver/tests/bidi',
             'imported/w3c/webdriver/tests/bidi/browser', 'imported/w3c/webdriver/tests/bidi/browser/create_user_context',
             'imported/w3c/webdriver/tests/bidi/browser/create_user_context/invalid.py'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('webdriver-tests', 'imported/w3c/webdriver/tests/bidi/log/entry_added/console_args.py/test_remote_values[new RegExp(/foo/g)-remote_value4]'),
            ['imported', 'imported/w3c', 'imported/w3c/webdriver', 'imported/w3c/webdriver/tests', 'imported/w3c/webdriver/tests/bidi',
             'imported/w3c/webdriver/tests/bidi/log', 'imported/w3c/webdriver/tests/bidi/log/entry_added',
             'imported/w3c/webdriver/tests/bidi/log/entry_added/console_args.py'],
        )

    def test_dotted_names(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('webkitpy-tests', 'reporelaypy.tests.checkout_unittest.CheckoutUnittest.test_constructor_no_sentinal'),
            ['reporelaypy', 'reporelaypy.tests', 'reporelaypy.tests.checkout_unittest', 'reporelaypy.tests.checkout_unittest.CheckoutUnittest'],
        )

    def test_api_test_names(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('api-tests', 'TestWebKitAPI.QuirksTest.ABadRowDoesNotAffectTheOtherRows'),
            ['TestWebKitAPI', 'TestWebKitAPI.QuirksTest'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('api-tests', 'TestWebKitAPI.EnhancedSecurityTests/securityRestrictionModeFromNavigationDecider(enabled:)'),
            ['TestWebKitAPI', 'TestWebKitAPI.EnhancedSecurityTests'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('api-tests', 'TestWebKitAPI.SharedMemoryTest/SharedMemoryFromMemoryTest.CreateHandleCopyFromMemory/1024001MallocReadWrite'),
            ['TestWebKitAPI', 'TestWebKitAPI.SharedMemoryTest', 'TestWebKitAPI.SharedMemoryTest/SharedMemoryFromMemoryTest', 'TestWebKitAPI.SharedMemoryTest/SharedMemoryFromMemoryTest.CreateHandleCopyFromMemory'],
        )
        self.assertEqual(SubSuiteContext.sub_suites_for('api-tests', 'TestWTF:WTF_Vector.MoveOnly_UncheckedAppend'), ['TestWTF', 'TestWTF:WTF_Vector'])
        self.assertEqual(SubSuiteContext.sub_suites_for('api-tests', 'TestLoaderClient:afterAll'), ['TestLoaderClient'])
        self.assertEqual(
            SubSuiteContext.sub_suites_for('api-tests', 'TestWebKitFindController:/webkit/WebKitFindController/previous'),
            ['TestWebKitFindController', 'TestWebKitFindController:/webkit', 'TestWebKitFindController:/webkit/WebKitFindController'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('api-tests', 'WPE/TestWebKitWebView:/webkit/WebKitWebView/snapshot-color-quadrants'),
            ['WPE', 'WPE/TestWebKitWebView', 'WPE/TestWebKitWebView:/webkit', 'WPE/TestWebKitWebView:/webkit/WebKitWebView'],
        )

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_every_directory(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['results']['fast']['encoding']['css-cached-bom.html'] = dict(actual='IMAGE', expected='FAIL', time=1.2)
        test_results['results']['fast']['dom'] = {'HTMLAnchorElement': {'anchor-nodownload.html': dict(actual='TIMEOUT', time=1.2)}}
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

        self.assertEqual(len(self.runs_for('fast', suite='layout-tests')), 5)
        self.assertEqual(self.runs_for('fast', suite='layout-tests')[0]['stats'], self.stats(run=5, failed=2, timedout=1, unexpected_failed=1, unexpected_timedout=1))
        self.assertEqual(self.runs_for('fast/encoding', suite='layout-tests')[0]['stats'], self.stats(run=4, failed=1))
        self.assertEqual(self.runs_for('fast/dom', suite='layout-tests')[0]['stats'], self.stats(run=1, failed=1, timedout=1, unexpected_failed=1, unexpected_timedout=1))
        self.assertEqual(self.runs_for('fast/dom/HTMLAnchorElement', suite='layout-tests')[0]['stats'], self.stats(run=1, failed=1, timedout=1, unexpected_failed=1, unexpected_timedout=1))
        self.assertEqual(self.runs_for('fast/encoding/css-cached-bom.html', suite='layout-tests'), [])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_api_test_counts(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='api-tests', test_results=self.api_test_results())

        self.assertEqual(self.runs_for('TestWebKitAPI')[0]['stats'], self.stats(run=4, failed=2, timedout=1, unexpected_failed=2, unexpected_timedout=1))
        self.assertEqual(self.runs_for('TestWebKitAPI.HSTS')[0]['stats'], self.stats(run=1))
        self.assertEqual(self.runs_for('TestWebKitAPI.TextExtractionTests')[0]['stats'], self.stats(run=2, failed=1, unexpected_failed=1))
        self.assertEqual(self.runs_for('TestWTF:WTF_Deque')[0]['stats'], self.stats(run=1, failed=1))
        self.assertEqual(
            self.runs_for('TestWebCore.FontCascadeTest')[0]['stats'],
            self.stats(run=1, failed=1, timedout=1, crashed=1, unexpected_failed=1, unexpected_timedout=1, unexpected_crashed=1),
        )
        self.assertEqual(self.runs_for('TestAuthentication:/webkit/Authentication')[0]['stats'], self.stats(run=1, failed=1, timedout=1))
        self.assertEqual(self.runs_for('TestAuthentication:'), [])
        self.assertEqual(self.runs_for('TestWebKitAPI.TextExtractionTests')[0]['details'], dict(number_of_flaky_tests=1))
        self.assertEqual(self.runs_for('TestWebKitAPI.HSTS')[0]['details'], dict(number_of_flaky_tests=0))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_binaries_add_up_to_the_suite(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={
                'TestWebKitAPI.Diffs.Text': dict(actual='TEXT', expected='FAIL'),
                'TestWebKitAPI.Diffs.Audio': dict(actual='AUDIO', expected='FAIL'),
                'TestWebKitAPI.Diffs.Image': dict(actual='IMAGE', expected='FAIL'),
                'TestWebKitAPI.Flaky.Once': dict(flakiness_num_tries=2, flakiness_num_passes=1),
                'TestWebKitAPI.Flaky.Twice': dict(flakiness_num_tries=3, flakiness_num_passes=1),
                'TestWebKitAPI.Flaky.Never': dict(flakiness_num_tries=2, flakiness_num_passes=2),
                'TestWTF:WTF_Deque.Error': dict(actual='ERROR'),
                'TestWTF:WTF_Deque.Timeout': dict(actual='TIMEOUT'),
                'TestAuthentication:': {'webkit': {'Authentication': {'authentication-crash': dict(actual='CRASH', expected='CRASH')}}},
            },
        )
        self.init_database(redis=redis, cassandra=cassandra, suite='api-tests', test_results=test_results)

        suite_row = next(iter(self.model.suite_context.find_by_commit(configurations=[self.CONFIGURATION], suite='api-tests').values()))[0]
        binaries = [self.runs_for(binary)[0] for binary in ['TestWebKitAPI', 'TestWTF', 'TestAuthentication']]
        for key in self.stats(run=0):
            self.assertEqual(sum(binary['stats'][key] for binary in binaries), suite_row['stats'][key])
        self.assertEqual(sum(binary['details']['number_of_flaky_tests'] for binary in binaries), suite_row['details']['number_of_flaky_tests'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_zero_counts_left_out(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        with self.model.sub_suite_context:
            rows = next(iter(self.model.configuration_context.select_from_table_with_configurations(
                SubSuiteContext.SubSuiteResultsByCommit.__table_name__, configurations=[self.CONFIGURATION],
                suite='layout-tests', sub_suite='fast', limit=1,
            ).values()))
        self.assertEqual(json.loads(rows[0].stats), dict(tests_run=4))
        self.assertEqual(json.loads(rows[0].details), {})
        self.assertEqual(rows[0].unpack()['stats'], self.stats(run=4))
        self.assertEqual(rows[0].unpack()['details'], dict(number_of_flaky_tests=0))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_number_of_links(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['details'] = dict(
            link='dummy-link',
            build_link='https://build.webkit.org/#/builders/1/builds/2',
            results_link='https://build.webkit.org/results/2',
        )
        test_results['results']['editing'] = {'caret.html': dict(time=1.2)}
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

        self.assertEqual(self.runs_for('fast', suite='layout-tests')[0]['details'], dict(number_of_flaky_tests=0, number_of_links=2))
        self.assertEqual(self.runs_for('editing', suite='layout-tests')[0]['details'], dict(number_of_flaky_tests=0, number_of_links=2))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_start_time(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        results = self.model.sub_suite_context.find_by_start_time(
            configurations=[self.CONFIGURATION], suite='layout-tests', sub_suite='fast/encoding',
            begin_query_time=(time.time() - 60 * 60),
        )
        self.assertEqual(len(next(iter(results.values()))), 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_callback(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        with MockModelFactory.safari(), MockModelFactory.webkit(), self.model.upload_context:
            configuration, uploads = next(iter(self.model.upload_context.find_test_results(configurations=[self.CONFIGURATION], suite='layout-tests').items()))
            status = self.model.upload_context.process_test_results(
                configuration=configuration, commits=uploads[0]['commits'], suite='layout-tests',
                test_results=uploads[0]['test_results'], timestamp=uploads[0]['timestamp'],
            )
        self.assertEqual(status['sub-suite-results'], dict(status='ok'))
        self.assertEqual(self.model.sub_suite_context.register(configuration, uploads[0]['commits'], suite=None, test_results={})['status'], 'error')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_tree(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['results']['fast']['dom'] = {'HTMLAnchorElement': {'anchor-nodownload.html': dict(actual='TIMEOUT', time=1.2)}}
        test_results['results']['editing'] = {'caret.html': dict(time=1.2)}
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

        self.assertEqual(self.model.sub_suite_context.tree('layout-tests'), dict(
            suite='layout-tests',
            sub_suites=[
                dict(name='editing'),
                dict(name='fast', children=[
                    dict(name='fast/dom', children=[dict(name='fast/dom/HTMLAnchorElement', same_tests_as_parent=True)]),
                    dict(name='fast/encoding'),
                ]),
            ],
            has_runs=True,
            max_selected=10,
        ))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_api_test_tree(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='api-tests', test_results=self.api_test_results())

        self.assertEqual(self.model.sub_suite_context.tree('api-tests')['sub_suites'], [
            dict(name='TestAuthentication', children=[
                dict(name='TestAuthentication:/webkit', same_tests_as_parent=True, children=[
                    dict(name='TestAuthentication:/webkit/Authentication', same_tests_as_parent=True),
                ]),
            ]),
            dict(name='TestWTF', children=[dict(name='TestWTF:WTF_Deque', same_tests_as_parent=True)]),
            dict(name='TestWebCore', children=[dict(name='TestWebCore.FontCascadeTest', same_tests_as_parent=True)]),
            dict(name='TestWebKitAPI', children=[
                dict(name='TestWebKitAPI.DownloadProgress'),
                dict(name='TestWebKitAPI.HSTS'),
                dict(name='TestWebKitAPI.TextExtractionTests'),
            ]),
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_same_tests_as_parent(self, redis=StrictRedis, cassandra=CassandraContext):
        passed = dict(time=1)
        self.init_database(redis=redis, cassandra=cassandra, test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={
                'fast': {'inline': {'a.html': passed, 'b.html': passed, 'legacy-margin-trim': {'c.html': passed}}},
                'webgl': {'1.0.3': {'conformance': {'x.html': passed, 'y.html': passed}}},
            },
        ))

        self.assertEqual(self.model.sub_suite_context.tree('layout-tests')['sub_suites'], [
            dict(name='fast', children=[
                dict(name='fast/inline', same_tests_as_parent=True, children=[dict(name='fast/inline/legacy-margin-trim')]),
            ]),
            dict(name='webgl', children=[
                dict(name='webgl/1.0.3', same_tests_as_parent=True, children=[dict(name='webgl/1.0.3/conformance', same_tests_as_parent=True)]),
            ]),
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_older_run_with_a_test_of_the_parents_own(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        configuration, upload = self.newest_upload('layout-tests')
        self.model.sub_suite_context.register(
            configuration, upload['commits'], suite='layout-tests', timestamp=time.time() - 20 * self.DAY,
            test_results=dict(results={'fast': {'retired.html': dict(time=1)}}),
        )

        self.assertEqual(self.model.sub_suite_context.tree('layout-tests')['sub_suites'], [
            dict(name='fast', children=[dict(name='fast/encoding')]),
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_newest_run_of_each_sub_suite(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        self.assertEqual(self.model.redis.zcard('sub_suites:layout-tests'), 2)

        configuration, upload = self.newest_upload('layout-tests')
        start_time = int(time.time()) - 5
        self.model.sub_suite_context.register(
            configuration, upload['commits'], suite='layout-tests', timestamp=start_time,
            test_results=dict(results={'fast': {'forms': {'select.html': dict(time=1)}}}),
        )
        self.assertEqual(self.model.redis.zscore('sub_suites:layout-tests', 'fast'), start_time)
        self.assertEqual(self.model.redis.zscore('sub_suites:layout-tests', 'fast/forms'), start_time)
        self.assertNotEqual(self.model.redis.zscore('sub_suites:layout-tests', 'fast/encoding'), start_time)
        self.assertEqual(self.model.redis.zcard('sub_suites:layout-tests'), 3)
        self.assertEqual(self.model.redis.zscore('sub_suites_with_own_tests:layout-tests', 'fast/forms'), start_time)
        self.assertEqual(self.model.redis.zscore('sub_suites_with_own_tests:layout-tests', 'fast'), None)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_sub_suites_listed_by_name_whatever_their_last_run(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        configuration, upload = self.newest_upload('layout-tests')
        for name, days in [('quiet', 31), ('recent', 29)]:
            self.model.sub_suite_context.register(
                configuration, upload['commits'], suite='layout-tests', timestamp=time.time() - days * self.DAY,
                test_results=dict(results={name: {'test.html': dict(time=1)}}),
            )

        self.assertEqual(self.model.sub_suite_context.tree('layout-tests')['sub_suites'], [
            dict(name='fast', children=[dict(name='fast/encoding', same_tests_as_parent=True)]),
            dict(name='quiet'),
            dict(name='recent'),
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_sub_suites_listed_as_long_as_their_rows_live(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        configuration, upload = self.newest_upload('layout-tests')
        lifetime = self.model.sub_suite_context.ttl_seconds
        self.model.redis.zadd('sub_suites:layout-tests', {'expired-before-any-upload': time.time() - lifetime - 60})
        self.model.redis.zadd('sub_suites_with_own_tests:layout-tests', {'fast': time.time() - lifetime - 60})
        self.assertNotIn('expired-before-any-upload', [node['name'] for node in self.model.sub_suite_context.tree('layout-tests')['sub_suites']])
        self.assertEqual(self.model.sub_suite_context.tree('layout-tests')['sub_suites'][0]['children'], [dict(name='fast/encoding', same_tests_as_parent=True)])

        self.model.sub_suite_context.register(
            configuration, upload['commits'], suite='layout-tests', timestamp=time.time() - lifetime - 60,
            test_results=dict(results={'expired': {'test.html': dict(time=1)}}),
        )
        self.assertEqual(self.model.redis.zscore('sub_suites:layout-tests', 'expired'), None)
        self.assertEqual(self.model.redis.zscore('sub_suites:layout-tests', 'expired-before-any-upload'), None)
        self.assertEqual(self.model.redis.zscore('sub_suites_with_own_tests:layout-tests', 'expired'), None)
        self.assertEqual([node['name'] for node in self.model.sub_suite_context.tree('layout-tests')['sub_suites']], ['fast'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_has_runs(self, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
            self.assertEqual(self.model.sub_suite_context.tree('canary-upload-check')['has_runs'], False)

            MockModelFactory.add_mock_results(self.model, suite='canary-upload-check', test_results=dict(
                details=dict(link='dummy-link'),
                run_stats=dict(tests_skipped=0),
                results={'canary-test': dict(time=1)},
            ))
            MockModelFactory.process_results(self.model, suite='canary-upload-check')

        tree = self.model.sub_suite_context.tree('canary-upload-check')
        self.assertEqual(tree['sub_suites'], [])
        self.assertEqual(tree['has_runs'], True)
        self.assertEqual(self.model.sub_suite_context.tree('layout-tests')['has_runs'], False)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_suite_without_sub_suites(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='canary-upload-check', test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={'canary-test': dict(actual='FAIL', time=1)},
        ))

        self.assertEqual(self.runs_for('canary-test', suite='canary-upload-check'), [])
        self.assertEqual(self.model.redis.zcard('sub_suites:canary-upload-check'), 0)
        tree = self.model.sub_suite_context.tree('canary-upload-check')
        self.assertEqual(tree['sub_suites'], [])
        self.assertEqual(tree['has_runs'], True)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_folders_of_a_suite_named_by_path(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='safer-cpp-checks', test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={'WebCore': {'dom': {'Node.cpp': {
                'UncountedCallArgsChecker': dict(actual='FAIL', time=1),
                'UncountedLocalVarsChecker': dict(time=1),
            }}}},
        ))

        self.assertEqual(self.runs_for('WebCore', suite='safer-cpp-checks')[0]['stats'], self.stats(run=2, failed=1, unexpected_failed=1))
        self.assertEqual(self.runs_for('WebCore/dom/Node.cpp', suite='safer-cpp-checks')[0]['stats'], self.stats(run=2, failed=1, unexpected_failed=1))
        self.assertEqual(self.runs_for('WebCore/dom/Node.cpp/UncountedCallArgsChecker', suite='safer-cpp-checks'), [])
        tree = self.model.sub_suite_context.tree('safer-cpp-checks')
        self.assertEqual(tree['sub_suites'], [dict(name='WebCore', children=[dict(name='WebCore/dom', same_tests_as_parent=True, children=[
            dict(name='WebCore/dom/Node.cpp', same_tests_as_parent=True),
        ])])])
