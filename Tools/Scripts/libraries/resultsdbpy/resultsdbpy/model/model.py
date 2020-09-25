# Copyright (C) 2019, 2020 Apple Inc. All rights reserved.
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

import traceback
import sys

from cassandra.cqlengine import columns
from cassandra.cqlengine.models import Model as CassandraModel
from resultsdbpy.model.archive_context import ArchiveContext
from resultsdbpy.model.ci_context import CIContext
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ConfigurationContext
from resultsdbpy.model.upload_context import UploadContext
from resultsdbpy.model.suite_context import SuiteContext
from resultsdbpy.model.test_context import TestContext
from resultsdbpy.model.failure_context import FailureContext


class Model(object):
    TTL_DAY = 60 * 60 * 24
    TTL_WEEK = 7 * TTL_DAY
    TTL_YEAR = 365 * TTL_DAY

    class HealthTable(CassandraModel):
        __table_name__ = 'health'
        key = columns.Text(partition_key=True, required=True)
        value = columns.Text(required=True)

    def __init__(self, redis, cassandra, repositories=[], default_ttl_seconds=TTL_YEAR * 5, archive_ttl_seconds=TTL_WEEK * 8, async_processing=False, s3_credentials=None):
        if default_ttl_seconds is not None and default_ttl_seconds < 4 * self.TTL_WEEK:
            raise ValueError('TTL must be at least 4 weeks')
        if archive_ttl_seconds is not None and archive_ttl_seconds < 2 * self.TTL_WEEK:
            raise ValueError('Archive TTL must be at least 2 weeks')
        self.default_ttl_seconds = default_ttl_seconds
        self.archive_ttl_seconds = archive_ttl_seconds or default_ttl_seconds
        self._async_processing = async_processing

        self.redis = redis
        self.cassandra = cassandra

        with self.cassandra:
            self.cassandra.create_table(self.HealthTable)

        self.commit_context = CommitContext(redis, cassandra)
        for repository in repositories:
            self.commit_context.register_repository(repository)
        self.configuration_context = ConfigurationContext(redis, cassandra)
        self.upload_context = UploadContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.default_ttl_seconds,
            async_processing=async_processing,
        )

        self.suite_context = SuiteContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.default_ttl_seconds,
        )
        self.test_context = TestContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.default_ttl_seconds,
        )
        self.failure_context = FailureContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.default_ttl_seconds,
        )
        self.ci_context = CIContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.default_ttl_seconds,
        )

        for context in [self.ci_context, self.failure_context, self.suite_context, self.test_context]:
            self.upload_context.register_upload_callback(context.name, context.register)

        self.archive_context = ArchiveContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.archive_ttl_seconds,
            s3_credentials=s3_credentials,
        )

    def healthy(self, writable=True):
        if writable:
            self.redis.set('health-check', b'healthy', ex=5 * 60)
        value = self.redis.get('health-check')
        if value is not None and value != b'healthy':
            return False

        with self.cassandra:
            if writable:
                self.cassandra.insert_row(
                    self.HealthTable.__table_name__,
                    key='health-check', value='healthy',
                    ttl=5 * 60,
                )
            values = [element.value for element in self.cassandra.select_from_table(
                self.HealthTable.__table_name__, limit=100, key='health-check',
            )]
            if values and values != ['healthy']:
                return False

        return True

    def do_work(self):
        if not self._async_processing:
            raise RuntimeError('No work to be done, asynchronous processing disabled')

        try:
            return self.upload_context.do_processing_work()
        except Exception as e:
            sys.stderr.write(f'{traceback.format_exc()}\n')
            sys.stderr.write(f'{e}\n')

        return False
