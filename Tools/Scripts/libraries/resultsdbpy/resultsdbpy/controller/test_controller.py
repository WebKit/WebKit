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


class TestController(HasCommitContext):
    DEFAULT_LIMIT = 1000

    def __init__(self, test_context):
        super(TestController, self).__init__(test_context.commit_context)
        self.test_context = test_context

    @query_as_kwargs()
    @limit_for_query(DEFAULT_LIMIT)
    def list_tests(self, suite=None, test=None, limit=None, **kwargs):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        if not suite:
            abort(400, description='No suite specified')

        with self.test_context:
            matching_tests = set()
            for t in test or [None]:
                matching_tests.update(self.test_context.names(suite=suite, test=t, limit=limit - len(matching_tests)))
            return jsonify(sorted(matching_tests))

    @query_as_kwargs()
    @uuid_range_for_query()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    def find_test_result(
        self, suite=None, test=None,
        configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None,
        limit=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        recent = boolean_query(*recent)[0] if recent else True

        if not suite:
            abort(400, description='No suite specified')
        if not test:
            abort(400, description='No test specified')

        with self.test_context:
            query_dict = dict(
                suite=suite, test=test,
                configurations=configurations, recent=recent,
                branch=branch[0], begin=begin, end=end,
                begin_query_time=begin_query_time, end_query_time=end_query_time,
                limit=limit,
            )
            specified_commits = sum([1 if element else 0 for element in [begin, end]])
            specified_timestamps = sum([1 if element else 0 for element in [begin_query_time, end_query_time]])

            if specified_commits >= specified_timestamps:
                find_function = self.test_context.find_by_commit

                def sort_function(result):
                    return result['uuid']

            else:
                find_function = self.test_context.find_by_start_time

                def sort_function(result):
                    return result['start_time']

            response = []
            for config, results in find_function(**query_dict).items():
                response.append(dict(
                    configuration=Configuration.Encoder().default(config),
                    results=sorted(results, key=sort_function),
                ))
            return jsonify(response)
