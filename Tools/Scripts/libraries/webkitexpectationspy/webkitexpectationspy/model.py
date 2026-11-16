# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

"""In-memory model of test expectations with lookup support."""

from typing import Dict, List, Set, Optional

from webkitexpectationspy.expectations import Expectation, ResultStatus
from webkitexpectationspy.parser import ParseWarning


class ExpectationsModel:
    def __init__(self, suite=None):
        self._suite = suite
        self._exact_expectations = {}
        self._wildcard_expectations = []
        self._all_expectations = []
        self._seen_patterns = {}

    def add_expectation(self, expectation):
        warning = None

        key = (expectation.test_pattern, expectation.configurations, expectation.version_specifiers)
        if key in self._seen_patterns:
            prev_file, prev_line = self._seen_patterns[key]
            if prev_file == expectation.filename:
                warning = ParseWarning(
                    expectation.filename,
                    expectation.line_number,
                    expectation.test_pattern,
                    'Duplicate expectation (previous at line {})'.format(prev_line),
                    test=expectation.test_pattern
                )

        self._seen_patterns[key] = (expectation.filename, expectation.line_number)
        self._all_expectations.append(expectation)

        is_wildcard = (
            self._suite.is_wildcard_pattern(expectation.test_pattern)
            if self._suite else expectation.test_pattern.endswith('*')
        )

        if is_wildcard:
            self._wildcard_expectations.append(expectation)
        else:
            if expectation.test_pattern not in self._exact_expectations:
                self._exact_expectations[expectation.test_pattern] = []
            self._exact_expectations[expectation.test_pattern].append(expectation)

        return warning

    def get_expectation(self, test_name, current_config=None,
                        current_version=None, version_order=None):
        if current_config is None:
            current_config = set()

        if test_name in self._exact_expectations:
            for exp in self._exact_expectations[test_name]:
                if exp.matches_configuration(current_config, current_version, version_order):
                    return exp

        best_match = None
        best_prefix_len = -1
        for exp in self._wildcard_expectations:
            if self._matches_test(exp, test_name):
                if exp.matches_configuration(current_config, current_version, version_order):
                    prefix_len = len(exp.test_pattern)
                    if prefix_len > best_prefix_len:
                        best_match = exp
                        best_prefix_len = prefix_len

        return best_match

    def _matches_test(self, expectation, test_name):
        if self._suite:
            return self._suite.matches_test(expectation.test_pattern, test_name)
        return expectation.matches_test(test_name)

    def get_expectation_or_pass(self, test_name, current_config=None,
                                current_version=None, version_order=None):
        exp = self.get_expectation(test_name, current_config, current_version, version_order)
        if exp:
            return exp
        return Expectation(test_name, expected={ResultStatus.PASS})

    def get_skipped_tests(self, all_tests, current_config=None,
                          current_version=None, version_order=None):
        skipped = set()
        for test in all_tests:
            exp = self.get_expectation(test, current_config, current_version, version_order)
            if exp and exp.skip:
                skipped.add(test)
        return skipped

    def get_slow_tests(self, all_tests, current_config=None,
                       current_version=None, version_order=None):
        slow_tests = {}
        for test in all_tests:
            exp = self.get_expectation(test, current_config, current_version, version_order)
            if exp and exp.slow:
                timeout = exp.modifiers.slow
                slow_tests[test] = None if (timeout is not None and timeout < 0) else timeout
        return slow_tests

    def get_wontfix_tests(self, all_tests, current_config=None,
                          current_version=None, version_order=None):
        wontfix = set()
        for test in all_tests:
            exp = self.get_expectation(test, current_config, current_version, version_order)
            if exp and exp.is_wontfix():
                wontfix.add(test)
        return wontfix

    def all_expectations(self):
        return self._all_expectations

    def clear(self):
        self._exact_expectations.clear()
        self._wildcard_expectations.clear()
        self._all_expectations.clear()
        self._seen_patterns.clear()
