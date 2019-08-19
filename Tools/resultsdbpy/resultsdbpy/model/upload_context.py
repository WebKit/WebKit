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
import io
import json
import time
import zipfile

from cassandra.cqlengine import columns
from collections import defaultdict
from datetime import datetime
from resultsdbpy.controller.commit import Commit
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ClusteredByConfiguration


class UploadContext(object):
    QUEUE_NAME = 'upload_queue'
    PROCESS_TIMEOUT = 24 * 60 * 60
    MAX_ATTEMPTS = 3
    RETRY_TIME = 5 * 60  # After 5 minutes, re-try a task even if it's in-flight
    MAX_TASKS_IN_SCAN = 10

    class SuitesByConfiguration(ClusteredByConfiguration):
        __table_name__ = 'suites_by_configuration'
        suite = columns.Text(primary_key=True, required=True)

    class UploadsByConfiguration(ClusteredByConfiguration):
        __table_name__ = 'uploads_by_configuration_01'
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        sdk = columns.Text(primary_key=True, required=True)
        commits = columns.Blob(required=True)
        test_results = columns.Blob(required=True)
        time_uploaded = columns.DateTime(required=True)
        upload_version = columns.Integer(required=True)

        def unpack(self):
            return dict(
                commits=[Commit.from_json(element) for element in json.loads(UploadContext.from_zip(bytearray(self.commits)))],
                sdk=None if self.sdk == '?' else self.sdk,
                test_results=json.loads(UploadContext.from_zip(bytearray(self.test_results))),
                timestamp=calendar.timegm(self.time_uploaded.timetuple()),
                version=self.upload_version,
            )

    def __init__(self, configuration_context, commit_context, ttl_seconds=None, async_processing=False):
        self.ttl_seconds = ttl_seconds
        self.configuration_context = configuration_context
        self.commit_context = commit_context
        self.cassandra = self.configuration_context.cassandra
        self._process_upload_callbacks = defaultdict(dict)
        with self:
            self.cassandra.create_table(self.SuitesByConfiguration)
            self.cassandra.create_table(self.UploadsByConfiguration)

        self._async_processing = async_processing
        self.redis = self.configuration_context.redis

    def __enter__(self):
        self.configuration_context.__enter__()
        self.commit_context.__enter__()

    def __exit__(self, *args, **kwargs):
        self.commit_context.__exit__(*args, **kwargs)
        self.configuration_context.__exit__(*args, **kwargs)

    def register_upload_callback(self, name, callback, suite=None):
        # If no suite is specified, all uploads from all suites will trigger the callback
        self._process_upload_callbacks[suite][name] = callback

    @classmethod
    def to_zip(cls, value, archive_name='archive'):
        if not isinstance(value, str):
            raise TypeError(f'Expected type {str}, got {type(value)}')

        compressed_file = io.BytesIO()
        with zipfile.ZipFile(compressed_file, mode='w', compression=zipfile.ZIP_DEFLATED) as zip_file:
            zip_file.writestr(archive_name, value)
        return bytearray(compressed_file.getvalue())

    @classmethod
    def from_zip(cls, value, archive_name='archive'):
        if not isinstance(value, bytearray):
            raise TypeError(f'Expected type {bytearray}, got {type(value)}')

        compressed_file = io.BytesIO()
        compressed_file.write(value)
        with zipfile.ZipFile(compressed_file, mode='r') as zip_file:
            return zip_file.read(archive_name).decode('utf-8')

    def synchronously_process_test_results(self, configuration, commits, suite, test_results, timestamp=None):
        timestamp = timestamp or time.time()

        # Allows partial errors to be forwarded back to the caller
        result = {}
        for name, callback in self._process_upload_callbacks[suite].items():
            result[name] = callback(configuration=configuration, commits=commits, suite=suite, test_results=test_results, timestamp=timestamp)
        for name, callback in self._process_upload_callbacks[None].items():
            result[name] = callback(configuration=configuration, commits=commits, suite=suite, test_results=test_results, timestamp=timestamp)
        return result

    def _find_job_with_attempts(self):
        now = int(time.time())
        are_jobs_left = False
        will_attempt = 0

        with self.redis.lock(name=f'lock_{self.QUEUE_NAME}', timeout=60):
            for key in self.redis.scan_iter(match=f'{self.QUEUE_NAME}*', count=self.MAX_TASKS_IN_SCAN):
                are_jobs_left = True
                key = key.decode('utf-8')
                try:
                    value = json.loads(self.redis.get(key))
                    if now > value.get('started_processing', 0) + self.RETRY_TIME:
                        will_attempt = value.get('attempts', 0) + 1
                except Exception:
                    will_attempt = 1

                if will_attempt:
                    self.redis.set(
                        key,
                        json.dumps(dict(started_processing=now, attempts=will_attempt)),
                        ex=self.PROCESS_TIMEOUT,
                    )
                    return are_jobs_left, key, will_attempt

        return are_jobs_left, None, None

    def _do_job_for_key(self, key, attempts=1):
        job_complete = False
        try:
            data = json.loads(self.redis.get(f'data_for_{key}'))
            self.synchronously_process_test_results(
                configuration=Configuration.from_json(data['configuration']),
                commits=[Commit.from_json(commit_json) for commit_json in data['commits']],
                suite=data['suite'],
                timestamp=data['timestamp'],
                test_results=data['test_results'],
            )
            job_complete = True
        finally:
            if job_complete or attempts >= self.MAX_ATTEMPTS:
                self.redis.delete(key)
                self.redis.delete(f'data_for_{key}')
            else:
                self.redis.set(
                    key,
                    json.dumps(dict(started_processing=0, attempts=attempts)),
                    ex=self.PROCESS_TIMEOUT,
                )

    def do_processing_work(self):
        jobs_left = True

        while jobs_left:
            jobs_left, key, attempts = self._find_job_with_attempts()

            if key:
                self._do_job_for_key(key, attempts=attempts)
            elif jobs_left:
                time.sleep(10)  # There are jobs, but other workers are processing them.

    def process_test_results(self, configuration, commits, suite, test_results, timestamp=None):
        timestamp = timestamp or time.time()

        if not self._async_processing:
            return self.synchronously_process_test_results(configuration, commits, suite, test_results=test_results, timestamp=timestamp)

        for branch in self.commit_context.branch_keys_for_commits(commits):
            hash_key = hash(configuration) ^ hash(branch) ^ hash(self.commit_context.uuid_for_commits(commits)) ^ hash(
                suite)
            self.redis.set(
                f'{self.QUEUE_NAME}:{hash_key}',
                json.dumps(dict(started_processing=0, attempts=0)),
                ex=self.PROCESS_TIMEOUT,
            )
            self.redis.set(
                f'data_for_{self.QUEUE_NAME}:{hash_key}',
                json.dumps(dict(
                    configuration=Configuration.Encoder().default(configuration),
                    suite=suite,
                    commits=Commit.Encoder().default(commits),
                    timestamp=timestamp,
                    test_results=test_results,
                )),
                ex=self.PROCESS_TIMEOUT,
            )
        return {key: dict(status='Queued') for key in list(self._process_upload_callbacks[suite].keys()) + list(self._process_upload_callbacks[None].keys())}

    def upload_test_results(self, configuration, commits, suite, test_results, timestamp=None, version=0):
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')
        for commit in commits:
            if not isinstance(commit, Commit):
                raise TypeError(f'Expected type {Commit}, got {type(commit)}')
        if len(commits) < 1:
            raise ValueError('Each test result must have at least 1 associated commit')
        if not isinstance(test_results, dict):
            raise TypeError(f'Expected type {dict}, got {type(test_results)}')
        timestamp = timestamp or time.time()
        if not isinstance(timestamp, datetime):
            timestamp = datetime.utcfromtimestamp(int(timestamp))

        uuid = self.commit_context.uuid_for_commits(commits)
        branches = self.commit_context.branch_keys_for_commits(commits)

        with self:
            self.configuration_context.register_configuration(configuration, timestamp=timestamp)

            for branch in branches:
                self.configuration_context.insert_row_with_configuration(
                    self.SuitesByConfiguration.__table_name__, configuration, suite=suite,
                    ttl=int((uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None,
                )
                self.configuration_context.insert_row_with_configuration(
                    self.UploadsByConfiguration.__table_name__, configuration=configuration,
                    suite=suite, branch=branch, uuid=uuid, sdk=configuration.sdk or '?', time_uploaded=timestamp,
                    commits=self.to_zip(json.dumps(commits, cls=Commit.Encoder)),
                    test_results=self.to_zip(json.dumps(test_results)),
                    upload_version=version,
                    ttl=int((uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER) + self.ttl_seconds - time.time()) if self.ttl_seconds else None,
                )

    def find_test_results(self, configurations, suite, branch=None, begin=None, end=None, recent=True, limit=100):
        if not isinstance(suite, str):
            raise TypeError(f'Expected type {str}, got {type(suite)}')

        with self:
            result = {}
            for configuration in configurations:
                result.update({config: [value.unpack() for value in values] for config, values in self.configuration_context.select_from_table_with_configurations(
                    self.UploadsByConfiguration.__table_name__, configurations=[configuration], recent=recent,
                    suite=suite, sdk=configuration.sdk, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY,
                    uuid__gte=CommitContext.convert_to_uuid(begin),
                    uuid__lte=CommitContext.convert_to_uuid(end, CommitContext.timestamp_to_uuid()), limit=limit,
                ).items()})
            return result

    def find_suites(self, configurations, recent=True, limit=100):
        with self:
            return {
                config: [row.suite for row in rows] for config, rows in self.configuration_context.select_from_table_with_configurations(
                    self.SuitesByConfiguration.__table_name__, configurations=configurations, recent=recent, limit=limit,
                ).items()
            }


class UploadCallbackContext(object):
    @classmethod
    def partial_status(cls, exception=None):
        if exception:
            return dict(
                status='error',
                description=str(exception),
            )
        return dict(status='ok')

    def __init__(self, name, configuration_context, commit_context, ttl_seconds=None):
        self.name = name
        self.configuration_context = configuration_context
        self.commit_context = commit_context
        self.cassandra = self.configuration_context.cassandra
        self.ttl_seconds = ttl_seconds

    def __enter__(self):
        self.configuration_context.__enter__()
        self.commit_context.__enter__()

    def __exit__(self, *args, **kwargs):
        self.commit_context.__exit__(*args, **kwargs)
        self.configuration_context.__exit__(*args, **kwargs)

    def register(self, *args, **kwargs):
        return dict(status='error', description=f'No register implemented for {self.name}')
