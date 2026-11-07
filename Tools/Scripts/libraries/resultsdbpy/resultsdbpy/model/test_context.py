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
import hashlib
import json
import re
import time

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model
from datetime import datetime
from resultsdbpy.model.ci_context import CIContext
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ClusteredByConfiguration
from resultsdbpy.model.upload_context import UploadCallbackContext
from webkitscmpy import Commit


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

    FAILURE_TYPES = [WARNING, ERROR, TIMEOUT, CRASH]
    STATS_FAILURE_TYPES = ['warning', 'failed', 'timedout', 'crashed']

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

    @classmethod
    def get_test_result_status(cls, test_result_json, will_filter_expected=False):
        failure_type = None
        failure_number = None
        if test_result_json.get('stats', None):
            if test_result_json.get('start_time', None):
                failure_number = test_result_json['stats']['tests{}failed'.format('_unexpected_' if will_filter_expected else '_')]
            else:
                failure_number = test_result_json['stats']['worst_tests{}failed'.format('_unexpected_' if will_filter_expected else '_')]
            if 'worst_tests_run' in test_result_json and test_result_json['stats']['worst_tests_run'] <= 1:
                failure_number = None

            for type in Expectations.STATS_FAILURE_TYPES:
                if test_result_json['stats'].get('tests{}{}'.format('_unexpected_' if will_filter_expected else '_', type.lower()), 0) > 0:
                    failure_type = type
        else:
            resultId = Expectations.string_to_state_ids(test_result_json['actual'])
            if will_filter_expected:
                resultId = Expectations.string_to_state_ids(cls.unexpected_results(test_result_json['actual'], test_result_json['expected']))
            for type in Expectations.FAILURE_TYPES:
                if Expectations.string_to_state_ids(type) >= resultId:
                    failure_type = type
        return failure_type, failure_number

    @classmethod
    def unexpected_results(cls, results, expectations):
        """
        This function is a python translation for expectations.js Expectation.unexpectedResults
        """
        r = results.split('.')
        for expectation in expectations.split(' '):
            try:
                i = r.index(expectation)
                r = r[:i] + r[i + 1:]
            except ValueError:
                pass
            if expectation == cls.FAIL:
                for expectation in [cls.TEXT, cls.AUDIO, cls.IMAGE]:
                    try:
                        i = r.index(expectation)
                        r = r[:i] + r[i + 1:]
                    except ValueError:
                        pass
        result = cls.PASS
        for candidate in r:
            if Expectations.string_to_state_ids(candidate) < Expectations.string_to_state_ids(result):
                result = candidate
        return result


class TestContext(UploadCallbackContext):
    DEFAULT_LIMIT = 100
    MAXIMUM_LOG_LENGTH = 8 * 1024
    MINIMUM_LOG_LINE_LENGTH = 200
    LOG_LINE_NOTE = ' [{:,} more bytes]'
    LOG_LEFT_OUT_NOTE = '[{:,} bytes left out]'
    LOG_HASH_LENGTH = 32

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

    class TestLogsByCommit(ClusteredByConfiguration):
        __table_name__ = 'test_logs_by_commit'
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        test = columns.Text(partition_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        start_time = columns.DateTime(primary_key=True, required=True)
        log = columns.Text(required=True)

        def unpack(self):
            return dict(
                uuid=self.uuid,
                start_time=calendar.timegm(self.start_time.timetuple()),
                log=self.log,
            )

    class TestLogsByHash(Model):
        __table_name__ = 'test_logs_by_hash'
        hash = columns.Text(partition_key=True, required=True)
        log = columns.Text(required=True)

    def __init__(self, *args, **kwargs):
        super(TestContext, self).__init__('test-results', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.TestResultsByCommit)
            self.cassandra.create_table(self.TestResultsByStartTime)
            self.cassandra.create_table(self.TestNameBySuite)
            self.cassandra.create_table(self.TestLogsByCommit)
            self.cassandra.create_table(self.TestLogsByHash)

    @classmethod
    def truncate_log(cls, log):
        encoded = log.encode('utf-8', 'replace')
        if len(encoded) <= cls.MAXIMUM_LOG_LENGTH:
            return encoded.decode('utf-8')

        lines = encoded.split(b'\n')
        kept = cls.truncate_log_lines(lines)
        truncated = b'\n'.join(kept)
        if len(truncated) > cls.MAXIMUM_LOG_LENGTH:
            truncated = b'\n'.join(cls.keep_log_ends(lines, kept))
        return truncated.decode('utf-8')

    @classmethod
    def truncate_log_lines(cls, lines):
        room = cls.MAXIMUM_LOG_LENGTH - (len(lines) - 1)
        lengths = sorted(len(line) for line in lines)
        for index, length in enumerate(lengths):
            lines_left = len(lengths) - index
            share = room // lines_left
            if length > share:
                notes = sum(len(cls.LOG_LINE_NOTE.format(cut_length)) for cut_length in lengths[index:])
                ceiling = max((room - notes) // lines_left, cls.MINIMUM_LOG_LINE_LENGTH)
                return [cls.truncate_log_line(line, ceiling) if len(line) > share else line for line in lines]
            room -= length
        return lines

    @classmethod
    def truncate_log_line(cls, line, length):
        kept = line[:length].decode('utf-8', 'ignore').encode('utf-8')
        truncated = kept + cls.LOG_LINE_NOTE.format(len(line) - len(kept)).encode('utf-8')
        return truncated if len(truncated) < len(line) else line

    @classmethod
    def keep_log_ends(cls, lines, kept):
        room = cls.MAXIMUM_LOG_LENGTH - len(cls.LOG_LEFT_OUT_NOTE.format(len(b'\n'.join(lines))))
        top, top_length = 0, 0
        while top_length + len(kept[top]) + 1 <= room // 2:
            top_length += len(kept[top]) + 1
            top += 1
        bottom, bottom_length = len(kept), 0
        while bottom_length + len(kept[bottom - 1]) + 1 <= room - room // 2:
            bottom -= 1
            bottom_length += len(kept[bottom]) + 1
        left_out = cls.LOG_LEFT_OUT_NOTE.format(len(b'\n'.join(lines[top:bottom]))).encode('utf-8')
        return kept[:top] + [left_out] + kept[bottom:]

    @classmethod
    def hash_log(cls, log):
        return hashlib.md5(log.encode('utf-8')).hexdigest()

    @classmethod
    def is_log_hash(cls, log):
        return bool(re.fullmatch(f'[0-9a-f]{{{cls.LOG_HASH_LENGTH}}}', log))

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

            number_of_links = len(CIContext.named_links(test_results.get('details', {})))

            with self:
                uuid = self.commit_context.uuid_for_commits(commits)
                ttl = int((uuid // Commit.UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None
                log_rows = []
                logs_by_hash = {}

                def callback(test, result, branch):
                    self.cassandra.insert_row(self.TestNameBySuite.__table_name__, suite=suite, test=test, ttl=ttl)
                    log = result.get('log')
                    details = {key: value for key, value in result.items() if key not in ['actual', 'expected', 'log']}
                    if number_of_links:
                        details['number_of_links'] = number_of_links
                    if isinstance(log, str) and log:
                        details['has_log'] = True
                        log = self.truncate_log(log)
                        if len(log) > self.LOG_HASH_LENGTH:
                            log_hash = self.hash_log(log)
                            logs_by_hash[log_hash] = log
                            log = log_hash
                        log_rows.append(dict(test=test, branch=branch, log=log))

                    args_to_write = dict(
                        actual=bytearray(Expectations.string_to_state_ids(result.get('actual', ''))),
                        expected=bytearray(Expectations.string_to_state_ids(result.get('expected', ''))),
                        details=json.dumps(details),
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

                for log_hash, log in logs_by_hash.items():
                    self.cassandra.insert_row(self.TestLogsByHash.__table_name__, hash=log_hash, log=log, ttl=self.ttl_seconds)

                for log_row in log_rows:
                    self.configuration_context.insert_row_with_configuration(
                        self.TestLogsByCommit.__table_name__, configuration=configuration, suite=suite,
                        uuid=uuid, ttl=ttl, sdk=configuration.sdk or '?', start_time=timestamp,
                        **log_row,
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

    def find_logs_by_commit(self, *args, **kwargs):
        results = self._find_results(self.TestLogsByCommit, *args, **kwargs)
        log_hashes = {run['log'] for runs in results.values() for run in runs if self.is_log_hash(run['log'])}
        if not log_hashes:
            return results

        with self:
            logs_by_hash = {row.hash: row.log for row in self.cassandra.select_from_table(
                self.TestLogsByHash.__table_name__, hash__in=list(log_hashes),
            )}
        return {
            config: [dict(run, log=logs_by_hash.get(run['log'], run['log'])) for run in runs]
            for config, runs in results.items()
        }
