# Copyright (C) 2017 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging

from webkitpy.common.net.abstracttestresults import AbstractTestResults

_log = logging.getLogger(__name__)


class JSCTestResults(AbstractTestResults):
    def __init__(self, all_api_tests_passed, stress_test_failures):
        self._all_api_tests_passed = all_api_tests_passed
        self._stress_test_failures = stress_test_failures

        self._failing_test_names = stress_test_failures[:]
        if not self._all_api_tests_passed:
            self._failing_test_names.append('apiTests')

    @classmethod
    def intersection(cls, first, second):
        intersection_api_tests_passed = first._all_api_tests_passed or second._all_api_tests_passed
        intersection_stress_test_failures = list(set(first._stress_test_failures) & set(second._stress_test_failures))
        return cls(intersection_api_tests_passed, intersection_stress_test_failures)

    @classmethod
    def results_from_string(cls, string):
        parsed_results = cls.parse_json_string(string)
        if not parsed_results:
            return None

        if 'allApiTestsPassed' not in parsed_results or 'stressTestFailures' not in parsed_results:
            return None

        return cls(parsed_results['allApiTestsPassed'], parsed_results['stressTestFailures'])

    def equals(self, other):
        return (self._all_api_tests_passed == other._all_api_tests_passed and
               set(self._stress_test_failures) == set(other._stress_test_failures))

    def is_subset(self, other):
        return set(self._failing_test_names) <= set(other._failing_test_names)

    def all_passed(self):
        return self._all_api_tests_passed and not self._stress_test_failures

    def failing_tests(self):
        return self._failing_test_names

    # No defined failure limit for JSC.
    def did_exceed_test_failure_limit(self):
        return False
