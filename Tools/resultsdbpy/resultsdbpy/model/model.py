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

import traceback
import sys

from resultsdbpy.model.archive_context import ArchiveContext
from resultsdbpy.model.ci_context import CIContext
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ConfigurationContext
from resultsdbpy.model.upload_context import UploadContext
from resultsdbpy.model.suite_context import SuiteContext
from resultsdbpy.model.test_context import TestContext


class Model(object):
    TTL_DAY = 60 * 60 * 24
    TTL_WEEK = 7 * TTL_DAY
    TTL_YEAR = 365 * TTL_DAY

    def __init__(self, redis, cassandra, repositories=[], default_ttl_seconds=TTL_YEAR * 5, async_processing=False):
        if default_ttl_seconds is not None and default_ttl_seconds < 4 * self.TTL_WEEK:
            raise ValueError('TTL must be at least 4 weeks')
        self.default_ttl_seconds = default_ttl_seconds
        self._async_processing = async_processing

        self.redis = redis
        self.cassandra = cassandra
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
        self.ci_context = CIContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.default_ttl_seconds,
        )

        for context in [self.suite_context, self.test_context, self.ci_context]:
            self.upload_context.register_upload_callback(context.name, context.register)

        self.archive_context = ArchiveContext(
            configuration_context=self.configuration_context,
            commit_context=self.commit_context,
            ttl_seconds=self.default_ttl_seconds,
        )

    def do_work(self):
        if not self._async_processing:
            raise RuntimeError('No work to be done, asynchronous processing disabled')

        try:
            self.upload_context.do_processing_work()
        except Exception as e:
            sys.stderr.write(f'{traceback.format_exc()}\n')
            sys.stderr.write(f'{e}\n')
