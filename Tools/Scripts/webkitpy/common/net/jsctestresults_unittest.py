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

import unittest

from webkitpy.common.net.jsctestresults import JSCTestResults


class JSCTestResultsTest(unittest.TestCase):
    def test_results_from_string(self):
        incomplete_json_v1 = '{"allApiTestsPassed": true}'
        incomplete_json_v2 = '{"stressTestFailures":[]}'
        self.assertEqual(None, JSCTestResults.results_from_string(incomplete_json_v1))
        self.assertEqual(None, JSCTestResults.results_from_string(incomplete_json_v2))

        no_failures_string = '{"allApiTestsPassed": true, "stressTestFailures":[]}'
        no_failures_results = JSCTestResults(True, [])
        self.assertTrue(no_failures_results.equals(JSCTestResults.results_from_string(no_failures_string)))

        api_test_failures_string = '{"allApiTestsPassed": false, "stressTestFailures":[]}'
        api_test_failures_results = JSCTestResults(False, [])
        self.assertTrue(api_test_failures_results.equals(JSCTestResults.results_from_string(api_test_failures_string)))

        many_failures_string = '{"allApiTestsPassed": false, "stressTestFailures":["es6.yaml/es6/typed_arrays_Int16Array.js.default", "es6.yaml/es6/typed_arrays_Int8Array.js.default"]}'
        many_failures_results = JSCTestResults(False, ["es6.yaml/es6/typed_arrays_Int16Array.js.default", "es6.yaml/es6/typed_arrays_Int8Array.js.default"])
        self.assertTrue(many_failures_results.equals(JSCTestResults.results_from_string(many_failures_string)))

        self.assertFalse(no_failures_results == api_test_failures_results)
        self.assertFalse(api_test_failures_results == many_failures_results)

    def test_intersection_api_tests(self):
        results1 = JSCTestResults(False, [])
        results2 = JSCTestResults(True, [])

        expected_intersection = JSCTestResults(True, [])
        self.assertTrue(expected_intersection.equals(JSCTestResults.intersection(results1, results2)))

    def test_intersection_stress_tests(self):
        results1 = JSCTestResults(True, ['failure1', 'failure2'])
        results2 = JSCTestResults(True, ['failure1', 'failure3'])

        expected_intersection = JSCTestResults(True, ['failure1'])
        self.assertTrue(expected_intersection.equals(JSCTestResults.intersection(results1, results2)))

    def test_intersection_general_case(self):
        results1 = JSCTestResults(True, ['failure1', 'failure2'])
        results2 = JSCTestResults(False, ['failure1'])

        expected_intersection = JSCTestResults(True, ['failure1'])
        self.assertTrue(expected_intersection.equals(JSCTestResults.intersection(results1, results2)))
