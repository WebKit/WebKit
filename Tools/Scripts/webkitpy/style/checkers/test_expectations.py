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
import os
import re
import sys

from common import TabChecker
from webkitpy.common.host import Host
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.port.base import DummyOptions


_log = logging.getLogger(__name__)


class TestExpectationsChecker(object):
    """Processes test_expectations.txt lines for validating the syntax."""

    categories = set(['test/expectations'])

    def _determine_port_from_exepectations_path(self, host, expectations_path):
        try:
            port_name = expectations_path.split(host.filesystem.sep)[-2]
            if not port_name:
                return None

            # Pass a configuration to avoid calling default_configuration() when initializing the port (takes 0.5 seconds on a Mac Pro!).
            return host.port_factory.get(port_name, options=DummyOptions(configuration="Release"))
        except Exception, e:
            _log.warn("Exception while getting port for path %s" % expectations_path)
            return None

    def __init__(self, file_path, handle_style_error, host=None):
        self._file_path = file_path
        self._handle_style_error = handle_style_error
        self._handle_style_error.turn_off_line_filtering()
        self._tab_checker = TabChecker(file_path, handle_style_error)
        self._output_regex = re.compile('.*test_expectations.txt:(?P<line>\d+)\s*(?P<message>.+)')

        # FIXME: host should be a required parameter, not an optional one.
        host = host or Host()
        host._initialize_scm()

        # Determining the port of this expectations.
        self._port_obj = self._determine_port_from_exepectations_path(host, file_path)
        # Using 'test' port when we couldn't determine the port for this
        # expectations.
        if not self._port_obj:
            _log.warn("Could not determine the port for %s. "
                      "Using 'test' port, but platform-specific expectations "
                      "will fail the check." % self._file_path)
            self._port_obj = host.port_factory.get('test')
        # Suppress error messages of test_expectations module since they will be reported later.
        log = logging.getLogger("webkitpy.layout_tests.layout_package.test_expectations")
        log.setLevel(logging.CRITICAL)

    def _handle_error_message(self, lineno, message, confidence):
        pass

    def check_test_expectations(self, expectations_str, tests=None, overrides=None):
        err = None
        expectations = None
        try:
            expectations = test_expectations.TestExpectations(
                port=self._port_obj, expectations=expectations_str, tests=tests,
                test_config=self._port_obj.test_configuration(),
                is_lint_mode=True, overrides=overrides)
        except test_expectations.ParseError, error:
            err = error

        if err:
            level = 2
            if err.fatal:
                level = 5
            for error in err.errors:
                matched = self._output_regex.match(error)
                if matched:
                    lineno, message = matched.group('line', 'message')
                    self._handle_style_error(int(lineno), 'test/expectations', level, message)


    def check_tabs(self, lines):
        self._tab_checker.check(lines)

    def check(self, lines):
        overrides = self._port_obj.test_expectations_overrides()
        expectations = '\n'.join(lines)
        self.check_test_expectations(expectations_str=expectations,
                                     tests=None,
                                     overrides=overrides)
        # Warn tabs in lines as well
        self.check_tabs(lines)
