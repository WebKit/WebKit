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

import json
import time

from flask import abort, jsonify, request
from resultsdbpy.controller.commit_controller import HasCommitContext
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.controller.configuration_controller import configuration_for_query
from resultsdbpy.controller.suite_controller import time_range_for_query
from webkitflaskpy.util import AssertRequest, query_as_kwargs, limit_for_query, boolean_query


class EWSController(HasCommitContext):
    DEFAULT_LIMIT = 100
    TIMESTAMP_TOLERANCE_SECONDS = 5 * 60
    MAX_TESTS_PER_QUERY = 100

    def __init__(self, commit_controller, ews_context):
        super().__init__(commit_controller.commit_context)
        self.commit_controller = commit_controller
        self.ews_context = ews_context

    def upload(self):
        AssertRequest.is_type(['POST'])
        AssertRequest.no_query()

        try:
            data = json.loads(request.get_data())
        except ValueError:
            abort(400, description='Expected uploaded data to be json')
        if not isinstance(data, dict):
            abort(400, description='Expected uploaded data to be a json object')

        try:
            configuration = Configuration.from_json(data.get('configuration', {}))
        except (ValueError, TypeError):
            abort(400, description='Invalid configuration')

        suite = data.get('suite')
        if not suite or not isinstance(suite, str):
            abort(400, description='Invalid suite')

        test_results = data.get('test_results')
        results = test_results.get('results') if isinstance(test_results, dict) else None
        if not results or not isinstance(results, dict) or any(not isinstance(result, dict) for result in results.values()):
            abort(400, description='Invalid test results')

        timestamp = data.get('timestamp', time.time())
        if isinstance(timestamp, bool) or not isinstance(timestamp, (int, float)):
            abort(400, description='Invalid timestamp')
        if not 0 < timestamp < time.time() + self.TIMESTAMP_TOLERANCE_SECONDS:
            abort(400, description=f'Unrealistic timestamp: {timestamp}')

        flaky_type = data.get('flaky_type')
        if flaky_type is not None and not isinstance(flaky_type, str):
            abort(400, description='Invalid flaky_type')

        details = data.get('details')
        if details is not None and not isinstance(details, dict):
            abort(400, description='Invalid details')

        commits = data.get('commits')
        if not commits or not isinstance(commits, list) or any(not isinstance(commit, dict) for commit in commits):
            abort(400, description='Invalid commits')
        commits = [self.commit_controller.register(commit=commit, fast=True) for commit in commits]

        try:
            stored = self.ews_context.record_results(
                configuration, commits, suite, test_results,
                timestamp=timestamp, flaky_type=flaky_type, details=details,
            )
        except (TypeError, ValueError) as error:
            abort(400, description=str(error))

        # Naming the tests that were stored lets a caller log the write instead of assuming it.
        return jsonify({'status': 'ok', 'tests': stored})

    @query_as_kwargs()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    def find(
        self, configurations=None, suite=None, tests=None, recent=None, flaky=None,
        branch=None, begin_query_time=None, end_query_time=None,
        limit=None, **kwargs
    ):
        """Results for one or more tests, one entry per test and configuration.

        limit is per test rather than shared across the batch, since a shared one would be spent on
        whichever test happened to be queried first. A test with no results is absent from the
        response rather than present and empty, and a batch where no test has any results is a 404.
        """
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        if not suite or not suite[0]:
            abort(400, description='No valid test suite specified')
        if not (tests := sorted({name for name in tests or [] if name})):
            abort(400, description='No valid test specified')
        if len(tests) > self.MAX_TESTS_PER_QUERY:
            abort(400, description=f'Cannot query more than {self.MAX_TESTS_PER_QUERY} tests at a time, got {len(tests)} tests')

        recent = boolean_query(*recent)[0] if recent else True
        flaky = boolean_query(*flaky)[0] if flaky else False

        found = self.ews_context.find_for_tests(
            configurations=configurations, suite=suite[0], branch=branch[0] if branch else None,
            begin_query_time=begin_query_time, end_query_time=end_query_time, limit=limit,
            tests=tests, flaky=flaky, recent=recent,
        )
        if not found:
            abort(404, description='No EWS results matching the specified criteria')

        response = []
        for name, by_configuration in found.items():
            for config, entries in by_configuration.items():
                response.append({
                    'test': name,
                    'configuration': Configuration.Encoder().default(config),
                    'results': sorted(entries, key=lambda result: (result['uuid'], result['start_time'])),
                })
        return jsonify(response)

    @query_as_kwargs()
    @limit_for_query(DEFAULT_LIMIT)
    def list_tests(self, suite=None, prefixes=None, flaky=None, limit=None, **kwargs):
        """Names recorded in the EWS tables by any configuration & branch."""
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        if not suite or not suite[0]:
            abort(400, description='No valid test suite specified')

        flaky = boolean_query(*flaky)[0] if flaky else False

        names = set()
        for prefix in prefixes or [None]:
            if len(names) >= limit:
                break
            names.update(self.ews_context.names(
                suite=suite[0], flaky=flaky, test=prefix, limit=limit - len(names),
            ))
        return jsonify(sorted(names))
