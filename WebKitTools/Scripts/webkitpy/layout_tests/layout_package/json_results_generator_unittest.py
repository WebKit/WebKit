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

"""Unit tests for json_results_generator.py."""

import unittest
import optparse
import random
import shutil
import tempfile

from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.layout_package import test_expectations
from webkitpy.layout_tests import port


class JSONGeneratorTest(unittest.TestCase):
    def setUp(self):
        json_results_generator.JSONResultsGenerator.output_json_in_init = False
        self.builder_name = 'DUMMY_BUILDER_NAME'
        self.build_name = 'DUMMY_BUILD_NAME'
        self.build_number = 'DUMMY_BUILDER_NUMBER'
        self._json = None
        self._num_runs = 0
        self._tests_list = set([])
        self._test_timings = {}
        self._failed_tests = {}
        self._passed_tests = set([])
        self._skipped_tests = set([])

    def _test_json_generation(self, passed_tests, failed_tests, skipped_tests):
        # Make sure we have sets (rather than lists).
        passed_tests = set(passed_tests)
        skipped_tests = set(skipped_tests)
        tests_list = passed_tests | set(failed_tests.keys())
        test_timings = {}
        i = 0
        for test in tests_list:
            test_timings[test] = float(self._num_runs * 100 + i)
            i += 1

        port_obj = port.get(None)

        generator = json_results_generator.JSONResultsGenerator(port_obj,
            self.builder_name, self.build_name, self.build_number,
            '',
            None,   # don't fetch past json results archive
            test_timings,
            failed_tests,
            passed_tests,
            skipped_tests,
            tests_list)

        # Test incremental json results
        incremental_json = generator.get_json(incremental=True)
        self._verify_json_results(
            tests_list,
            test_timings,
            passed_tests,
            failed_tests,
            skipped_tests,
            incremental_json,
            1)

        # Test aggregated json results
        generator.set_archived_results(self._json)
        json = generator.get_json(incremental=False)
        self._json = json
        self._num_runs += 1
        self._tests_list |= tests_list
        self._test_timings.update(test_timings)
        self._failed_tests.update(failed_tests)
        self._passed_tests |= passed_tests
        self._skipped_tests |= skipped_tests
        self._verify_json_results(
            self._tests_list,
            self._test_timings,
            self._passed_tests,
            self._failed_tests,
            self._skipped_tests,
            self._json,
            self._num_runs)

    def _verify_json_results(self, tests_list, test_timings,
                             passed_tests, failed_tests,
                             skipped_tests, json, num_runs):
        # Aliasing to a short name for better access to its constants.
        JRG = json_results_generator.JSONResultsGenerator

        self.assertTrue(JRG.VERSION_KEY in json)
        self.assertTrue(self.builder_name in json)

        buildinfo = json[self.builder_name]
        self.assertTrue(JRG.FIXABLE in buildinfo)
        self.assertTrue(JRG.TESTS in buildinfo)
        self.assertTrue(len(buildinfo[JRG.BUILD_NUMBERS]) == num_runs)
        self.assertTrue(buildinfo[JRG.BUILD_NUMBERS][0] == self.build_number)

        if tests_list or skipped_tests:
            fixable = {}
            for fixable_items in buildinfo[JRG.FIXABLE]:
                for (type, count) in fixable_items.iteritems():
                    if type in fixable:
                        fixable[type] = fixable[type] + count
                    else:
                        fixable[type] = count

            if passed_tests:
                self.assertTrue(fixable[JRG.PASS_RESULT] == len(passed_tests))
            else:
                self.assertTrue(JRG.PASS_RESULT not in fixable or
                                fixable[JRG.PASS_RESULT] == 0)
            if skipped_tests:
                self.assertTrue(fixable[JRG.SKIP_RESULT] == len(skipped_tests))
            else:
                self.assertTrue(JRG.SKIP_RESULT not in fixable or
                                fixable[JRG.SKIP_RESULT] == 0)

        if failed_tests:
            tests = buildinfo[JRG.TESTS]
            for test_name, failure in failed_tests.iteritems():
                self.assertTrue(test_name in tests)
                test = tests[test_name]

                failed = 0
                for result in test[JRG.RESULTS]:
                    if result[1] == JRG.FAIL_RESULT:
                        failed = result[0]
                self.assertTrue(failed == 1)

                timing_count = 0
                for timings in test[JRG.TIMES]:
                    if timings[1] == test_timings[test_name]:
                        timing_count = timings[0]
                self.assertTrue(timing_count == 1)

        fixable_count = len(skipped_tests) + len(failed_tests.keys())
        if skipped_tests or failed_tests:
            self.assertTrue(sum(buildinfo[JRG.FIXABLE_COUNT]) == fixable_count)

    def test_json_generation(self):
        reason = test_expectations.TEXT

        self._test_json_generation([], {}, [])
        self._test_json_generation(['A1', 'B1'], {}, [])
        self._test_json_generation([], {'A2': reason, 'B2': reason}, [])
        self._test_json_generation([], {}, ['A3', 'B3'])
        self._test_json_generation(['A4'], {'B4': reason, 'C4': reason}, [])
        self._test_json_generation(
            [], {'A5': reason, 'B5': reason}, ['C5', 'D5'])
        self._test_json_generation(
            ['A6', 'B6', 'C6'], {'D6': reason}, ['E6', 'F6'])


if __name__ == '__main__':
    unittest.main()
