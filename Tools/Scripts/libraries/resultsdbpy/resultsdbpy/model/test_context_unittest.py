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
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.test_context import Expectations, TestContext
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from unittest import mock


class TestContextTest(WaitForDockerTestCase):
    KEYSPACE = 'test_context_test_keyspace'
    DETAILS_WITH_LINKS = dict(link='https://ci.example.com/build/1', build_link='https://ci.example.com/build/1', post_build_link='https://ci.example.com/post-build/1')
    BUILDBOT_DETAILS = {'buildbot-master': 'build.webkit.org', 'builder-name': 'Catalina-Release-WK1-Tests', 'build-number': '1', 'buildbot-worker': 'bot2'}

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, test_results=None):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
            MockModelFactory.add_mock_results(self.model, test_results=test_results)
            MockModelFactory.process_results(self.model)

    def init_database_with_details(self, details, redis=StrictRedis, cassandra=CassandraContext):
        test_results = MockModelFactory.layout_test_results()
        test_results['details'] = details
        self.init_database(redis=redis, cassandra=cassandra, test_results=test_results)

    def results_for_test(self):
        results = self.model.test_context.find_by_commit(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-link-charset.html', recent=True,
        )
        return next(iter(results.values()))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_list_all(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.names(suite='layout-tests')
        self.assertEqual(results, [
            'fast/encoding/css-cached-bom.html',
            'fast/encoding/css-charset-default.xhtml',
            'fast/encoding/css-charset.html',
            'fast/encoding/css-link-charset.html',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_list_partial(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.names(suite='layout-tests', test='fast/encoding/css-charset')
        self.assertEqual(results, [
            'fast/encoding/css-charset-default.xhtml',
            'fast/encoding/css-charset.html',
        ])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_all(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.find_by_commit(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-link-charset.html', recent=True,
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(len(next(iter(results.values()))), 5)
        for result in next(iter(results.values())):
            self.assertEqual(result['actual'], Expectations.PASS)
            self.assertEqual(result['expected'], Expectations.PASS)
            self.assertEqual(result['time'], 1.2)
            self.assertEqual(result['modifiers'], '')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_number_of_links(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database_with_details(self.DETAILS_WITH_LINKS, redis=redis, cassandra=cassandra)
        results = self.results_for_test()
        self.assertEqual(len(results), 5)
        for result in results:
            self.assertEqual(sorted(result.keys()), ['actual', 'expected', 'modifiers', 'number_of_links', 'start_time', 'time', 'uuid'])
            self.assertEqual(result['number_of_links'], 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_number_of_links_for_link_alone(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database_with_details(dict(link='https://ci.example.com/build/1'), redis=redis, cassandra=cassandra)
        for result in self.results_for_test():
            self.assertNotIn('number_of_links', result)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_number_of_links_for_buildbot(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database_with_details(self.BUILDBOT_DETAILS, redis=redis, cassandra=cassandra)
        for result in self.results_for_test():
            self.assertNotIn('number_of_links', result)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_commit(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.find_by_commit(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-cached-bom.html', recent=False,
            begin=1601661000,
            end=1601663000,
        )
        self.assertEqual(len(results), 2)
        for results_for_config in results.values():
            self.assertEqual(len(results_for_config), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_find_by_time(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.find_by_start_time(
            configurations=[Configuration(platform='mac', style='Release', flavor='wk1')],
            suite='layout-tests', test='fast/encoding/css-charset.html', recent=False,
            begin_query_time=(time.time() - 60 * 60),
        )

        self.assertEqual(len(results), 1)
        self.assertEqual(len(next(iter(results.values()))), 5)


class TestContextLogsTest(WaitForDockerTestCase):
    KEYSPACE = 'test_context_logs_test_keyspace'
    CONFIGURATIONS = [Configuration(platform='mac', style='Release', flavor='wk1')]
    FAILING_TEST = 'TestWebKitAPI.MessagePortSecurity.SharedWorkerPortStillWorksAfterContextProcessDiesWithCachedClient'
    PASSING_TEST = 'TestWebKitAPI.MessagePortSecurity.MessagePortsSurviveNetworkProcessRestart'
    LOG = (
        'Tools/TestWebKitAPI/Resources/cocoa/MessagePortSecurity.mm:861\n'
        'Value of: messagePortWaitUntilTrue([&] { return repliesContain(webViewB.get(), @"echo:after-relaunch"); })\n'
        '  Actual: false\n'
        'Expected: true\n'
    )
    CRASH_LOG = ''.join(f'{frame}   0x7003b7f83c WebKit::WebProcessProxy::updateSiteForMainFrame(WebCore::Site const&)\n' for frame in range(200))

    def init_database(self, redis=StrictRedis, cassandra=CassandraContext, suite='api-tests', log=LOG, results=None):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=self.KEYSPACE)
            self.model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=self.KEYSPACE, create_keyspace=True))
            MockModelFactory.add_mock_results(self.model, suite=suite, test_results=dict(
                details=dict(link='dummy-link'),
                run_stats=dict(tests_skipped=0),
                results=results or {
                    self.FAILING_TEST: dict(expected='PASS', actual='FAIL', time=1200, log=log),
                    self.PASSING_TEST: dict(expected='PASS', time=1200),
                },
            ))
            MockModelFactory.process_results(self.model, suite=suite)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_log_kept_apart(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        logs = self.model.test_context.find_logs_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual(len(logs), 1)
        self.assertEqual([run['log'] for run in next(iter(logs.values()))], [self.LOG] * 5)

        results = self.model.test_context.find_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual(len(next(iter(results.values()))), 5)
        for result in next(iter(results.values())):
            self.assertEqual(result['actual'], Expectations.FAIL)
            self.assertEqual(result['time'], 1200)
            self.assertNotIn('log', result)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_no_log(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, log='')
        for test in [self.FAILING_TEST, self.PASSING_TEST]:
            self.assertEqual(self.model.test_context.find_logs_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=test), {})
            results = self.model.test_context.find_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=test)
            self.assertEqual(len(next(iter(results.values()))), 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_logs_written_after_the_results_batch(self, redis=StrictRedis, cassandra=CassandraContext):
        insert_row = cassandra.insert_row
        batches_open_for_each_log_row = []

        def insert_row_recording_open_batches(context, table_name, **kwargs):
            if table_name in [TestContext.TestLogsByCommit.__table_name__, TestContext.TestLogsByHash.__table_name__]:
                batches_open_for_each_log_row.append(len(context._batch))
            return insert_row(context, table_name, **kwargs)

        crashes = {f'TestWebKitAPI.LoadWebArchive.Crash{index}': dict(expected='PASS', actual='CRASH', time=1200, log=self.CRASH_LOG[:7000]) for index in range(30)}
        with mock.patch.object(cassandra, 'insert_row', new=insert_row_recording_open_batches):
            self.init_database(redis=redis, cassandra=cassandra, results=crashes)
        self.assertGreaterEqual(len(batches_open_for_each_log_row), len(crashes) + 1)
        self.assertEqual(set(batches_open_for_each_log_row), {0})

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_log_kept_apart_in_other_suites(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, suite='webkitpy-tests')
        logs = self.model.test_context.find_logs_by_commit(configurations=self.CONFIGURATIONS, suite='webkitpy-tests', test=self.FAILING_TEST)
        self.assertEqual([run['log'] for run in next(iter(logs.values()))], [self.LOG] * 5)

        results = self.model.test_context.find_by_commit(configurations=self.CONFIGURATIONS, suite='webkitpy-tests', test=self.FAILING_TEST)
        self.assertEqual([(result.get('log'), result.get('has_log')) for result in next(iter(results.values()))], [(None, True)] * 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_result_marked_when_its_log_is_kept(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        results = self.model.test_context.find_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual([result.get('has_log') for result in next(iter(results.values()))], [True] * 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_result_not_marked_without_a_log(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, log='')
        for test in [self.FAILING_TEST, self.PASSING_TEST]:
            results = self.model.test_context.find_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=test)
            self.assertEqual([result.get('has_log') for result in next(iter(results.values()))], [None] * 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_long_log_stored_truncated(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, log=self.CRASH_LOG)
        logs = self.model.test_context.find_logs_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual(next(iter(logs.values()))[0]['log'], TestContext.truncate_log(self.CRASH_LOG))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_log_stored_once_by_hash(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        stored = self.model.test_context._find_results(TestContext.TestLogsByCommit, configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual([run['log'] for run in next(iter(stored.values()))], [TestContext.hash_log(self.LOG)] * 5)

        with self.model.test_context:
            rows = self.model.test_context.cassandra.select_from_table(TestContext.TestLogsByHash.__table_name__)
        self.assertEqual([(row.hash, row.log) for row in rows], [(TestContext.hash_log(self.LOG), self.LOG)])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_short_log_stored_as_is(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, log='Timed out\n')
        stored = self.model.test_context._find_results(TestContext.TestLogsByCommit, configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual([run['log'] for run in next(iter(stored.values()))], ['Timed out\n'] * 5)

        with self.model.test_context:
            self.assertEqual(self.model.test_context.cassandra.select_from_table(TestContext.TestLogsByHash.__table_name__), [])
        logs = self.model.test_context.find_logs_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual([run['log'] for run in next(iter(logs.values()))], ['Timed out\n'] * 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_log_that_looks_like_a_hash_kept(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra, log='0123456789abcdef' * 2)
        stored = self.model.test_context._find_results(TestContext.TestLogsByCommit, configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual([run['log'] for run in next(iter(stored.values()))], ['0123456789abcdef' * 2] * 5)

        logs = self.model.test_context.find_logs_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertEqual([run['log'] for run in next(iter(logs.values()))], ['0123456789abcdef' * 2] * 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_runs_with_different_logs(self, redis=StrictRedis, cassandra=CassandraContext):
        self.init_database(redis=redis, cassandra=cassandra)
        MockModelFactory.add_mock_results(self.model, suite='api-tests', test_results=dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={self.FAILING_TEST: dict(expected='PASS', actual='CRASH', time=1200, log=self.CRASH_LOG)},
        ), timestamp=time.time() - 60)
        MockModelFactory.process_results(self.model, suite='api-tests')

        logs = self.model.test_context.find_logs_by_commit(configurations=self.CONFIGURATIONS, suite='api-tests', test=self.FAILING_TEST)
        self.assertCountEqual([run['log'] for run in next(iter(logs.values()))], [self.LOG] * 5 + [TestContext.truncate_log(self.CRASH_LOG)] * 5)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    def test_hashed_log_outlives_its_rows(self, redis=StrictRedis, cassandra=CassandraContext):
        insert_row = cassandra.insert_row
        ttls = []

        def insert_row_recording_ttl(context, table_name, ttl=None, **kwargs):
            ttls.append((table_name, ttl))
            return insert_row(context, table_name, ttl=ttl, **kwargs)

        with mock.patch.object(cassandra, 'insert_row', new=insert_row_recording_ttl):
            self.init_database(redis=redis, cassandra=cassandra)
        log_ttls = [ttl for table_name, ttl in ttls if table_name == TestContext.TestLogsByCommit.__table_name__]
        hashed_log_ttls = [ttl for table_name, ttl in ttls if table_name == TestContext.TestLogsByHash.__table_name__]
        self.assertEqual(set(hashed_log_ttls), {self.model.test_context.ttl_seconds})
        self.assertLess(max(log_ttls), self.model.test_context.ttl_seconds)

    def test_is_log_hash(self):
        self.assertTrue(TestContext.is_log_hash(TestContext.hash_log(self.LOG)))
        self.assertFalse(TestContext.is_log_hash(self.LOG))
        self.assertFalse(TestContext.is_log_hash(TestContext.hash_log(self.LOG).upper()))

    def test_truncate_log_within_limit(self):
        self.assertEqual(TestContext.truncate_log(self.LOG), self.LOG)
        self.assertEqual(TestContext.truncate_log('x' * TestContext.MAXIMUM_LOG_LENGTH), 'x' * TestContext.MAXIMUM_LOG_LENGTH)

    def test_truncate_log_wide_lines(self):
        self.assertEqual(
            TestContext.truncate_log('\n'.join(['a' * 1000, 'b' * 2000, 'c' * 15000, 'd' * 6000])),
            '\n'.join(['a' * 1000, 'b' * 2000, 'c' * 2575 + ' [12,425 more bytes]', 'd' * 2575 + ' [3,425 more bytes]']),
        )

    def test_truncate_log_keeps_line_within_its_share(self):
        self.assertEqual(
            TestContext.truncate_log('\n'.join(['a' * 2730, 'b' * 15000, 'c' * 15000])),
            '\n'.join(['a' * 2730, 'b' * 2710 + ' [12,290 more bytes]', 'c' * 2710 + ' [12,290 more bytes]']),
        )

    def test_truncate_log_without_line_breaks(self):
        self.assertEqual(TestContext.truncate_log('x' * 10000), 'x' * 8172 + ' [1,828 more bytes]')

    def test_truncate_log_inside_character(self):
        self.assertEqual(TestContext.truncate_log('x' + 'é' * 5000), 'x' + 'é' * 4085 + ' [1,830 more bytes]')

    def test_truncate_log_many_lines(self):
        lines = [f'{index:03}' + 'x' * 57 for index in range(400)]
        lines[0], lines[200] = 'w' * 1000, 'm' * 1000
        self.assertEqual(
            TestContext.truncate_log('\n'.join(lines)),
            '\n'.join(['w' * 200 + ' [800 more bytes]'] + lines[1:64] + ['[17,409 bytes left out]'] + lines[334:]),
        )

    def test_truncate_log_replaces_text_that_is_not_utf8(self):
        self.assertEqual(TestContext.truncate_log('  Actual: \ud800\n'), '  Actual: ?\n')
