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

from resultsdbpy.model.configuration_context import ClusteredByConfiguration
from resultsdbpy.model.upload_context import UploadCallbackContext


class EWSContext(UploadCallbackContext):
    FLAKY_TTL_SECONDS = 90 * 24 * 60 * 60

    class EWSResultsBase(ClusteredByConfiguration):
        suite = columns.Text(partition_key=True, required=True)
        branch = columns.Text(partition_key=True, required=True)
        test = columns.Text(partition_key=True, required=True)

        start_time = columns.DateTime(primary_key=True, required=True, clustering_order='DESC')
        uuid = columns.BigInt(primary_key=True, required=True)

        result = columns.Text(required=True)  # JSON run-webkit-tests result leaf
        details = columns.Text(required=False)  # Catch-all JSON blob: {authors, pr_number, build_number, build_url, ...}

        def unpack(self):
            results = dict(
                uuid=self.uuid,
                start_time=calendar.timegm(self.start_time.timetuple()),
                result=json.loads(self.result),
            )
            if self.details:
                results['details'] = json.loads(self.details)
            return results

    class EWSFailedTestsByStartTime(EWSResultsBase):
        __table_name__ = 'ews_failed_tests_by_start_time'

    class EWSFlakyTestsByStartTime(EWSResultsBase):
        __table_name__ = 'ews_flaky_tests_by_start_time'
        flaky_type = columns.Text(required=True)  # WithinStepDirtyTree, BetweenStepsDirtyTree, WithinStepCleanTree

        def unpack(self):
            results = super().unpack()
            results['flaky_type'] = self.flaky_type
            return results

    class EWSTestNameBySuite(Model):
        """Which tests each table holds, across every configuration and branch."""
        __table_name__ = 'ews_test_names_by_suite'
        suite = columns.Text(partition_key=True, required=True)
        flaky = columns.Boolean(partition_key=True, required=True)
        test = columns.Text(primary_key=True, required=True)

    def __init__(self, *args, **kwargs):
        super(EWSContext, self).__init__('ews-results', *args, **kwargs)

        with self:
            self.cassandra.create_table(self.EWSFailedTestsByStartTime)
            self.cassandra.create_table(self.EWSFlakyTestsByStartTime)
            self.cassandra.create_table(self.EWSTestNameBySuite)

    def record_results(self, configuration, commits, suite, test_results, timestamp=None, flaky_type=None, details=None):
        stored = set()
        uuid = self.commit_context.uuid_for_commits(commits)
        timestamp = timestamp or time.time()
        if isinstance(timestamp, datetime):
            timestamp = calendar.timegm(timestamp.timetuple())

        # Not relative to commit timestamp since an old commit may cause the short FLAKY TTL to expire immediately.
        ttl = self.FLAKY_TTL_SECONDS if flaky_type is not None else self.ttl_seconds
        ttl = int(ttl) if ttl else None

        extra = dict(flaky_type=flaky_type) if flaky_type is not None else {}
        table = self.EWSFlakyTestsByStartTime if flaky_type is not None else self.EWSFailedTestsByStartTime

        with self, self.cassandra.batch_query_context():
            for branch in self.commit_context.branch_keys_for_commits(commits):
                for test, result in (test_results.get('results') or {}).items():
                    self.configuration_context.insert_row_with_configuration(
                        table.__table_name__, configuration=configuration, suite=suite, branch=branch,
                        test=test, result=json.dumps(result),
                        uuid=uuid, start_time=timestamp, ttl=ttl,
                        details=json.dumps(details) if details else None,
                        **extra,
                    )
                    stored.add(test)
                self.configuration_context.register_configuration(configuration, branch=branch, timestamp=timestamp)

            for test in stored:
                self.cassandra.insert_row(
                    self.EWSTestNameBySuite.__table_name__,
                    suite=suite, flaky=flaky_type is not None, test=test, ttl=ttl,
                )
        return sorted(stored)

    def names(self, suite, flaky, test=None, limit=100):
        """Test names recorded for a suite, optionally restricted to a name prefix."""
        with self:
            args = {'suite': suite, 'flaky': flaky, 'limit': limit}
            if test:
                args.update(test__gte=test, test__lte=test + '~')
            return [row.test for row in self.cassandra.select_from_table(self.EWSTestNameBySuite.__table_name__, **args)]

    def find_for_test(self, configurations, suite, test, flaky, branch=None, recent=True, begin_query_time=None, end_query_time=None, limit=100):
        table = self.EWSFlakyTestsByStartTime if flaky else self.EWSFailedTestsByStartTime

        def get_time(time):
            return time if isinstance(time, datetime) else datetime.utcfromtimestamp(int(time)) if time else None

        with self:
            return {
                config: [row.unpack() for row in rows]
                for config, rows in self.configuration_context.select_from_table_with_configurations(
                    table.__table_name__, configurations=configurations,
                    suite=suite, branch=branch or self.commit_context.DEFAULT_BRANCH_KEY, test=test,
                    start_time__gte=get_time(begin_query_time), start_time__lte=get_time(end_query_time),
                    recent=recent, limit=limit,
                ).items()
            }
