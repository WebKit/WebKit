# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Checks WebKit style for test_expectations files."""

import logging
import optparse
import os
import re
import sys

from common import TabChecker
from webkitpy.common.host import Host
from webkitpy.layout_tests.models import test_expectations
from webkitpy.style.error_handlers import DefaultStyleErrorHandler


_log = logging.getLogger(__name__)


class TestExpectationsChecker(object):
    """Processes TestExpectations lines for validating the syntax."""

    categories = set(['test/expectations'])

    def _determine_port_from_expectations_path(self, host, expectations_path):
        # Pass a configuration to avoid calling default_configuration() when initializing the port (takes 0.5 seconds on a Mac Pro!).
        options_wk1 = optparse.Values({'configuration': 'Release', 'webkit_test_runner': False})
        options_wk2 = optparse.Values({'configuration': 'Release', 'webkit_test_runner': True})
        for port_name in host.port_factory.all_port_names():
            ports = [host.port_factory.get(port_name, options=options_wk1), host.port_factory.get(port_name, options=options_wk2)]
            for port in ports:
                for test_expectation_file in port.expectations_files():
                    if test_expectation_file.replace(port.path_from_webkit_base() + host.filesystem.sep, '') == expectations_path:
                        return port
        return None

    def __init__(self, file_path, handle_style_error, host=None):
        self._file_path = file_path
        self._handle_style_error = handle_style_error
        self._tab_checker = TabChecker(file_path, handle_style_error)

        # FIXME: host should be a required parameter, not an optional one.
        host = host or Host()
        host.initialize_scm()

        self._port_obj = self._determine_port_from_expectations_path(host, file_path)

        # Suppress error messages of test_expectations module since they will be reported later.
        log = logging.getLogger("webkitpy.layout_tests.layout_package.test_expectations")
        log.setLevel(logging.CRITICAL)

    def _handle_error_message(self, lineno, message, confidence):
        pass

    def check_test_expectations(self, expectations_str, tests=None):
        parser = test_expectations.TestExpectationParser(self._port_obj, tests, allow_rebaseline_modifier=False)
        expectations = parser.parse('expectations', expectations_str)

        level = 5
        for expectation_line in expectations:
            for warning in expectation_line.warnings:
                self._handle_style_error(expectation_line.line_number, 'test/expectations', level, warning)

    def check_tabs(self, lines):
        self._tab_checker.check(lines)

    def check(self, lines):
        expectations = '\n'.join(lines)
        if self._port_obj:
            self.check_test_expectations(expectations_str=expectations, tests=None)

        # Warn tabs in lines as well
        self.check_tabs(lines)

    @staticmethod
    def _should_log_linter_warning(warning, files, cwd, host):
        abs_filename = host.filesystem.join(cwd, warning.filename)

        # Case 1, the line the warning was tied to is in our patch.
        if abs_filename in files and files[abs_filename] and warning.line_number in files[abs_filename]:
            return True

        for file, lines in warning.related_files.iteritems():
            abs_filename = host.filesystem.join(cwd, file)
            if abs_filename in files:
                # Case 2, a file associated with the warning is in our patch
                # Note that this will really only happen if you delete a test.
                if lines is None:
                    return True

                # Case 3, a line associated with the warning is in our patch.
                for line in lines:
                    if files[abs_filename] and line in files[abs_filename]:
                        return True
        return False

    @staticmethod
    def lint_test_expectations(files, configuration, cwd, increment_error_count=lambda: 0, line_numbers=None, host=Host()):
        error_count = 0
        files_linted = set()
        ports_to_lint = [host.port_factory.get(name) for name in host.port_factory.all_port_names()]
        for port in ports_to_lint:
            for expectations_file in port.expectations_dict().keys():
                style_error_handler = DefaultStyleErrorHandler(expectations_file, configuration, increment_error_count, line_numbers)

                try:
                    if expectations_file in files_linted:
                        continue
                    expectations = test_expectations.TestExpectations(
                        port,
                        expectations_to_lint={expectations_file: port.expectations_dict()[expectations_file]})
                    expectations.parse_all_expectations()
                except test_expectations.ParseError as e:
                    for warning in e.warnings:
                        if TestExpectationsChecker._should_log_linter_warning(warning, files, cwd, host):
                            style_error_handler(warning.line_number, 'test/expectations', 5, warning.error)
                            error_count += 1
                files_linted.add(expectations_file)
        return error_count
