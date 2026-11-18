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
from cassandra.cqlengine.models import Model
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

    class SubSuiteResultsByCommit(SuiteContext.SuiteResultsBase):
        __table_name__ = 'sub_suite_results_by_commit'
        sub_suite = columns.Text(partition_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)

    class SubSuiteNameBySuite(Model):
        __table_name__ = 'sub_suite_names_by_suite'
        suite = columns.Text(partition_key=True, required=True)
        sub_suite = columns.Text(primary_key=True, required=True)

    @classmethod
    def sub_suites_for(cls, test):
        name = test.split('?')[0].split('[')[0]
        separators = '/' if '/' in name else '.:'
        return [name[:index] for index, character in enumerate(name) if character in separators and index and name[index - 1] not in separators]

    def __init__(self, *args, **kwargs):
        super(SubSuiteContext, self).__init__('sub-suite-results', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.SubSuiteResultsByCommit)
            self.cassandra.create_table(self.SubSuiteNameBySuite)

    def register(self, configuration, commits, suite, test_results, timestamp=None):
        timestamp = timestamp or time.time()
        if not isinstance(timestamp, datetime):
            timestamp = datetime.utcfromtimestamp(int(timestamp))

        try:
            if not isinstance(suite, str):
                raise TypeError(f'Expected type {str}, got {type(suite)}')

            if isinstance(timestamp, datetime):
                timestamp = calendar.timegm(timestamp.timetuple())

            stats_by_sub_suite = {}

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
                for key, point in SuiteContext.MILDEST_RESULT_COUNTED_AS.items():
                    counts[f'tests_{key}'] = 1 if worst_result <= point else 0
                    counts[f'tests_unexpected_{key}'] = 1 if worst_unexpected_result <= point else 0

                for sub_suite in self.sub_suites_for(test):
                    stats = stats_by_sub_suite.setdefault(sub_suite, dict.fromkeys(counts, 0))
                    for key, value in counts.items():
                        stats[key] += value

            Expectations.iterate_through_nested_results(test_results.get('results'), callback)

            details = {}
            number_of_links = len(CIContext.named_links(test_results.get('details', {})))
            if number_of_links:
                details['number_of_links'] = number_of_links

            uuid = self.commit_context.uuid_for_commits(commits)
            ttl = int((uuid // Commit.UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None

            with self, self.cassandra.batch_query_context():
                for branch in self.commit_context.branch_keys_for_commits(commits):
                    for sub_suite, stats in stats_by_sub_suite.items():
                        self.configuration_context.insert_row_with_configuration(
                            self.SubSuiteResultsByCommit.__table_name__, configuration=configuration, suite=suite,
                            branch=branch, sub_suite=sub_suite, uuid=uuid, ttl=ttl,
                            sdk=configuration.sdk or '?', start_time=timestamp,
                            details=json.dumps(details),
                            stats=json.dumps(stats),
                        )
                for sub_suite in stats_by_sub_suite:
                    self.cassandra.insert_row(self.SubSuiteNameBySuite.__table_name__, suite=suite, sub_suite=sub_suite, ttl=ttl)
        except Exception as e:
            return self.partial_status(e)
        return self.partial_status()

    def trie(self, suite):
        with self:
            listed = [row.sub_suite for row in self.cassandra.select_from_table(self.SubSuiteNameBySuite.__table_name__, suite=suite)]

        nodes = {}
        top_level = {}
        for name in sorted(listed):
            parents = [parent for parent in self.sub_suites_for(name) if parent in nodes]
            siblings = nodes[parents[-1]] if parents else top_level
            siblings[name] = nodes[name] = {}

        return dict(suite=suite, sub_suites=top_level, max_selected=self.MAX_SUB_SUITES)

    def find_by_commit(
            self, configurations, suite, sub_suite, recent=True,
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
                    self.SubSuiteResultsByCommit.__table_name__, configurations=[configuration], recent=recent,
                    suite=suite, sub_suite=sub_suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()),
                    start_time__gte=get_time(begin_query_time), start_time__lte=get_time(end_query_time),
                    limit=limit,
                ).items()})
            return result
