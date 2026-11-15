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

import calendar
import json
import time

from cassandra.cqlengine import columns
from datetime import datetime
from resultsdbpy.model.ci_context import CIContext
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.suite_context import SuiteContext
from resultsdbpy.model.test_context import Expectations
from resultsdbpy.model.upload_context import UploadCallbackContext
from webkitscmpy import Commit


class SubSuiteContext(UploadCallbackContext):
    DEFAULT_LIMIT = 100
    MAX_SUB_SUITES = 10
    COUNTS = [
        'tests_run', 'tests_failed', 'tests_timedout', 'tests_crashed',
        'tests_unexpected_failed', 'tests_unexpected_timedout', 'tests_unexpected_crashed',
    ]

    DOTTED_SUITES = ['api-tests', 'webkitpy-tests']

    class SubSuiteResultsBase(SuiteContext.SuiteResultsBase):
        sub_suite = columns.Text(partition_key=True, required=True)

        def unpack(self):
            result = super().unpack()
            result['stats'] = {key: result['stats'].get(key, 0) for key in SubSuiteContext.COUNTS}
            result['details'].setdefault('number_of_flaky_tests', 0)
            return result

    class SubSuiteResultsByCommit(SubSuiteResultsBase):
        __table_name__ = 'sub_suite_results_by_commit'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)

    class SubSuiteResultsByStartTime(SubSuiteResultsBase):
        __table_name__ = 'sub_suite_results_by_start_time'
        start_time = columns.DateTime(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True)

    @classmethod
    def sub_suites_for(cls, suite, test):
        if suite not in cls.DOTTED_SUITES:
            path = test.split('?')[0].split('[')[0]
            return [path[:index] for index, character in enumerate(path) if character == '/' and index]

        result = []
        depth = 0
        binary_separator = test.find(':')
        for index, character in enumerate(test):
            if character == '(':
                depth += 1
            elif character == ')':
                depth -= 1
            elif not depth and (character in './' or index == binary_separator) and index and test[index - 1] not in './:':
                result.append(test[:index])
        return result

    def __init__(self, *args, **kwargs):
        super(SubSuiteContext, self).__init__('sub-suite-results', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.SubSuiteResultsByCommit)
            self.cassandra.create_table(self.SubSuiteResultsByStartTime)

    def register(self, configuration, commits, suite, test_results, timestamp=None):
        timestamp = timestamp or time.time()
        if not isinstance(timestamp, datetime):
            timestamp = datetime.utcfromtimestamp(int(timestamp))

        try:
            if not isinstance(suite, str):
                raise TypeError(f'Expected type {str}, got {type(suite)}')

            if isinstance(timestamp, datetime):
                timestamp = calendar.timegm(timestamp.timetuple())

            failure_trigger_points = dict(
                failed=Expectations.STRING_TO_STATE_ID[Expectations.ERROR],
                timedout=Expectations.STRING_TO_STATE_ID[Expectations.TIMEOUT],
                crashed=Expectations.STRING_TO_STATE_ID[Expectations.CRASH],
            )
            stats_by_sub_suite = {}
            details_by_sub_suite = {}
            sub_suites_with_own_tests = set()

            def callback(test, result):
                actual_results = Expectations.string_to_state_ids(result.get('actual', ''))
                expected_results = set(Expectations.string_to_state_ids(result.get('expected', '')))

                if Expectations.STRING_TO_STATE_ID[Expectations.FAIL] in expected_results:
                    expected_results.add(Expectations.STRING_TO_STATE_ID[Expectations.TEXT])
                    expected_results.add(Expectations.STRING_TO_STATE_ID[Expectations.AUDIO])
                    expected_results.add(Expectations.STRING_TO_STATE_ID[Expectations.IMAGE])

                worst_result = min(actual_results)
                unexpected_results = set(actual_results) - expected_results
                worst_unexpected_result = min(unexpected_results or [Expectations.STRING_TO_STATE_ID[Expectations.PASS]])

                counts = dict(tests_run=1)
                for key, point in failure_trigger_points.items():
                    counts[f'tests_{key}'] = 1 if worst_result <= point else 0
                    counts[f'tests_unexpected_{key}'] = 1 if worst_unexpected_result <= point else 0
                is_flaky = (('flakiness_num_tries' in result) and ('flakiness_num_passes' in result) and
                            result['flakiness_num_passes'] < result['flakiness_num_tries'])

                sub_suites = self.sub_suites_for(suite, test)
                if sub_suites:
                    sub_suites_with_own_tests.add(sub_suites[-1])
                for sub_suite in sub_suites:
                    stats = stats_by_sub_suite.setdefault(sub_suite, dict.fromkeys(counts, 0))
                    for key, value in counts.items():
                        stats[key] += value
                    details = details_by_sub_suite.setdefault(sub_suite, dict(number_of_flaky_tests=0))
                    details['number_of_flaky_tests'] += 1 if is_flaky else 0

            Expectations.iterate_through_nested_results(test_results.get('results'), callback)

            number_of_links = len(CIContext.named_links(test_results.get('details', {})))
            if number_of_links:
                for details in details_by_sub_suite.values():
                    details['number_of_links'] = number_of_links

            uuid = self.commit_context.uuid_for_commits(commits)
            ttl = int((uuid // Commit.UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None

            with self, self.cassandra.batch_query_context():
                for branch in self.commit_context.branch_keys_for_commits(commits):
                    for sub_suite, stats in stats_by_sub_suite.items():
                        for table in [self.SubSuiteResultsByCommit, self.SubSuiteResultsByStartTime]:
                            self.configuration_context.insert_row_with_configuration(
                                table.__table_name__, configuration=configuration, suite=suite,
                                branch=branch, sub_suite=sub_suite, uuid=uuid, ttl=ttl,
                                sdk=configuration.sdk or '?', start_time=timestamp,
                                details=json.dumps({key: value for key, value in details_by_sub_suite[sub_suite].items() if value}),
                                stats=json.dumps({key: value for key, value in stats.items() if value}),
                            )

            self._remember_newest_runs(suite, list(stats_by_sub_suite), sub_suites_with_own_tests, timestamp)
        except Exception as e:
            return self.partial_status(e)
        return self.partial_status()

    def _remember_newest_runs(self, suite, sub_suites, sub_suites_with_own_tests, start_time):
        redis = self.configuration_context.redis
        if sub_suites:
            redis.zadd(f'sub_suites:{suite}', {sub_suite: start_time for sub_suite in sub_suites})
        if sub_suites_with_own_tests:
            redis.zadd(f'sub_suites_with_own_tests:{suite}', {sub_suite: start_time for sub_suite in sub_suites_with_own_tests})
        if self.ttl_seconds:
            redis.zremrangebyscore(f'sub_suites:{suite}', '-inf', time.time() - self.ttl_seconds)
            redis.zremrangebyscore(f'sub_suites_with_own_tests:{suite}', '-inf', time.time() - self.ttl_seconds)
        redis.set(f'sub_suites_newest_run:{suite}', start_time, ex=int(self.ttl_seconds) if self.ttl_seconds else None)

    def tree(self, suite):
        redis = self.configuration_context.redis
        oldest_start_time = time.time() - self.ttl_seconds if self.ttl_seconds else '-inf'
        listed = {
            sub_suite.decode('utf-8') for sub_suite in redis.zrangebyscore(f'sub_suites:{suite}', oldest_start_time, '+inf')
        }
        with_own_tests = {
            sub_suite.decode('utf-8') for sub_suite in redis.zrangebyscore(f'sub_suites_with_own_tests:{suite}', oldest_start_time, '+inf')
        }

        nodes = {}
        top_level = []
        for name in sorted(listed):
            node = dict(name=name)
            parents = [parent for parent in self.sub_suites_for(suite, name) if parent in nodes]
            if parents:
                nodes[parents[-1]].setdefault('children', []).append(node)
            else:
                top_level.append(node)
            nodes[name] = node

        for name, node in nodes.items():
            if len(node.get('children', [])) == 1 and name not in with_own_tests:
                node['children'][0]['same_tests_as_parent'] = True

        return dict(
            suite=suite,
            sub_suites=top_level,
            has_runs=bool(top_level) or redis.get(f'sub_suites_newest_run:{suite}') is not None,
            max_selected=self.MAX_SUB_SUITES,
        )

    def _find_results(
            self, table, configurations, suite, sub_suite, recent=True,
            branch=None, begin=None, end=None,
            begin_query_time=None, end_query_time=None,
            limit=DEFAULT_LIMIT,
    ):
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')
        if not isinstance(sub_suite, str):
            raise TypeError(f'Expected type {str}, got {type(sub_suite)}')

        def get_time(time):
            if isinstance(time, datetime):
                return time
            elif time:
                return datetime.utcfromtimestamp(int(time))
            return None

        with self:
            result = {}
            for configuration in configurations:
                result.update({config: [value.unpack() for value in values] for config, values in self.configuration_context.select_from_table_with_configurations(
                    table.__table_name__, configurations=[configuration], recent=recent,
                    suite=suite, sub_suite=sub_suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()),
                    start_time__gte=get_time(begin_query_time), start_time__lte=get_time(end_query_time),
                    limit=limit,
                ).items()})
            return result

    def find_by_commit(self, *args, **kwargs):
        return self._find_results(self.SubSuiteResultsByCommit, *args, **kwargs)

    def find_by_start_time(self, *args, **kwargs):
        return self._find_results(self.SubSuiteResultsByStartTime, *args, **kwargs)
