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


class Expectations:
    # These are ordered by priority, meaning that a test which both crashes and has
    # a warning should be considered to have crashed.
    STATE_ID_TO_STRING = collections.OrderedDict()
    STATE_ID_TO_STRING[0x00] = 'CRASH'
    STATE_ID_TO_STRING[0x08] = 'TIMEOUT'
    STATE_ID_TO_STRING[0x10] = 'IMAGE'
    STATE_ID_TO_STRING[0x18] = 'AUDIO'
    STATE_ID_TO_STRING[0x20] = 'TEXT'
    STATE_ID_TO_STRING[0x28] = 'FAIL'
    STATE_ID_TO_STRING[0x30] = 'ERROR'
    STATE_ID_TO_STRING[0x38] = 'WARNING'
    STATE_ID_TO_STRING[0x40] = 'PASS'

    STRING_TO_STATE_ID = {string: id for id, string in STATE_ID_TO_STRING.items()}

    CRASH, TIMEOUT, IMAGE, AUDIO, TEXT, FAIL, ERROR, WARNING, PASS = STATE_ID_TO_STRING.values()

    @classmethod
    def string_to_state_ids(cls, string):
        result = set([elm for elm in [cls.STRING_TO_STATE_ID.get(str) for str in string.split(' ')] if elm is not None])
        return sorted(result) or [cls.STRING_TO_STATE_ID[cls.PASS]]

    @classmethod
    def state_ids_to_string(cls, state_ids):
        if isinstance(state_ids, str):
            return ' '.join([cls.STATE_ID_TO_STRING[ord(state)] for state in sorted(state_ids)])
        return ' '.join([cls.STATE_ID_TO_STRING[int(state)] for state in sorted(state_ids)])

    @classmethod
    def iterate_through_nested_results(cls, results, callback=lambda test, results: None):
        def recurse(partial_test, results):
            potential_base_case = True
            for key, value in results.items():
                if isinstance(value, dict):
                    potential_base_case = False
                    recurse(partial_test + '/' + key, value)
                elif not potential_base_case:
                    raise ValueError('Incorrectly formatted nested results dictionary')

            if potential_base_case:
                # If we don't have a dictionary of dictionaries, that means this is a leaf node
                callback(partial_test, results)

        for key, value in results.items():
            recurse(key, value)


class TestContext(UploadCallbackContext):
    DEFAULT_LIMIT = 100

    class TestResultsBase(ClusteredByConfiguration):
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        test = columns.Text(partition_key=True, required=True)
        expected = columns.Blob(required=True)
        actual = columns.Blob(required=True)
        details = columns.Text(required=False)

        def unpack(self):
            result = dict(
                uuid=self.uuid,
                start_time=calendar.timegm(self.start_time.timetuple()),
                expected=Expectations.state_ids_to_string(self.expected),
                actual=Expectations.state_ids_to_string(self.actual),
            )
            for key, value in (json.loads(self.details) if self.details else {}).items():
                if key in result:
                    continue
                result[key] = value
            return result

    class TestResultsByCommit(TestResultsBase):
        __table_name__ = 'test_results_by_commit'
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)

    class TestResultsByStartTime(TestResultsBase):
        __table_name__ = 'test_results_by_start_time'
        start_time = columns.DateTime(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True)

    class TestNameBySuite(Model):
        __table_name__ = 'test_names_by_suite'
        suite = columns.Text(partition_key=True, required=True)
        test = columns.Text(primary_key=True, required=True)

    def __init__(self, *args, **kwargs):
        super(TestContext, self).__init__('test-results', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.TestResultsByCommit)
            self.cassandra.create_table(self.TestResultsByStartTime)
            self.cassandra.create_table(self.TestNameBySuite)

    def names(self, suite, test=None, limit=DEFAULT_LIMIT):
        with self:
            if test:
                # FIXME: SASI indecies are the cannoical way to solve this problem, but require Cassandra 3.4 which
                # hasn't been deployed to our datacenters yet. This works for commits, but is less transparent.
                return [model.test for model in self.cassandra.select_from_table(
                    self.TestNameBySuite.__table_name__, limit=limit, suite=suite,
                    test__gte=test, test__lte=(test + '~'),
                )]

            return [model.test for model in self.cassandra.select_from_table(
                self.TestNameBySuite.__table_name__, limit=limit, suite=suite,
            )]

    def register(self, configuration, commits, suite, test_results, timestamp=None):
        timestamp = timestamp or time.time()
        if not isinstance(timestamp, datetime):
            timestamp = datetime.utcfromtimestamp(int(timestamp))

        try:
            if not isinstance(suite, str):
                raise TypeError(f'Expected type {str}, got {type(suite)}')

            if isinstance(timestamp, datetime):
                timestamp = calendar.timegm(timestamp.timetuple())

            with self:
                uuid = self.commit_context.uuid_for_commits(commits)
                ttl = int((uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None

                def callback(test, result, branch):
                    self.cassandra.insert_row(self.TestNameBySuite.__table_name__, suite=suite, test=test, ttl=ttl)

                    args_to_write = dict(
                        actual=bytearray(Expectations.string_to_state_ids(result.get('actual', ''))),
                        expected=bytearray(Expectations.string_to_state_ids(result.get('expected', ''))),
                        details=json.dumps({key: value for key, value in result.items() if key not in ['actual', 'expected']}),
                    )
                    for table in [self.TestResultsByCommit, self.TestResultsByStartTime]:
                        self.configuration_context.insert_row_with_configuration(
                            table.__table_name__, configuration=configuration, suite=suite,
                            branch=branch, uuid=uuid, ttl=ttl,
                            test=test, sdk=configuration.sdk or '?', start_time=timestamp,
                            **args_to_write)

                with self.cassandra.batch_query_context():
                    for branch in self.commit_context.branch_keys_for_commits(commits):
                        Expectations.iterate_through_nested_results(
                            test_results.get('results'),
                            lambda test, result: callback(test, result, branch=branch),
                        )

        except Exception as e:
            return self.partial_status(e)
        return self.partial_status()

    def _find_results(
            self, table, configurations, suite, test, recent=True,
            branch=None, begin=None, end=None,
            begin_query_time=None, end_query_time=None,
            limit=DEFAULT_LIMIT,
    ):
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')
        if not isinstance(test, str):
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
                    suite=suite, test=test, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()),
                    start_time__gte=get_time(begin_query_time), start_time__lte=get_time(end_query_time),
                    limit=limit,
                ).items()})
            return result

    def find_by_commit(self, *args, **kwargs):
        return self._find_results(self.TestResultsByCommit, *args, **kwargs)

    def find_by_start_time(self, *args, **kwargs):
        return self._find_results(self.TestResultsByStartTime, *args, **kwargs)
