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

import calendar
import json
import time

from cassandra.cqlengine import columns
from datetime import datetime
from resultsdbpy.controller.commit import Commit
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ClusteredByConfiguration
from resultsdbpy.model.test_context import Expectations
from resultsdbpy.model.upload_context import UploadCallbackContext


class SuiteContext(UploadCallbackContext):
    DEFAULT_LIMIT = 100

    class SuiteResultsBase(ClusteredByConfiguration):
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        details = columns.Text(required=False)
        stats = columns.Text(required=False)

        def unpack(self):
            return dict(
                uuid=self.uuid,
                start_time=calendar.timegm(self.start_time.timetuple()),
                details=json.loads(self.details) if self.details else {},
                stats=json.loads(self.stats) if self.stats else {},
            )

    class SuiteResultsByCommit(SuiteResultsBase):
        __table_name__ = 'suite_results_by_commit'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)

    class SuiteResultsByStartTime(SuiteResultsBase):
        __table_name__ = 'suite_results_by_start_time'
        start_time = columns.DateTime(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True)

    def __init__(self, *args, **kwargs):
        super(SuiteContext, self).__init__('suite-results', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.SuiteResultsByCommit)
            self.cassandra.create_table(self.SuiteResultsByStartTime)

    def register(self, configuration, commits, suite, test_results, timestamp=None):
        timestamp = timestamp or time.time()
        if not isinstance(timestamp, datetime):
            timestamp = datetime.utcfromtimestamp(int(timestamp))

        try:
            if not isinstance(suite, str):
                raise TypeError(f'Expected type {str}, got {type(suite)}')

            if isinstance(timestamp, datetime):
                timestamp = calendar.timegm(timestamp.timetuple())

            run_stats = test_results.get('run_stats', {})
            failure_trigger_points = dict(
                failed=Expectations.STRING_TO_STATE_ID[Expectations.ERROR],
                timedout=Expectations.STRING_TO_STATE_ID[Expectations.TIMEOUT],
                crashed=Expectations.STRING_TO_STATE_ID[Expectations.CRASH],
            )
            run_stats['tests_run'] = 0
            for key in failure_trigger_points.keys():
                run_stats[f'tests_{key}'] = 0
                run_stats[f'tests_unexpected_{key}'] = 0

            def callback(test, result):
                run_stats['tests_run'] += 1
                actual_results = Expectations.string_to_state_ids(result.get('actual', ''))
                expected_results = set(Expectations.string_to_state_ids(result.get('expected', '')))

                # FAIL is a special case, we want to treat tests with TEXT, AUDIO and IMAGE diffs as failures
                if Expectations.STRING_TO_STATE_ID[Expectations.FAIL] in expected_results:
                    expected_results.add(Expectations.STRING_TO_STATE_ID[Expectations.TEXT])
                    expected_results.add(Expectations.STRING_TO_STATE_ID[Expectations.AUDIO])
                    expected_results.add(Expectations.STRING_TO_STATE_ID[Expectations.IMAGE])

                worst_result = min(actual_results)
                unexpected_results = set(actual_results) - expected_results
                worst_unexpected_result = min(unexpected_results or [Expectations.STRING_TO_STATE_ID[Expectations.PASS]])

                for key, point in failure_trigger_points.items():
                    if worst_result <= point:
                        run_stats[f'tests_{key}'] += 1
                    if worst_unexpected_result <= point:
                        run_stats[f'tests_unexpected_{key}'] += 1

            Expectations.iterate_through_nested_results(test_results.get('results'), callback)

            uuid = self.commit_context.uuid_for_commits(commits)
            ttl = int((uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None

            with self, self.cassandra.batch_query_context():
                for branch in self.commit_context.branch_keys_for_commits(commits):
                    for table in [self.SuiteResultsByCommit, self.SuiteResultsByStartTime]:
                        self.configuration_context.insert_row_with_configuration(
                            table.__table_name__, configuration=configuration, suite=suite,
                            branch=branch, uuid=uuid, ttl=ttl,
                            sdk=configuration.sdk or '?', start_time=timestamp,
                            details=json.dumps(test_results.get('details', {})),
                            stats=json.dumps(run_stats),
                        )
        except Exception as e:
            return self.partial_status(e)
        return self.partial_status()

    def _find_results(
            self, table, configurations, suite, recent=True,
            branch=None, begin=None, end=None,
            begin_query_time=None, end_query_time=None,
            limit=DEFAULT_LIMIT,
    ):
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')

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
                    suite=suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()),
                    start_time__gte=get_time(begin_query_time), start_time__lte=get_time(end_query_time),
                    limit=limit,
                ).items()})
            return result

    def find_by_commit(self, *args, **kwargs):
        return self._find_results(self.SuiteResultsByCommit, *args, **kwargs)

    def find_by_start_time(self, *args, **kwargs):
        return self._find_results(self.SuiteResultsByStartTime, *args, **kwargs)
