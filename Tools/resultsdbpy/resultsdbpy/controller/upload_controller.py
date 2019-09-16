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

import json
import time

from collections import defaultdict
from flask import abort, jsonify, request
from resultsdbpy.flask_support.util import AssertRequest, query_as_kwargs, limit_for_query, boolean_query
from resultsdbpy.controller.commit import Commit
from resultsdbpy.controller.commit_controller import uuid_range_for_query, HasCommitContext
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.controller.configuration_controller import configuration_for_query


class UploadController(HasCommitContext):
    DEFAULT_LIMIT = 100

    def __init__(self, commit_controller, upload_context):
        super(UploadController, self).__init__(commit_controller.commit_context)
        self.commit_controller = commit_controller
        self.upload_context = upload_context

    @query_as_kwargs()
    @uuid_range_for_query()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    def _find_uploads_for_query(self, configurations=None, suite=None, branch=None, begin=None, end=None, recent=None, limit=None, **kwargs):
        AssertRequest.query_kwargs_empty(**kwargs)
        recent = boolean_query(*recent)[0] if recent else True

        with self.upload_context:
            if not suite:
                suites = set()
                for config_suites in self.upload_context.find_suites(configurations=configurations, recent=recent).values():
                    [suites.add(suite) for suite in config_suites]
            else:
                suites = set(suite)

            current_uploads = 0
            result = defaultdict(dict)
            for suite in suites:
                if current_uploads >= limit:
                    break
                results_dict = self.upload_context.find_test_results(
                    configurations=configurations, suite=suite, branch=branch[0],
                    begin=begin, end=end, recent=recent, limit=(limit - current_uploads),
                )
                for config, results in results_dict.items():
                    current_uploads += len(results)
                    result[config][suite] = results
            return result

    def download(self):
        AssertRequest.is_type(['GET'])

        with self.upload_context:
            uploads = self._find_uploads_for_query()

            response = []
            for config, suite_results in uploads.items():
                for suite, results in suite_results.items():
                    for result in results:
                        config.sdk = result.get('sdk')
                        response.append(dict(
                            configuration=Configuration.Encoder().default(config),
                            suite=suite,
                            commits=Commit.Encoder().default(result['commits']),
                            timestamp=result['timestamp'],
                            test_results=result['test_results'],
                        ))

            return jsonify(response)

    def upload(self):
        if request.method == 'GET':
            return self.download()

        AssertRequest.is_type(['POST'])
        AssertRequest.no_query()

        with self.upload_context:
            try:
                data = request.form or json.loads(request.get_data())
            except ValueError:
                abort(400, description='Expected uploaded data to be json')

            try:
                configuration = Configuration.from_json(data.get('configuration', {}))
            except (ValueError, TypeError):
                abort(400, description='Invalid configuration')

            suite = data.get('suite')
            if not suite:
                abort(400, description='No test suite specified')

            commits = [self.commit_controller.register(commit=commit) for commit in data.get('commits', [])]

            test_results = data.get('test_results', {})
            if not test_results:
                abort(400, description='No test results specified')

            timestamp = data.get('timestamp', time.time())
            version = data.get('version', 0)

            try:
                self.upload_context.upload_test_results(configuration, commits, suite, test_results, timestamp, version=version)
            except (TypeError, ValueError) as error:
                abort(400, description=str(error))

            processing_results = self.upload_context.process_test_results(configuration, commits, suite, test_results, timestamp)
            return jsonify(dict(status='ok', processing=processing_results))

    def process(self):
        AssertRequest.is_type(['POST'])

        with self.upload_context:
            uploads = self._find_uploads_for_query()
            if not uploads:
                abort(404, description='No uploads matching the specified criteria')

            response = []
            for config, suite_results in uploads.items():
                for suite, results in suite_results.items():
                    for result in results:
                        config.sdk = result.get('sdk')
                        processing_results = self.upload_context.process_test_results(
                            configuration=config, commits=result['commits'], suite=suite,
                            test_results=result['test_results'], timestamp=result['timestamp'],
                        )
                        response.append(dict(
                            configuration=Configuration.Encoder().default(config),
                            suite=suite,
                            commits=Commit.Encoder().default(result['commits']),
                            timestamp=result['timestamp'],
                            processing=processing_results,
                        ))

            return jsonify(response)

    @query_as_kwargs()
    @configuration_for_query()
    def suites(self, configurations=None, recent=None, suite=None, branch=None, **kwargs):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        with self.upload_context:
            suites_by_config = self.upload_context.find_suites(
                configurations=configurations,
                recent=boolean_query(*recent)[0] if recent else True,
                branch=branch[0] if branch else None,
            )
            result = []
            for config, candidate_suites in suites_by_config.items():
                suites_for_config = [s for s in candidate_suites if not suite or s in suite]
                if suites_for_config:
                    result.append([config, suites_for_config])
            if not result:
                abort(404, description='No suites matching the specified criteria')
            return jsonify(Configuration.Encoder().default(result))
