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

from webkitpy.common.net.apitestresults import APITestResults


class APITestResultsTest(unittest.TestCase):
    def test_results_from_string(self):
        incomplete_json_v1 = '{"failures": []}'
        incomplete_json_v2 = '{"timeouts":[]}'
        self.assertEqual(None, APITestResults.results_from_string(incomplete_json_v1))
        self.assertEqual(None, APITestResults.results_from_string(incomplete_json_v2))

        no_failures_string = '{"timeouts": [], "failures":[]}'
        no_failures_results = APITestResults([], [])
        self.assertTrue(no_failures_results.equals(APITestResults.results_from_string(no_failures_string)))

        many_failures_string = '{"timeouts":["TimeRanges.AddOrder"], "failures": ["FloatPoint.Math", "URLParserTest.Basic"]}'
        many_failures_results = APITestResults(["FloatPoint.Math", "URLParserTest.Basic"], ["TimeRanges.AddOrder"])
        self.assertTrue(many_failures_results.equals(APITestResults.results_from_string(many_failures_string)))

        self.assertFalse(no_failures_results.equals(many_failures_results))

    def test_intersection(self):
        results1 = APITestResults(['failure1', 'failure2'], ['timeout1'])
        results2 = APITestResults(['failure1'], ['timeout1', 'timeout2'])

        expected_intersection = APITestResults(['failure1'], ['timeout1'])
        self.assertTrue(expected_intersection.equals(APITestResults.intersection(results1, results2)))
