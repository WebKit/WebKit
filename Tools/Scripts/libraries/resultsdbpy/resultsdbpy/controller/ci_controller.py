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

from flask import abort, jsonify
from resultsdbpy.controller.commit_controller import uuid_range_for_query, HasCommitContext
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.controller.configuration_controller import configuration_for_query
from resultsdbpy.controller.suite_controller import time_range_for_query
from resultsdbpy.flask_support.util import AssertRequest, query_as_kwargs, limit_for_query, boolean_query


class CIController(HasCommitContext):
    DEFAULT_QUERY_LIMIT = 100

    def __init__(self, ci_context, upload_context):
        super(CIController, self).__init__(ci_context.commit_context)
        self.ci_context = ci_context
        self.upload_context = upload_context

    def _suites_for_query_arguments(self, suite=None, configurations=None, is_recent=True):
        # The user may have specified a set of suites to search by, or we need to determine the
        # suites based on the current configuration.
        if suite:
            return suite

        with self.upload_context:
            suites = set()
            # Returns a dictionary where the keys are configuration objects and the values are strings
            # representing suites associated with that configuration.
            for candidate_suites in self.upload_context.find_suites(configurations=configurations, recent=is_recent).values():
                for candidate in candidate_suites:
                    suites.add(candidate)

            return sorted(suites)

    @limit_for_query(DEFAULT_QUERY_LIMIT)
    @configuration_for_query()
    def urls_for_queue(self, suite=None, branch=None, configurations=None, recent=None, limit=None, **kwargs):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        is_recent = True
        if recent:
            is_recent = boolean_query(*recent)[0]

        with self.ci_context, self.upload_context:
            suites = self._suites_for_query_arguments(suite=suite, configurations=configurations, is_recent=is_recent)
            if not branch:
                branch = [None]

            results = []
            for suite in suites:
                for config, url in self.ci_context.find_urls_by_queue(
                    configurations=configurations, recent=is_recent,
                    branch=branch[0], suite=suite, limit=limit,
                ).items():
                    configuration_dict = Configuration.Encoder().default(config)
                    configuration_dict['suite'] = suite
                    results.append(dict(configuration=configuration_dict, url=url))

            return results

    @uuid_range_for_query()
    @limit_for_query(DEFAULT_QUERY_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    def urls_for_builds(
        self, suite=None,
        configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None,
        limit=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        is_recent = True
        if recent:
            is_recent = boolean_query(*recent)[0]

        with self.ci_context, self.upload_context:
            suites = self._suites_for_query_arguments(suite=suite, configurations=configurations, is_recent=is_recent)
            if not branch:
                branch = [None]

            query_dict = dict(
                configurations=configurations, recent=is_recent,
                branch=branch[0], begin=begin, end=end,
                begin_query_time=begin_query_time, end_query_time=end_query_time,
                limit=limit,
            )
            num_uuid_query_args = sum([1 if element else 0 for element in [begin, end]])
            num_timestamp_query_args = sum([1 if element else 0 for element in [begin_query_time, end_query_time]])

            if num_uuid_query_args >= num_timestamp_query_args:
                find_function = self.ci_context.find_urls_by_commit

                def sort_function(result):
                    return result['uuid']
            else:
                find_function = self.ci_context.find_urls_by_start_time

                def sort_function(result):
                    return result['start_time']

            results = []
            for suite in suites:
                for config, urls in find_function(suite=suite, **query_dict).items():
                    configuration_dict = Configuration.Encoder().default(config)
                    configuration_dict['suite'] = suite
                    results.append(dict(configuration=configuration_dict, urls=sorted(urls, key=sort_function)))

            return results

    @query_as_kwargs()
    def urls_for_queue_endpoint(self, **kwargs):
        return jsonify(self.urls_for_queue(**kwargs))

    @query_as_kwargs()
    def urls_for_builds_endpoint(self, **kwargs):
        return jsonify(self.urls_for_builds(**kwargs))
