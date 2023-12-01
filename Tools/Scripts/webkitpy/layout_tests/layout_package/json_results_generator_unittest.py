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

import json
import unittest

from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.models.test_results import TestResult


class JSONGeneratorTest(unittest.TestCase):
    def setUp(self):
        # For archived results.
        self._json = None
        self._num_runs = 0
        self._tests_set = set([])
        self._test_timings = {}
        self._failed_count_map = {}

        self._PASS_count = 0
        self._DISABLED_count = 0
        self._FLAKY_count = 0
        self._FAILS_count = 0
        self._fixable_count = 0

    def test_strip_json_wrapper(self):
        json = "['contents']"
        self.assertEqual(json_results_generator.strip_json_wrapper(json_results_generator._JSON_PREFIX + json + json_results_generator._JSON_SUFFIX), json)
        self.assertEqual(json_results_generator.strip_json_wrapper(json), json)

    def test_perf_metrics_for_test(self):
        individual_test_timings = []
        individual_test_timings.append(TestResult('foo/bar/baz.html', test_run_time=1.2))
        individual_test_timings.append(TestResult('foo/bar/ba.html', test_run_time=1.4))
        individual_test_timings.append(TestResult('bar.html', test_run_time=0.0001))
        metrics = json_results_generator.perf_metrics_for_test(1200, individual_test_timings)

        expected_metrics = {
            "layout_tests": {
                "metrics": {
                    "Time": ["Total", "Arithmetic"],
                },
                "tests": {
                    "foo": {
                        "metrics": {
                            "Time": ["Total", "Arithmetic"],
                        },
                        "tests": {
                            "bar":  {
                                "metrics": {
                                    "Time": {"current": [2600]},
                                }
                            }
                        }
                    },
                    "bar.html": {
                        "metrics": {
                            "Time": {"current": [0]},
                        }
                    }
                }
            },
            "layout_tests_run_time": {
                "metrics": {
                    "Time": {"current": [1200]},
                }
            }}
        self.assertEqual(json.dumps(metrics), json.dumps(expected_metrics))
