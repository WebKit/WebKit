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


class FailureController(HasCommitContext):
    DEFAULT_LIMIT = 1000

    def __init__(self, failure_context):
        super(FailureController, self).__init__(failure_context.commit_context)
        self.failure_context = failure_context

    @query_as_kwargs()
    @uuid_range_for_query()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    def failures(
        self, suite=None,
        configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None,
        unexpected=None, collapsed=None,
        limit=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        recent = boolean_query(*recent)[0] if recent else True
        unexpected = boolean_query(*unexpected)[0] if unexpected else True
        collapsed = boolean_query(*collapsed)[0] if collapsed else True

        if not suite:
            abort(400, description='No suite specified')

        with self.failure_context:
            query_dict = dict(
                suite=suite,
                configurations=configurations, recent=recent,
                branch=branch[0], begin=begin, end=end,
                begin_query_time=begin_query_time, end_query_time=end_query_time,
                limit=limit, unexpected=unexpected, collapsed=collapsed,
            )
            num_specified_commits = sum([1 if element else 0 for element in [begin, end]])
            num_specified_timestamps = sum([1 if element else 0 for element in [begin_query_time, end_query_time]])

            if num_specified_commits >= num_specified_timestamps:
                find_function = self.failure_context.failures_by_commit

                def sort_function(result):
                    return result['uuid']

            else:
                find_function = self.failure_context.failures_by_start_time

                def sort_function(result):
                    return result['start_time']

            failures = find_function(**query_dict)
            if failures is None:
                abort(404, description='No test runs found with the specified criteria')

            if collapsed:
                return jsonify(sorted(set(failures)))

            response = []
            for config, results in failures.items():
                response.append(dict(
                    configuration=Configuration.Encoder().default(config),
                    results=sorted(results, key=sort_function),
                ))
            return jsonify(response)
