#!/usr/bin/python
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

import os
import sys
import unittest

from test_expectations import TestExpectationsChecker
from webkitpy.common.host_mock import MockHost


class ErrorCollector(object):
    """An error handler class for unit tests."""

    def __init__(self):
        self._errors = []
        self.turned_off_filtering = False

    def turn_off_line_filtering(self):
        self.turned_off_filtering = True

    def __call__(self, lineno, category, confidence, message):
        self._errors.append('%s  [%s] [%d]' % (message, category, confidence))

    def get_errors(self):
        return ''.join(self._errors)

    def reset_errors(self):
        self._errors = []
        self.turned_off_filtering = False


class TestExpectationsTestCase(unittest.TestCase):
    """TestCase for test_expectations.py"""

    def setUp(self):
        self._error_collector = ErrorCollector()
        self._test_file = 'passes/text.html'

    def _expect_port_for_expectations_path(self, expected_port_or_port_class, expectations_path):
        host = MockHost()
        checker = TestExpectationsChecker(expectations_path, ErrorCollector(), host=host)
        port = checker._determine_port_from_exepectations_path(host, expectations_path)
        if port:
            self.assertEquals(port.__class__.__name__, expected_port_or_port_class)
        else:
            self.assertEquals(port, expected_port_or_port_class)

    def test_determine_port_from_exepectations_path(self):
        self._expect_port_for_expectations_path(None, "/")
        self._expect_port_for_expectations_path("ChromiumMacPort", "/mock-checkout/LayoutTests/chromium-mac/test_expectations.txt")

    def assert_lines_lint(self, lines, expected):
        self._error_collector.reset_errors()
        checker = TestExpectationsChecker('test/test_expectations.txt',
                                          self._error_collector, host=MockHost())
        checker.check_test_expectations(expectations_str='\n'.join(lines),
                                        tests=[self._test_file],
                                        overrides=None)
        checker.check_tabs(lines)
        self.assertEqual(expected, self._error_collector.get_errors())
        self.assertTrue(self._error_collector.turned_off_filtering)

    def test_valid_expectations(self):
        self.assert_lines_lint(
            ["BUGCR1234 MAC : passes/text.html = PASS FAIL"],
            "")
        self.assert_lines_lint(
            ["SKIP BUGCR1234 : passes/text.html = TIMEOUT PASS"],
            "")
        self.assert_lines_lint(
            ["BUGCR1234 DEBUG : passes/text.html = TIMEOUT PASS"],
            "")
        self.assert_lines_lint(
            ["BUGCR1234 DEBUG SKIP : passes/text.html = TIMEOUT PASS"],
            "")
        self.assert_lines_lint(
            ["BUGCR1234 MAC DEBUG SKIP : passes/text.html = TIMEOUT PASS"],
            "")
        self.assert_lines_lint(
            ["BUGCR1234 DEBUG MAC : passes/text.html = TIMEOUT PASS"],
            "")
        self.assert_lines_lint(
            ["SLOW BUGCR1234 : passes/text.html = PASS"],
            "")
        self.assert_lines_lint(
            ["WONTFIX SKIP : passes/text.html = TIMEOUT"],
            "")

    def test_modifier_errors(self):
        self.assert_lines_lint(
            ["BUG1234 : passes/text.html = FAIL"],
            "BUG\\d+ is not allowed, must be one of BUGCR\\d+, BUGWK\\d+, BUGV8_\\d+, or a non-numeric bug identifier. passes/text.html  [test/expectations] [5]")

    def test_valid_modifiers(self):
        self.assert_lines_lint(
            ["INVALID-MODIFIER : passes/text.html = PASS"],
            "Unrecognized modifier 'invalid-modifier' "
            "passes/text.html  [test/expectations] [5]")
        self.assert_lines_lint(
            ["SKIP : passes/text.html = PASS"],
            "Test lacks BUG modifier. "
            "passes/text.html  [test/expectations] [2]")

    def test_expectation_errors(self):
        self.assert_lines_lint(
            ["missing expectations"],
            "Missing a ':' missing expectations  [test/expectations] [5]")
        self.assert_lines_lint(
            ["SLOW : passes/text.html = TIMEOUT"],
            "A test can not be both SLOW and TIMEOUT. "
            "If it times out indefinitely, then it should be just TIMEOUT. "
            "passes/text.html  [test/expectations] [5]")
        self.assert_lines_lint(
            ["BUGWK1 : does/not/exist.html = FAIL"],
            "Path does not exist. does/not/exist.html  [test/expectations] [2]")

    def test_parse_expectations(self):
        self.assert_lines_lint(
            ["BUGWK1 : passes/text.html = PASS"],
            "")
        self.assert_lines_lint(
            ["BUGWK1 : passes/text.html = UNSUPPORTED"],
            "Unsupported expectation: unsupported "
            "passes/text.html  [test/expectations] [5]")
        self.assert_lines_lint(
            ["BUGWK1 : passes/text.html = PASS UNSUPPORTED"],
            "Unsupported expectation: unsupported "
            "passes/text.html  [test/expectations] [5]")

    def test_already_seen_test(self):
        self.assert_lines_lint(
            ["BUGWK1 : passes/text.html = PASS",
             "BUGWK1 : passes/text.html = TIMEOUT"],
            "Duplicate or ambiguous expectation. %s  [test/expectations] [5]" % self._test_file)

        self.assert_lines_lint(
            ["BUGWK1 LEOPARD : passes/text.html = PASS",
             "BUGWK1 MAC : passes/text.html = TIMEOUT"],
            "More specific entry on line 1 overrides line 2 passes/text.html  [test/expectations] [5]")

        self.assert_lines_lint(
            ["BUGWK1 LEOPARD : passes/text.html = PASS",
             "BUGWK1 LEOPARD RELEASE : passes/text.html = TIMEOUT"],
            "More specific entry on line 2 overrides line 1 passes/text.html  [test/expectations] [5]")

        self.assert_lines_lint(
            ["BUGWK1 RELEASE : passes/text.html = PASS",
             "BUGWK1 CPU : passes/text.html = TIMEOUT"],
            "Entries on line 1 and line 2 match overlapping sets of configurations passes/text.html  [test/expectations] [5]")

        self.assert_lines_lint(
            ["BUGWK1 WIN : passes/text.html = PASS",
             "BUGWK1 MAC : passes/text.html = TIMEOUT"],
            "")

        self.assert_lines_lint(
            ["BUGWK1 LEOPARD DEBUG : passes/text.html = PASS",
             "BUGWK1 LEOPARD RELEASE : passes/text.html = TIMEOUT"],
            "")


    def test_tab(self):
        self.assert_lines_lint(
            ["\tBUGWK1 : passes/text.html = PASS"],
            "Line contains tab character.  [whitespace/tab] [5]")
