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
import collections
import json
import time

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model
from datetime import datetime
from resultsdbpy.controller.commit import Commit
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ClusteredByConfiguration
from resultsdbpy.model.upload_context import UploadCallbackContext
from resultsdbpy.model.test_context import Expectations


class FailureContext(UploadCallbackContext):
    DEFAULT_LIMIT = 100

    class TestFailuresBase(ClusteredByConfiguration):
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        tests = columns.Text(required=True)

        def unpack(self):
            results = json.loads(self.tests) if self.tests else {}
            for test in results.keys():
                results[test] = Expectations.state_ids_to_string([results[test]])
            results['uuid'] = self.uuid
            results['start_time'] = calendar.timegm(self.start_time.timetuple())
            return results

    class TestFailuresByCommit(TestFailuresBase):
        __table_name__ = 'test_failures_by_commit'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)

    class TestFailuresByStartTime(TestFailuresBase):
        __table_name__ = 'test_failures_by_start_time'
        start_time = columns.DateTime(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True)

    class UnexpectedTestFailuresByCommit(TestFailuresBase):
        __table_name__ = 'unexpected_test_failures_by_commit'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)

    class UnexpectedTestFailuresByStartTime(TestFailuresBase):
        __table_name__ = 'unexpected_test_failures_by_start_time'
        start_time = columns.DateTime(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True)

    def __init__(self, *args, **kwargs):
        super(FailureContext, self).__init__('test-failures', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.TestFailuresByCommit)
            self.cassandra.create_table(self.TestFailuresByStartTime)
            self.cassandra.create_table(self.UnexpectedTestFailuresByCommit)
            self.cassandra.create_table(self.UnexpectedTestFailuresByStartTime)

    def register(self, configuration, commits, suite, test_results, timestamp=None):
        try:
            if not isinstance(suite, str):
                raise TypeError(f'Expected type {str}, got {type(suite)}')

            timestamp = timestamp or time.time()
            if isinstance(timestamp, datetime):
                timestamp = calendar.timegm(timestamp.timetuple())

            with self:
                uuid = self.commit_context.uuid_for_commits(commits)
                ttl = int((uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None

                def callback(test, result, failures, unexpected):
                    failed_result = Expectations.string_to_state_ids(result.get('actual', ''))
                    expected = set(Expectations.string_to_state_ids(result.get('expected', '')))
                    if Expectations.STRING_TO_STATE_ID[Expectations.FAIL] in expected:
                        expected.add(Expectations.STRING_TO_STATE_ID[Expectations.TEXT])
                        expected.add(Expectations.STRING_TO_STATE_ID[Expectations.AUDIO])
                        expected.add(Expectations.STRING_TO_STATE_ID[Expectations.IMAGE])

                    unexpected_result = set(failed_result) - expected
                    if failed_result:
                        worst = min(failed_result)
                        if worst < Expectations.STRING_TO_STATE_ID[Expectations.WARNING]:
                            failures[test] = min(worst, failures.get(test, Expectations.STRING_TO_STATE_ID[Expectations.PASS]))

                    if unexpected_result:
                        worst = min(unexpected_result)
                        if worst < Expectations.STRING_TO_STATE_ID[Expectations.WARNING]:
                            unexpected[test] = min(worst, unexpected.get(test, Expectations.STRING_TO_STATE_ID[Expectations.PASS]))

                with self.cassandra.batch_query_context():
                    for branch in self.commit_context.branch_keys_for_commits(commits):
                        failures = {}
                        unexpected = {}

                        Expectations.iterate_through_nested_results(
                            test_results.get('results'),
                            lambda test, result: callback(test, result, failures=failures, unexpected=unexpected),
                        )

                        for table in [self.TestFailuresByCommit, self.TestFailuresByStartTime]:
                            self.configuration_context.insert_row_with_configuration(
                                table.__table_name__, configuration=configuration, suite=suite,
                                branch=branch, uuid=uuid, ttl=ttl,
                                sdk=configuration.sdk or '?', start_time=timestamp,
                                tests=json.dumps(failures),
                            )

                        for table in [self.UnexpectedTestFailuresByCommit, self.UnexpectedTestFailuresByStartTime]:
                            self.configuration_context.insert_row_with_configuration(
                                table.__table_name__, configuration=configuration, suite=suite,
                                branch=branch, uuid=uuid, ttl=ttl,
                                sdk=configuration.sdk or '?', start_time=timestamp,
                                tests=json.dumps(unexpected),
                            )

        except Exception as e:
            return self.partial_status(e)
        return self.partial_status()

    def _failures(
            self, all_table, unexpected_table, configurations, suite, recent=True,
            branch=None, begin=None, end=None,
            begin_query_time=None, end_query_time=None,
            unexpected=True, collapsed=True, limit=DEFAULT_LIMIT,
    ):
        table = unexpected_table if unexpected else all_table
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str} for suite, got {type(suite)}')

        def get_time(time):
            if isinstance(time, datetime):
                return time
            elif time:
                return datetime.utcfromtimestamp(int(time))
            return None

        with self:
            has_test_runs = False
            if collapsed:
                result = set()
            else:
                result = {}

            for configuration in configurations:
                for config, values in self.configuration_context.select_from_table_with_configurations(
                    table.__table_name__, configurations=[configuration], recent=recent,
                    suite=suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()),
                    start_time__gte=get_time(begin_query_time), start_time__lte=get_time(end_query_time),
                    limit=limit,
                ).items():
                    if collapsed:
                        for value in values:
                            has_test_runs = True
                            for test in value.unpack():
                                if test not in ['uuid', 'start_time']:
                                    result.add(test)
                    else:
                        runs = []
                        for value in values:
                            has_test_runs = True

                            # uuid and start_time are not in the unpacked values
                            unpacked = value.unpack()
                            if len(unpacked) > 2:
                                runs.append(unpacked)
                        if runs:
                            result.update({config: runs})

            return result if has_test_runs else None

    def failures_by_commit(self, *args, **kwargs):
        return self._failures(self.TestFailuresByCommit, self.UnexpectedTestFailuresByCommit, *args, **kwargs)

    def failures_by_start_time(self, *args, **kwargs):
        return self._failures(self.TestFailuresByStartTime, self.UnexpectedTestFailuresByStartTime, *args, **kwargs)
