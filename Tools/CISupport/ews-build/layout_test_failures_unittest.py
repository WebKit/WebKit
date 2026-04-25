# Copyright (C) 2023 Apple Inc. All rights reserved.
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


import json
import unittest

from .layout_test_failures import LayoutTestFailures

sample_json = """{"tests":{"http":{"tests":{"IndexedDB":{"collect-IDB-objects.https.html":{"report":"FLAKY","expected":"PASS","actual":"TEXT PASS"}},"xmlhttprequest":{"on-network-timeout-error-during-preflight.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}}}},"transitions":{"lengthsize-transition-to-from-auto.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS"}},"imported":{"blink":{"storage":{"indexeddb":{"blob-valid-before-commit.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true}}}}},"fast":{"text":{"font-weight-fallback.html":{"report":"FLAKY","expected":"PASS","actual":"TIMEOUT PASS","has_stderr":true,"reftest_type":["=="]}},"scrolling":{"ios":{"reconcile-layer-position-recursive.html":{"report":"REGRESSION","expected":"PASS","actual":"TEXT"}}}}},"skipped":13174,"num_regressions":1,"other_crashes":{},"interrupted":false,"num_missing":0,"layout_tests_dir":"/Volumes/Data/worker/iOS-12-Simulator-WK2-Tests-EWS/build/LayoutTests","version":4,"num_passes":42158,"pixel_tests_enabled":false,"date":"11:28AM on July 16, 2019","has_pretty_patch":true,"fixable":55329,"num_flaky":5,"uses_expectations_file":true}"""

sample_failing_tests = ["fast/scrolling/ios/reconcile-layer-position-recursive.html"]

sample_flaky_tests = [
    "http/tests/IndexedDB/collect-IDB-objects.https.html",
    "http/tests/xmlhttprequest/on-network-timeout-error-during-preflight.html",
    "transitions/lengthsize-transition-to-from-auto.html",
    "imported/blink/storage/indexeddb/blob-valid-before-commit.html",
    "fast/text/font-weight-fallback.html",
]


class TestLayoutTestFailures(unittest.TestCase):
    def test_results_from_string_valid_jsonp(self):
        failures = LayoutTestFailures.results_from_string(
            f"ADD_RESULTS({sample_json});"
        )
        self.assertEqual(sample_failing_tests, failures.failing_tests)
        self.assertEqual(sample_flaky_tests, failures.flaky_tests)
        self.assertEqual(False, failures.did_exceed_test_failure_limit)

    def test_results_from_string_valid_json(self):
        failures = LayoutTestFailures.results_from_string(sample_json)
        self.assertEqual(sample_failing_tests, failures.failing_tests)
        self.assertEqual(sample_flaky_tests, failures.flaky_tests)
        self.assertEqual(False, failures.did_exceed_test_failure_limit)

    def test_results_from_string_invalid_json(self):
        failures = LayoutTestFailures.results_from_string(sample_json + " invalid JSON")
        self.assertEqual(None, failures)

    def test_results_from_string_invalid_jsonp_suffix(self):
        failures = LayoutTestFailures.results_from_string(
            f"ADD_RESULTS({sample_json}); invalid JSONP"
        )
        self.assertEqual(None, failures)

    def test_results_from_string_invalid_jsonp_suffix_valid_js(self):
        failures = LayoutTestFailures.results_from_string(
            f"ADD_RESULTS({sample_json}); null"
        )
        self.assertEqual(None, failures)

    def test_results_from_string_invalid_jsonp_internal(self):
        failures = LayoutTestFailures.results_from_string(
            f"ADD_RESULTS({sample_json} invalid JSON);"
        )
        self.assertEqual(None, failures)

    def test_results_from_string_empty(self):
        failures = LayoutTestFailures.results_from_string("")
        self.assertEqual(None, failures)

    def test_results_from_string_ws_only(self):
        failures = LayoutTestFailures.results_from_string("\x20" * 10)
        self.assertEqual(None, failures)

    def test_results_from_string_split_at_4096(self):
        # c.f. https://github.com/buildbot/buildbot/issues/4906
        long_json = f'{{"{"a" * 2500}":"{"b" * 2500}",{sample_json[1:]}'

        long_json_lines = []
        for i in range(0, len(long_json), 4096):
            long_json_lines.append(long_json[i:i + 4096])

        new_json = "\n".join(long_json_lines)

        # Check we now have invalid JSON
        try:
            json.loads(new_json)
        except json.JSONDecodeError:
            pass
        else:
            self.assertTrue(False, msg="Unreachable!")

        failures = LayoutTestFailures.results_from_string(new_json)
        self.assertEqual(sample_failing_tests, failures.failing_tests)
        self.assertEqual(sample_flaky_tests, failures.flaky_tests)
        self.assertEqual(False, failures.did_exceed_test_failure_limit)
