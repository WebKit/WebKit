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

import logging

from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures

class JSONLayoutResultsGenerator(json_results_generator.JSONResultsGeneratorBase):
    """A JSON results generator for layout tests."""

    LAYOUT_TESTS_PATH = "LayoutTests"

    # Additional JSON fields.
    WONTFIX = "wontfixCounts"

    FAILURE_TO_CHAR = {test_expectations.PASS: json_results_generator.JSONResultsGeneratorBase.PASS_RESULT,
                       test_expectations.SKIP: json_results_generator.JSONResultsGeneratorBase.SKIP_RESULT,
                       test_expectations.FAIL: "Y",
                       test_expectations.CRASH: "C",
                       test_expectations.TIMEOUT: "T",
                       test_expectations.IMAGE: "I",
                       test_expectations.TEXT: "F",
                       test_expectations.MISSING: "O",
                       test_expectations.AUDIO: "A",
                       test_expectations.IMAGE_PLUS_TEXT: "Z"}

    def __init__(self, port, builder_name, build_name, build_number,
        results_file_base_path, builder_base_url,
        test_timings, expectations, result_summary, all_tests,
        test_results_server=None, test_type="", master_name=""):
        """Modifies the results.json file. Grabs it off the archive directory
        if it is not found locally.

        Args:
          result_summary: ResultsSummary object storing the summary of the test
              results.
        """
        super(JSONLayoutResultsGenerator, self).__init__(
            port, builder_name, build_name, build_number, results_file_base_path,
            builder_base_url, {}, port.repository_paths(),
            test_results_server, test_type, master_name)

        self._expectations = expectations

        self._result_summary = result_summary
        self._failures = dict((test_name, test_failures.determine_result_type(failures))
            for (test_name, failures) in result_summary.failures.iteritems())
        self._all_tests = all_tests
        self._test_timings = dict((test_tuple.test_name, test_tuple.test_run_time) for test_tuple in test_timings)

        self.generate_json_output()

    def _get_path_relative_to_layout_test_root(self, test):
        """Returns the path of the test relative to the layout test root.
        For example, for:
          src/third_party/WebKit/LayoutTests/fast/forms/foo.html
        We would return
          fast/forms/foo.html
        """
        index = test.find(self.LAYOUT_TESTS_PATH)
        if index is not -1:
            index += len(self.LAYOUT_TESTS_PATH)

        if index is -1:
            # Already a relative path.
            relativePath = test
        else:
            relativePath = test[index + 1:]

        # Make sure all paths are unix-style.
        return relativePath.replace('\\', '/')

    # override
    def _get_test_timing(self, test_name):
        if test_name in self._test_timings:
            # Floor for now to get time in seconds.
            return int(self._test_timings[test_name])
        return 0

    # override
    def _get_failed_test_names(self):
        return set(self._failures.keys())

    # override
    def _get_modifier_char(self, test_name):
        if test_name not in self._all_tests:
            return self.NO_DATA_RESULT

        if test_name in self._failures:
            return self.FAILURE_TO_CHAR[self._failures[test_name]]

        return self.PASS_RESULT

    # override
    def _get_result_char(self, test_name):
        return self._get_modifier_char(test_name)

    # override
    def _insert_failure_summaries(self, results_for_builder):
        summary = self._result_summary

        self._insert_item_into_raw_list(results_for_builder,
            len((set(summary.failures.keys()) |
                summary.tests_by_expectation[test_expectations.SKIP]) &
                summary.tests_by_timeline[test_expectations.NOW]),
            self.FIXABLE_COUNT)
        self._insert_item_into_raw_list(results_for_builder,
            self._get_failure_summary_entry(test_expectations.NOW),
            self.FIXABLE)
        self._insert_item_into_raw_list(results_for_builder,
            len(self._expectations.get_tests_with_timeline(
                test_expectations.NOW)), self.ALL_FIXABLE_COUNT)
        self._insert_item_into_raw_list(results_for_builder,
            self._get_failure_summary_entry(test_expectations.WONTFIX),
            self.WONTFIX)

    # override
    def _normalize_results_json(self, test, test_name, tests):
        super(JSONLayoutResultsGenerator, self)._normalize_results_json(
            test, test_name, tests)

        # Remove tests that don't exist anymore.
        full_path = self._filesystem.join(self._port.layout_tests_dir(), test_name)
        full_path = self._filesystem.normpath(full_path)
        if not self._filesystem.exists(full_path):
            del tests[test_name]

    def _get_failure_summary_entry(self, timeline):
        """Creates a summary object to insert into the JSON.

        Args:
          summary   ResultSummary object with test results
          timeline  current test_expectations timeline to build entry for
                    (e.g., test_expectations.NOW, etc.)
        """
        entry = {}
        summary = self._result_summary
        timeline_tests = summary.tests_by_timeline[timeline]
        entry[self.SKIP_RESULT] = len(
            summary.tests_by_expectation[test_expectations.SKIP] &
            timeline_tests)
        entry[self.PASS_RESULT] = len(
            summary.tests_by_expectation[test_expectations.PASS] &
            timeline_tests)
        for failure_type in summary.tests_by_expectation.keys():
            if failure_type not in self.FAILURE_TO_CHAR:
                continue
            count = len(summary.tests_by_expectation[failure_type] &
                        timeline_tests)
            entry[self.FAILURE_TO_CHAR[failure_type]] = count
        return entry
