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

import hashlib
import io
import json
import time

from flask import abort, jsonify, request, send_file
from resultsdbpy.controller.commit import Commit
from resultsdbpy.controller.commit_controller import uuid_range_for_query, HasCommitContext
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.controller.configuration_controller import configuration_for_query
from resultsdbpy.controller.suite_controller import time_range_for_query
from resultsdbpy.flask_support.util import AssertRequest, query_as_kwargs, limit_for_query, boolean_query


class ArchiveController(HasCommitContext):
    def __init__(self, commit_controller, archive_context, upload_context):
        super(ArchiveController, self).__init__(commit_controller.commit_context)
        self.archive_context = archive_context
        self.commit_controller = commit_controller
        self.upload_context = upload_context

    def endpoint(self):
        if request.method == 'POST':
            return self.upload()
        return self.download()

    @query_as_kwargs()
    @uuid_range_for_query()
    @configuration_for_query()
    @time_range_for_query()
    def download(
        self, suite=None, configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)
        recent = boolean_query(*recent)[0] if recent else True

        if not suite:
            suites = set()
            for config_suites in self.upload_context.find_suites(configurations=configurations, recent=recent).values():
                [suites.add(suite) for suite in config_suites]
        else:
            suites = set(suite)

        result = None
        filename = None
        digest = None
        with self.archive_context, self.upload_context:
            for suite in suites:
                for configuration, archives in self.archive_context.find_archive(
                    configurations=configurations, suite=suite, branch=branch[0],
                    begin=begin, end=end, recent=recent, limit=2,
                    begin_query_time=begin_query_time, end_query_time=end_query_time,
                ).items():
                    for archive in archives:
                        if archive.get('archive') and archive.get('digest'):
                            if digest and digest != archive.get('digest'):
                                abort(400, description='Multiple archives matching the specified criteria')
                            result = archive.get('archive')
                            filename = f'{configuration}@{archive["uuid"]}'.replace(' ', '_').replace('.', '-')
                            digest = archive.get('digest')

        if not result:
            abort(404, description='No archives matching the specified criteria')
        return send_file(result, attachment_filename=f'{filename}.zip', as_attachment=True)

    def upload(self):
        AssertRequest.is_type(['POST'])
        AssertRequest.no_query()

        with self.archive_context:
            if 'file' not in request.files:
                abort(400, description='No archive provided')
            archive = io.BytesIO(request.files['file'].read())

            try:
                data = request.form or json.loads(request.get_data())
            except ValueError:
                abort(400, description='Expected meta-data to be json')

            try:
                configuration = Configuration.from_json(data.get('configuration', {}))
            except (ValueError, TypeError) as e:
                abort(400, description=f'Invalid configuration, error: {e}')

            suite = data.get('suite')
            if not suite:
                abort(400, description='No test suite specified')

            try:
                commits = [self.commit_controller.register(commit=commit) for commit in json.loads(data.get('commits', '[]'))]
            except ValueError:
                abort(400, description='Expected commit meta-data to be json')
            if not commits:
                abort(400, description='No commits provided')

            timestamp = data.get('timestamp', time.time())

            try:
                self.archive_context.register(archive, configuration, commits, suite, timestamp)
            except (TypeError, ValueError) as error:
                abort(400, description=str(error))

        return jsonify(dict(status='ok'))
