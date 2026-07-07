# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Checks WebKit style for API test expectations (TestExpectations/apitests)."""

import logging
import optparse

from webkitpy.api_tests.test_expectations import APITestExpectations
from webkitpy.common.host import Host
from webkitpy.style.error_handlers import DefaultStyleErrorHandler


_log = logging.getLogger(__name__)


class APITestExpectationsChecker(object):
    """Runs the shared API test expectations linter and reports issues that touch the patch.

    The actual validation (syntax, 3-section ordering, stale skips, ...) lives in
    webkitexpectationspy and is reused as-is via APITestExpectations.lint(); this checker
    only forwards the resulting warnings into check-webkit-style.
    """

    categories = set(['apitest/expectations'])

    @staticmethod
    def _should_log_warning(warning, files, cwd, host):
        # warning.filename is absolute (api_test_expectations_files are rooted at the
        # WebKit base); joining an absolute path with cwd yields the absolute path.
        abs_filename = host.filesystem.join(cwd, warning.filename)
        patched_lines = files.get(abs_filename)
        if not patched_lines:
            # None/empty means the whole file is part of the patch (e.g. newly added).
            return abs_filename in files
        return warning.line_number in patched_lines

    @staticmethod
    def lint_test_expectations(files, configuration, cwd, increment_error_count=lambda: 0, host=None):
        host = host or Host()

        # Pass a configuration to avoid calling default_configuration() when
        # initializing the port (takes ~0.5s); expectations parsing ignores it.
        options = optparse.Values({'configuration': 'Release'})
        port = host.port_factory.get(options=options)

        expectations = APITestExpectations(port)
        expectations.parse_all_expectations()

        error_count = 0
        has_fixable = False
        for warning in expectations.lint():
            if not APITestExpectationsChecker._should_log_warning(warning, files, cwd, host):
                continue
            message = getattr(warning, 'message', None) or getattr(warning, 'error', '')
            style_error_handler = DefaultStyleErrorHandler(warning.filename, configuration, increment_error_count)
            style_error_handler(warning.line_number, 'apitest/expectations', 5, message)
            error_count += 1
            if getattr(warning, 'fixable', False):
                has_fixable = True

        if has_fixable:
            _log.info('Some of these can be fixed automatically by running: run-api-tests --fix-expectations')
        return error_count
