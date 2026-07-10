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
from resultsdbpy.model.commit_context import CommitContext
from resultsdbpy.model.configuration_context import ClusteredByConfiguration
from resultsdbpy.model.test_context import Expectations
from resultsdbpy.model.upload_context import UploadCallbackContext


TTL_SECONDS = 30 * 24 * 60 * 60
FLAKY_TTL_SECONDS = 7 * 24 * 60 * 60


class EWSContext(UploadCallbackContext):
    DEFAULT_LIMIT = 100

    class EWSResultsBase(ClusteredByConfiguration):
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        uuid = columns.BigInt(primary_key=True, required=True, clustering_order='DESC')
        start_time = columns.DateTime(primary_key=True, required=True)
        result = columns.Integer(required=True)
        remote = columns.Text(required=False)
        pr_number = columns.Integer(required=False)
        commit_hash = columns.Text(required=False)
        details = columns.Text(required=False)  # Catch all json blob after deploy

        def unpack(self):
            results = dict(
                uuid=self.uuid,
                start_time=calendar.timegm(self.start_time.timetuple()),
                result=Expectations.state_ids_to_string([self.result]),
            )
            if self.remote:
                results['remote'] = self.remote
            if self.pr_number is not None:
                results['pr_number'] = self.pr_number
            if self.commit_hash:
                results['commit_hash'] = self.commit_hash
            if self.details:
                results['details'] = json.loads(self.details)
            return results

    class EWSFailedTestsByCommit(EWSResultsBase):
        # test in the partition: one test's failure history is a single-partition read.
        __table_name__ = 'ews_failed_tests_by_commit'
        test = columns.Text(partition_key=True, required=True)

    class EWSFlakyTestsByCommit(EWSResultsBase):
        # test as the leading clustering key: the (configuration, suite, branch) partition
        # holds the small set of flaky tests, so we can list them all or check one cheaply.
        __table_name__ = 'ews_flaky_tests_by_commit'
        test = columns.Text(primary_key=True, required=True)
        flaky_type = columns.Text(required=True)  # InterStep, IntraStep, etc.

        def unpack(self):
            results = super().unpack()
            results['flaky_type'] = self.flaky_type
            return results

    def __init__(self, *args, **kwargs):
        super(EWSContext, self).__init__('ews-results', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.EWSFailedTestsByCommit)
            self.cassandra.create_table(self.EWSFlakyTestsByCommit)
