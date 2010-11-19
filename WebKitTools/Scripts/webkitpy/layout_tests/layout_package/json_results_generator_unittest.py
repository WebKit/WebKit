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
        self._tests_set = set([])
        self._test_timings = {}
        self._failed_tests = set([])

        self._PASS_tests = set([])
        self._DISABLED_tests = set([])
        self._FLAKY_tests = set([])
        self._FAILS_tests = set([])

    def _test_json_generation(self, passed_tests_list, failed_tests_list):
        tests_set = set(passed_tests_list) | set(failed_tests_list)

        DISABLED_tests = set([t for t in tests_set
                             if t.startswith('DISABLED_')])
        FLAKY_tests = set([t for t in tests_set
                           if t.startswith('FLAKY_')])
        FAILS_tests = set([t for t in tests_set
                           if t.startswith('FAILS_')])
        PASS_tests = tests_set - (DISABLED_tests | FLAKY_tests | FAILS_tests)

        passed_tests = set(passed_tests_list) - DISABLED_tests
        failed_tests = set(failed_tests_list)

        test_timings = {}
        i = 0
        for test in tests_set:
            test_timings[test] = float(self._num_runs * 100 + i)
            i += 1

        # For backward compatibility.
        reason = test_expectations.TEXT
        failed_tests_dict = dict([(name, reason) for name in failed_tests])

        port_obj = port.get(None)
        generator = json_results_generator.JSONResultsGenerator(port_obj,
            self.builder_name, self.build_name, self.build_number,
            '',
            None,   # don't fetch past json results archive
            test_timings,
            failed_tests_dict,
            passed_tests,
            (),
            tests_set)

        # Test incremental json results
        incremental_json = generator.get_json(incremental=True)
        self._verify_json_results(
            tests_set,
            test_timings,
            failed_tests,
            PASS_tests,
            DISABLED_tests,
            FLAKY_tests,
            incremental_json,
            1)

        # Test aggregated json results
        generator.set_archived_results(self._json)
        json = generator.get_json(incremental=False)
        self._json = json
        self._num_runs += 1
        self._tests_set |= tests_set
        self._test_timings.update(test_timings)
        self._failed_tests.update(failed_tests)
        self._PASS_tests |= PASS_tests
        self._DISABLED_tests |= DISABLED_tests
        self._FLAKY_tests |= FLAKY_tests
        self._verify_json_results(
            self._tests_set,
            self._test_timings,
            self._failed_tests,
            self._PASS_tests,
            self._DISABLED_tests,
            self._FLAKY_tests,
            self._json,
            self._num_runs)

    def _verify_json_results(self, tests_set, test_timings, failed_tests,
                             PASS_tests, DISABLED_tests, FLAKY_tests,
                             json, num_runs):
        # Aliasing to a short name for better access to its constants.
        JRG = json_results_generator.JSONResultsGenerator

        self.assertTrue(JRG.VERSION_KEY in json)
        self.assertTrue(self.builder_name in json)

        buildinfo = json[self.builder_name]
        self.assertTrue(JRG.FIXABLE in buildinfo)
        self.assertTrue(JRG.TESTS in buildinfo)
        self.assertEqual(len(buildinfo[JRG.BUILD_NUMBERS]), num_runs)
        self.assertEqual(buildinfo[JRG.BUILD_NUMBERS][0], self.build_number)

        if tests_set or DISABLED_tests:
            fixable = {}
            for fixable_items in buildinfo[JRG.FIXABLE]:
                for (type, count) in fixable_items.iteritems():
                    if type in fixable:
                        fixable[type] = fixable[type] + count
                    else:
                        fixable[type] = count

            if PASS_tests:
                self.assertEqual(fixable[JRG.PASS_RESULT], len(PASS_tests))
            else:
                self.assertTrue(JRG.PASS_RESULT not in fixable or
                                fixable[JRG.PASS_RESULT] == 0)
            if DISABLED_tests:
                self.assertEqual(fixable[JRG.SKIP_RESULT], len(DISABLED_tests))
            else:
                self.assertTrue(JRG.SKIP_RESULT not in fixable or
                                fixable[JRG.SKIP_RESULT] == 0)
            if FLAKY_tests:
                self.assertEqual(fixable[JRG.FLAKY_RESULT], len(FLAKY_tests))
            else:
                self.assertTrue(JRG.FLAKY_RESULT not in fixable or
                                fixable[JRG.FLAKY_RESULT] == 0)

        if failed_tests:
            tests = buildinfo[JRG.TESTS]
            for test_name in failed_tests:
                self.assertTrue(test_name in tests)
                test = tests[test_name]

                failed = 0
                for result in test[JRG.RESULTS]:
                    if result[1] == JRG.FAIL_RESULT:
                        failed = result[0]

                self.assertEqual(1, failed)

                timing_count = 0
                for timings in test[JRG.TIMES]:
                    if timings[1] == test_timings[test_name]:
                        timing_count = timings[0]
                self.assertEqual(1, timing_count)

        fixable_count = len(DISABLED_tests | failed_tests)
        if DISABLED_tests or failed_tests:
            self.assertEqual(sum(buildinfo[JRG.FIXABLE_COUNT]), fixable_count)

    def test_json_generation(self):
        self._test_json_generation([], [])
        self._test_json_generation(['A1', 'B1'], [])
        self._test_json_generation([], ['FAILS_A2', 'FAILS_B2'])
        self._test_json_generation(['DISABLED_A3', 'DISABLED_B3'], [])
        self._test_json_generation(['A4'], ['B4', 'FAILS_C4'])
        self._test_json_generation(['DISABLED_C5', 'DISABLED_D5'], ['A5', 'B5'])
        self._test_json_generation(
            ['A6', 'B6', 'FAILS_C6', 'DISABLED_E6', 'DISABLED_F6'],
            ['FAILS_D6'])
        self._test_json_generation(
            ['A7', 'FLAKY_B7', 'DISABLED_C7'],
            ['FAILS_D7', 'FLAKY_D8'])

if __name__ == '__main__':
    unittest.main()
