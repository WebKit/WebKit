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

    def test_folders_cut_at_each_slash(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('fast/canvas/hdr/float16-canvas-drawimage-canvas-accelerated.html'),
            ['fast', 'fast/canvas', 'fast/canvas/hdr'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('ChakraCore.yaml/ChakraCore/test/Array/ArrayBtreeBadCodeGen.js.default'),
            ['ChakraCore.yaml', 'ChakraCore.yaml/ChakraCore', 'ChakraCore.yaml/ChakraCore/test', 'ChakraCore.yaml/ChakraCore/test/Array'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('es6.yaml/es6/for..of_loops_iterator_closing_break.js.default'),
            ['es6.yaml', 'es6.yaml/es6'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('imported/selenium/py/test/selenium/webdriver/common/bidi_storage_tests.py/TestBidiStorage::test_add_and_get_cookie[webkitgtk]'),
            ['imported', 'imported/selenium', 'imported/selenium/py', 'imported/selenium/py/test', 'imported/selenium/py/test/selenium',
             'imported/selenium/py/test/selenium/webdriver', 'imported/selenium/py/test/selenium/webdriver/common',
             'imported/selenium/py/test/selenium/webdriver/common/bidi_storage_tests.py'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('WebCore/dom/Node.cpp/UncountedCallArgsChecker'),
            ['WebCore', 'WebCore/dom', 'WebCore/dom/Node.cpp'],
        )

    def test_folders_stop_at_a_query_or_parameters(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('imported/w3c/web-platform-tests/css/css-backgrounds/box-shadow-radius-generated.html?width=200&height=40&spread=50&radius=100px%20/%2020px'),
            ['imported', 'imported/w3c', 'imported/w3c/web-platform-tests', 'imported/w3c/web-platform-tests/css', 'imported/w3c/web-platform-tests/css/css-backgrounds'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('imported/w3c/webdriver/tests/bidi/browser/create_user_context/invalid.py/test_params_proxy_http_proxy_invalid_value[http://foo]'),
            ['imported', 'imported/w3c', 'imported/w3c/webdriver', 'imported/w3c/webdriver/tests', 'imported/w3c/webdriver/tests/bidi',
             'imported/w3c/webdriver/tests/bidi/browser', 'imported/w3c/webdriver/tests/bidi/browser/create_user_context',
             'imported/w3c/webdriver/tests/bidi/browser/create_user_context/invalid.py'],
        )

    def test_no_folder_right_after_a_separator(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('WebKit//Volumes/Data/analysis/webkit/WebKitBuild/WebKit.build/Release/WebKit.build/DerivedSources/WebKit-Swift-Generated.h/MemoryUnsafeCastChecker')[:2],
            ['WebKit', 'WebKit//Volumes'],
        )
        self.assertEqual(SubSuiteContext.sub_suites_for('Binary.Suite..Test'), ['Binary', 'Binary.Suite'])

    def test_folders_cut_at_each_dot_without_a_slash(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('reporelaypy.tests.checkout_unittest.CheckoutUnittest.test_constructor_no_sentinal'),
            ['reporelaypy', 'reporelaypy.tests', 'reporelaypy.tests.checkout_unittest', 'reporelaypy.tests.checkout_unittest.CheckoutUnittest'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('TestWebKitAPI.QuirksTest.ABadRowDoesNotAffectTheOtherRows'),
            ['TestWebKitAPI', 'TestWebKitAPI.QuirksTest'],
        )
        self.assertEqual(SubSuiteContext.sub_suites_for('TestWTF:WTF_Vector.MoveOnly_UncheckedAppend'), ['TestWTF', 'TestWTF:WTF_Vector'])
        self.assertEqual(SubSuiteContext.sub_suites_for('TestLoaderClient:afterAll'), ['TestLoaderClient'])
        self.assertEqual(SubSuiteContext.sub_suites_for('index.html'), ['index'])

    def test_dotted_names_with_a_slash_cut_only_at_slash(self):
        self.assertEqual(
            SubSuiteContext.sub_suites_for('TestIPC.ArgumentCoderTest/ArgumentCoderDecodingMoveCounterTest/Encoder.DecodeArray'),
            ['TestIPC.ArgumentCoderTest', 'TestIPC.ArgumentCoderTest/ArgumentCoderDecodingMoveCounterTest'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('TestWebKitAPI.EnhancedSecurityTests/securityRestrictionModeFromNavigationDecider(enabled:)'),
            ['TestWebKitAPI.EnhancedSecurityTests'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('TestWebKitFindController:/webkit/WebKitFindController/previous'),
            ['TestWebKitFindController:', 'TestWebKitFindController:/webkit', 'TestWebKitFindController:/webkit/WebKitFindController'],
        )
        self.assertEqual(
            SubSuiteContext.sub_suites_for('WPE/TestWebKitWebView:/webkit/WebKitWebView/autoplay-policy'),
            ['WPE', 'WPE/TestWebKitWebView:', 'WPE/TestWebKitWebView:/webkit', 'WPE/TestWebKitWebView:/webkit/WebKitWebView'],
        )

    def test_no_folders_without_a_separator(self):
        self.assertEqual(SubSuiteContext.sub_suites_for('canary-test'), [])

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

        self.assertEqual(self.runs_for('TestWebKitAPI')[0]['stats'], self.stats(run=2, failed=1, timedout=1, unexpected_failed=1, unexpected_timedout=1))
        self.assertEqual(self.runs_for('TestWebKitAPI.HSTS')[0]['stats'], self.stats(run=1))
        self.assertEqual(self.runs_for('TestWebKitAPI.TextExtractionTests')[0]['stats'], self.stats(run=2, failed=1, unexpected_failed=1))
        self.assertEqual(self.runs_for('TestWTF:WTF_Deque')[0]['stats'], self.stats(run=1, failed=1))
        self.assertEqual(
            self.runs_for('TestWebCore.FontCascadeTest')[0]['stats'],
            self.stats(run=1, failed=1, timedout=1, crashed=1, unexpected_failed=1, unexpected_timedout=1, unexpected_crashed=1),
        )
        self.assertEqual(self.runs_for('TestAuthentication:')[0]['stats'], self.stats(run=1, failed=1, timedout=1))
        self.assertEqual(self.runs_for('TestAuthentication:/webkit/Authentication')[0]['stats'], self.stats(run=1, failed=1, timedout=1))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_slash_tests_count_toward_their_slash_folders_only(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='api-tests', test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={
                'TestIPC.ArgumentCoderTest': {'ArgumentCoderDecodingMoveCounterTest': {
                    'Encoder.DecodeArray': dict(actual='FAIL', time=450),
                    'Encoder.DecodeDirectly': dict(time=140),
                }},
                'TestIPC.ArgumentCoderDecodingMoveCounterTest.DecodeArray': dict(time=104),
            },
        ))

        self.assertEqual(self.runs_for('TestIPC.ArgumentCoderTest')[0]['stats'], self.stats(run=2, failed=1, unexpected_failed=1))
        self.assertEqual(self.runs_for('TestIPC')[0]['stats'], self.stats(run=1))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_top_level_sub_suites_add_up_to_the_suite(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={
                'TestWebKitAPI.Diffs.Text': dict(actual='TEXT', expected='FAIL'),
                'TestWebKitAPI.Diffs.Audio': dict(actual='AUDIO', expected='FAIL'),
                'TestWebKitAPI.Diffs.Image': dict(actual='IMAGE', expected='FAIL'),
                'TestWebKitAPI.Diffs.Pass': dict(),
                'TestWTF:WTF_Deque.Error': dict(actual='ERROR'),
                'TestWTF:WTF_Deque.Timeout': dict(actual='TIMEOUT'),
                'TestAuthentication:': {'webkit': {'Authentication': {'authentication-crash': dict(actual='CRASH', expected='CRASH')}}},
            },
        )
        self.init_database(redis=redis, cassandra=cassandra, suite='api-tests', test_results=test_results)

        suite_row = next(iter(self.model.suite_context.find_by_commit(configurations=[self.CONFIGURATION], suite='api-tests').values()))[0]
        top_level = [self.runs_for(sub_suite)[0] for sub_suite in ['TestWebKitAPI', 'TestWTF', 'TestAuthentication:']]
        for key in self.stats(run=0):
            self.assertEqual(sum(sub_suite['stats'][key] for sub_suite in top_level), suite_row['stats'][key])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_zero_counts_written(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)

        with self.model.sub_suite_context:
            rows = next(iter(self.model.configuration_context.select_from_table_with_configurations(
                SubSuiteContext.SubSuiteResultsByCommit.__table_name__, configurations=[self.CONFIGURATION],
                suite='layout-tests', sub_suite='fast', limit=1,
            ).values()))
        self.assertEqual(json.loads(rows[0].stats), self.stats(run=4))
        self.assertEqual(json.loads(rows[0].details), {})

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

        self.assertEqual(self.runs_for('fast', suite='layout-tests')[0]['details'], dict(number_of_links=2))
        self.assertEqual(self.runs_for('editing', suite='layout-tests')[0]['details'], dict(number_of_links=2))

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
    def test_trie(self, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['results']['fast']['dom'] = {'HTMLAnchorElement': {'anchor-nodownload.html': dict(actual='TIMEOUT', time=1.2)}}
        test_results['results']['editing'] = {'caret.html': dict(time=1.2)}
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

        self.assertEqual(self.model.sub_suite_context.trie('layout-tests'), dict(
            suite='layout-tests',
            sub_suites={
                'editing': {},
                'fast': {'fast/dom': {'fast/dom/HTMLAnchorElement': {}}, 'fast/encoding': {}},
            },
            max_selected=10,
        ))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_api_test_trie(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='api-tests', test_results=self.api_test_results())

        self.assertEqual(self.model.sub_suite_context.trie('api-tests')['sub_suites'], {
            'TestAuthentication:': {'TestAuthentication:/webkit': {'TestAuthentication:/webkit/Authentication': {}}},
            'TestWTF': {'TestWTF:WTF_Deque': {}},
            'TestWebCore': {'TestWebCore.FontCascadeTest': {}},
            'TestWebKitAPI': {'TestWebKitAPI.DownloadProgress': {}, 'TestWebKitAPI.HSTS': {}, 'TestWebKitAPI.TextExtractionTests': {}},
        })

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_trie_nests_by_folder_not_by_sort_order(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={'fast': {
                'css': {'cascade': {'background-clip-and-webkit-background-clip-cascade-order.html': dict(time=1)}},
                'css-grid-layout': {'aspect-ratio-auto-repeat-crash.html': dict(time=1)},
            }},
        ))

        self.assertEqual(self.model.sub_suite_context.trie('layout-tests')['sub_suites'], {
            'fast': {'fast/css': {'fast/css/cascade': {}}, 'fast/css-grid-layout': {}},
        })

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_sub_suites_listed_as_long_as_their_rows_live(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        configuration, upload = self.newest_upload('layout-tests')
        self.model.sub_suite_context.ttl_seconds = 1
        self.model.sub_suite_context.register(
            configuration, upload['commits'], suite='layout-tests',
            test_results=dict(results={'expired': {'retired': {'test.html': dict(time=1)}}}),
        )

        self.assertEqual(self.runs_for('expired', suite='layout-tests'), [])
        self.assertEqual(self.model.sub_suite_context.trie('layout-tests')['sub_suites'], {'fast': {'fast/encoding': {}}})

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_suite_without_sub_suites(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='canary-upload-check', test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={'canary-test': dict(actual='FAIL', time=1)},
        ))

        self.assertEqual(self.runs_for('canary-test', suite='canary-upload-check'), [])
        self.assertEqual(self.model.sub_suite_context.trie('canary-upload-check')['sub_suites'], {})

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
        self.assertEqual(self.model.sub_suite_context.trie('safer-cpp-checks')['sub_suites'], {'WebCore': {'WebCore/dom': {'WebCore/dom/Node.cpp': {}}}})
