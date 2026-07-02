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
from heapq import merge
from resultsdbpy.controller.commit_controller import uuid_range_for_query, commit_for_query, HasCommitContext
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.controller.configuration_controller import configuration_for_query
from resultsdbpy.controller.suite_controller import time_range_for_query
from resultsdbpy.model.test_context import Expectations
from webkitflaskpy.util import AssertRequest, query_as_kwargs, limit_for_query, boolean_query, unescape_argument


class TestController(HasCommitContext):
    DEFAULT_LIMIT = 1000

    def __init__(self, test_context):
        super(TestController, self).__init__(test_context.commit_context)
        self.test_context = test_context

    @query_as_kwargs()
    @limit_for_query(DEFAULT_LIMIT)
    @unescape_argument(
        suite=['?', '#'],
        test=['?', '#'],
    )
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
    @unescape_argument(
        suite=['?', '#'],
        test=['?', '#'],
    )
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

    @query_as_kwargs()
    @commit_for_query()
    @limit_for_query(100)
    @configuration_for_query()
    @unescape_argument(
        suite=['?', '#'],
        test=['?', '#'],
    )
    def summarize_test_results(
        self, suite=None, test=None,
        configurations=None, recent=None,
        branch=None, commit=None,
        limit=None, include_expectations=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        recent = boolean_query(*recent)[0] if recent else True
        include_expectations = boolean_query(*include_expectations)[0] if include_expectations else False

        if not suite:
            abort(400, description='No suite specified')
        if not test:
            abort(400, description='No test specified')

        limit += 1
        before_commits = []
        after_commits = []
        with self.commit_context:
            for repo_id in self.commit_context.repositories.keys():
                if commit:
                    before_commits = sorted(list(reversed(self.commit_context.find_commits_in_range(
                        repository_id=repo_id,
                        end=commit, branch=branch[0], limit=limit,
                    ))) + before_commits)
                    after_commits = sorted(list(reversed(self.commit_context.find_commits_in_range(
                        repository_id=repo_id,
                        begin=commit, branch=branch[0], limit=limit,
                    ))) + after_commits)
                else:
                    before_commits = list(merge(
                        self.commit_context.find_commits_in_range(repository_id=repo_id, branch=branch[0], limit=limit),
                        before_commits,
                    ))
                    before_commits.reverse()
                after_commits.reverse()

        before_commits = sorted(before_commits)
        after_commits = sorted(after_commits)

        before_commits = before_commits[-limit:]
        after_commits = after_commits[:limit]
        if before_commits and after_commits and before_commits[-1] == after_commits[0]:
            del before_commits[-1]

        if not before_commits and not after_commits:
            return abort(400, description='No commits in specified range')

        # Use the linear distance from the specified commit
        scale_for_uuid = {}
        count = limit - 1
        for c in reversed(before_commits):
            scale_for_uuid[c.uuid] = count
            count -= 1
        count = limit
        for c in after_commits:
            scale_for_uuid[c.uuid] = count
            count -= 1

        # A direct match to the provided commit matters most
        if commit:
            scale_for_uuid[commit.uuid] = limit * 2

        response = {}
        for value in Expectations.STATE_ID_TO_STRING.values():
            response[value.lower()] = {} if include_expectations else 0

        with self.test_context:
            for config, results in self.test_context.find_by_commit(
                suite=suite, test=test,
                configurations=configurations, recent=recent,
                branch=branch[0],
                limit=limit * 4,
                begin=(before_commits or after_commits)[0], end=(after_commits or before_commits)[-1],
            ).items():
                for result in results:
                    scale = scale_for_uuid.get(int(result['uuid']), 0)
                    if not scale:
                        continue
                    tag = result.get('actual', 'PASS').lower()
                    if include_expectations:
                        expected = 'expected' if not result.get('expected') or result['actual'] == result['expected'] else 'unexpected'
                        response[tag][expected] = response[tag].get(expected, 0) + scale
                    else:
                        response[tag] = response[tag] + scale

        aggregate = sum([sum(value.values()) if include_expectations else value for value in response.values()])
        if not aggregate:
            return abort(400, description='No results for specified test and configuration in provided commit range')
        for key in response.keys():
            if include_expectations:
                for expectation in response[key].keys():
                    response[key][expectation] = 100 * response[key][expectation] // (aggregate or 1)
            else:
                response[key] = 100 * response[key] // (aggregate or 1)

        return jsonify(response)
