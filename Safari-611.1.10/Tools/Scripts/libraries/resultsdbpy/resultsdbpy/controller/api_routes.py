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

from flask import abort, jsonify
from resultsdbpy.controller.archive_controller import ArchiveController
from resultsdbpy.controller.commit_controller import CommitController
from resultsdbpy.controller.ci_controller import CIController
from resultsdbpy.controller.failure_controller import FailureController
from resultsdbpy.controller.suite_controller import SuiteController
from resultsdbpy.controller.test_controller import TestController
from resultsdbpy.controller.upload_controller import UploadController
from resultsdbpy.flask_support.authed_blueprint import AuthedBlueprint
from werkzeug.exceptions import HTTPException


class APIRoutes(AuthedBlueprint):
    def __init__(self, model, import_name=__name__, auth_decorator=None):
        super(APIRoutes, self).__init__('controller', import_name, url_prefix='/api', auth_decorator=auth_decorator)

        self.commit_controller = CommitController(commit_context=model.commit_context)
        self.upload_controller = UploadController(commit_controller=self.commit_controller, upload_context=model.upload_context)

        self.suite_controller = SuiteController(suite_context=model.suite_context)
        self.test_controller = TestController(test_context=model.test_context)
        self.failure_controller = FailureController(failure_context=model.failure_context)

        self.ci_controller = CIController(ci_context=model.ci_context, upload_context=model.upload_context)
        self.archive_controller = ArchiveController(commit_controller=self.commit_controller, archive_context=model.archive_context, upload_context=model.upload_context)

        for code in [400, 404, 405]:
            self.register_error_handler(code, self.error_response)
        self.register_error_handler(500, self.response_500)

        self.add_url_rule('/commits', 'commit_controller', self.commit_controller.default, methods=('GET', 'POST'))
        self.add_url_rule('/commits/find', 'commit_controller_find', self.commit_controller.find, methods=('GET',))
        self.add_url_rule('/commits/repositories', 'commit_controller_repositories', self.commit_controller.repositories, methods=('GET',))
        self.add_url_rule('/commits/branches', 'commit_controller_branches',  self.commit_controller.branches, methods=('GET',))
        self.add_url_rule('/commits/siblings', 'commit_controller_siblings', self.commit_controller.siblings, methods=('GET',))
        self.add_url_rule('/commits/next', 'commit_controller_next', self.commit_controller.next, methods=('GET',))
        self.add_url_rule('/commits/previous', 'commit_controller_previous', self.commit_controller.previous, methods=('GET',))
        self.add_url_rule('/commits/register', 'commit_controller_register', self.commit_controller.register, methods=('POST',))

        self.add_url_rule('/upload', 'upload', self.upload_controller.upload, methods=('GET', 'POST'))
        self.add_url_rule('/upload/archive', 'upload_archive', self.archive_controller.endpoint, methods=('GET', 'POST'))
        self.add_url_rule('/upload/process', 'process', self.upload_controller.process, methods=('POST',))
        self.add_url_rule('/suites', 'suites', self.upload_controller.suites, methods=('GET',))
        self.add_url_rule('/<path:suite>/tests', 'tests-in-suite', self.test_controller.list_tests, methods=('GET',))

        self.add_url_rule('/results/<path:suite>', 'suite-results', self.suite_controller.find_run_results, methods=('GET',))
        self.add_url_rule('/results/<path:suite>/<path:test>', 'test-results', self.test_controller.find_test_result, methods=('GET',))

        self.add_url_rule('/failures/<path:suite>', 'suite-failures', self.failure_controller.failures, methods=('GET',))

        self.add_url_rule('/urls/queue', 'queue-urls', self.ci_controller.urls_for_queue_endpoint, methods=('GET',))
        self.add_url_rule('/urls', 'build-urls', self.ci_controller.urls_for_builds_endpoint, methods=('GET',))

    def error_response(self, error):
        response = jsonify(status='error', error=error.name, description=error.description)
        response.status = f'error.{error.name}'
        response.status_code = error.code
        return response

    def response_500(cls, error):
        if isinstance(error, HTTPException):
            print(traceback.format_stack())
            response = jsonify(status='error', error=error.name, description=error.description)
            response.status = f'error.{error.name}'
            response.status_code = error.code
            return response

        response = jsonify(status='error', error='Internal Server Error', description=str(error))
        response.status = 'error.Internal Server Error'
        response.status_code = 500
        return response
