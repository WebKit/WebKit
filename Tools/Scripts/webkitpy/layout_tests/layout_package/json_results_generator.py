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
import logging

from webkitpy.common.iteration_compatibility import iteritems, iterkeys

# A JSON results generator for generic tests.
# FIXME: move this code out of the layout_package directory.

_log = logging.getLogger(__name__)

_JSON_PREFIX = "ADD_RESULTS("
_JSON_SUFFIX = ");"
_JSON_PREFIX_B = b"ADD_RESULTS("
_JSON_SUFFIX_B = b");"


def has_json_wrapper(string):
    if isinstance(string, bytes):
        return string.startswith(_JSON_PREFIX_B) and string.endswith(_JSON_SUFFIX_B)
    else:
        return string.startswith(_JSON_PREFIX) and string.endswith(_JSON_SUFFIX)


def strip_json_wrapper(json_content):
    # FIXME: Kill this code once the server returns json instead of jsonp.
    if has_json_wrapper(json_content):
        return json_content[len(_JSON_PREFIX):len(json_content) - len(_JSON_SUFFIX)]
    return json_content


def load_json(filesystem, file_path):
    content = filesystem.read_text_file(file_path)
    content = strip_json_wrapper(content)
    return json.loads(content)


def write_json(filesystem, json_object, file_path, callback=None):
    # Specify separators in order to get compact encoding.
    json_string = json.dumps(json_object, separators=(',', ':'))
    if callback:
        json_string = callback + "(" + json_string + ");"
    filesystem.write_text_file(file_path, json_string)


def convert_trie_to_flat_paths(trie, prefix=None):
    """Converts the directory structure in the given trie to flat paths, prepending a prefix to each."""
    result = {}
    for name, data in iteritems(trie):
        if prefix:
            name = prefix + "/" + name

        if len(data) and not "results" in data:
            result.update(convert_trie_to_flat_paths(data, name))
        else:
            result[name] = data

    return result


def add_path_to_trie(path, value, trie):
    """Inserts a single flat directory path and associated value into a directory trie structure."""
    if not "/" in path:
        trie[path] = value
        return

    directory, slash, rest = path.partition("/")
    if not directory in trie:
        trie[directory] = {}
    add_path_to_trie(rest, value, trie[directory])


def test_timings_trie(port, individual_test_timings):
    """Breaks a test name into chunks by directory and puts the test time as a value in the lowest part, e.g.
    foo/bar/baz.html: 1ms
    foo/bar/baz1.html: 3ms

    becomes
    foo: {
        bar: {
            baz.html: 1,
            baz1.html: 3
        }
    }
    """
    trie = {}
    for test_result in individual_test_timings:
        test = test_result.test_name
        add_path_to_trie(test, int(1000 * test_result.test_run_time), trie)

    return trie


def _add_perf_metric_for_test(path, time, tests, depth, depth_limit):
    """
    Aggregate test time to result for a given test at a specified depth_limit.
    """
    if not "/" in path:
        tests["tests"][path] = {
            "metrics": {
                "Time": {
                    "current": [time],
                }}}
        return

    directory, slash, rest = path.partition("/")
    if depth == depth_limit:
        if directory not in tests["tests"]:
            tests["tests"][directory] = {
                "metrics": {
                    "Time": {
                        "current": [time],
                    }}}
        else:
            tests["tests"][directory]["metrics"]["Time"]["current"][0] += time
        return
    else:
        if directory not in tests["tests"]:
            tests["tests"][directory] = {
                "metrics": {
                    "Time": ["Total", "Arithmetic"],
                },
                "tests": {}
            }
        _add_perf_metric_for_test(rest, time, tests["tests"][directory], depth + 1, depth_limit)


def perf_metrics_for_test(run_time, individual_test_timings):
    """
    Output two performace metrics
    1. run time, which is how much time consumed by the layout tests script
    2. run time of first-level and second-level of test directories
    """
    total_run_time = 0

    for test_result in individual_test_timings:
        total_run_time += int(1000 * test_result.test_run_time)

    perf_metric = {
        "layout_tests": {
            "metrics": {
                "Time": ["Total", "Arithmetic"],
            },
            "tests": {}
        },
        "layout_tests_run_time": {
            "metrics": {
                "Time": {"current": [run_time]},
            }}}
    for test_result in individual_test_timings:
        test = test_result.test_name
        # for now, we only send two levels of directories
        _add_perf_metric_for_test(test, int(1000 * test_result.test_run_time), perf_metric["layout_tests"], 1, 2)
    return perf_metric


# FIXME: We already have a TestResult class in test_results.py
class TestResult(object):
    """A simple class that represents a single test result."""

    # Test modifier constants.
    (NONE, FAILS, FLAKY, DISABLED) = range(4)

    def __init__(self, test, failed=False, elapsed_time=0):
        self.test_name = test
        self.failed = failed
        self.test_run_time = elapsed_time

        test_name = test
        try:
            test_name = test.split('.')[1]
        except IndexError:
            _log.warn("Invalid test name: %s.", test)
            pass

        if test_name.startswith('FAILS_'):
            self.modifier = self.FAILS
        elif test_name.startswith('FLAKY_'):
            self.modifier = self.FLAKY
        elif test_name.startswith('DISABLED_'):
            self.modifier = self.DISABLED
        else:
            self.modifier = self.NONE

    def fixable(self):
        return self.failed or self.modifier == self.DISABLED


class JSONResultsGenerator(object):
    """A JSON results generator for generic tests."""

    MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG = 750
    # Min time (seconds) that will be added to the JSON.
    MIN_TIME = 1

    # Note that in non-chromium tests those chars are used to indicate
    # test modifiers (FAILS, FLAKY, etc) but not actual test results.
    PASS_RESULT = "P"
    SKIP_RESULT = "X"
    FAIL_RESULT = "F"
    FLAKY_RESULT = "L"
    NO_DATA_RESULT = "N"

    MODIFIER_TO_CHAR = {TestResult.NONE: PASS_RESULT,
                        TestResult.DISABLED: SKIP_RESULT,
                        TestResult.FAILS: FAIL_RESULT,
                        TestResult.FLAKY: FLAKY_RESULT}

    RESULTS = "results"
    TIMES = "times"
    TIME = "secondsSinceEpoch"
    TESTS = "tests"

    FIXABLE_COUNT = "fixableCount"
    FIXABLE = "fixableCounts"
    ALL_FIXABLE_COUNT = "allFixableCount"

    RESULTS_FILENAME = "results.json"
    TIMES_MS_FILENAME = "times_ms.json"
    INCREMENTAL_RESULTS_FILENAME = "incremental_results.json"

    URL_FOR_TEST_LIST_JSON = "http://%s/testfile?builder=%s&name=%s&testlistjson=1&testtype=%s&master=%s"

    def __init__(self, port,
        results_file_base_path,
        test_results_map, svn_repositories=None,
        test_type=""):
        """Modifies the results.json file. Grabs it off the archive directory
        if it is not found locally.

        Args
          port: port-specific wrapper
          results_file_base_path: Absolute path to the directory containing the
              results json file.
          test_results_map: A dictionary that maps test_name to TestResult.
          svn_repositories: A (json_field_name, svn_path) pair for SVN
              repositories that tests rely on.  The SVN revision will be
              included in the JSON with the given json_field_name.
          test_type: test type string (e.g. 'layout-tests').
        """
        self._port = port
        self._filesystem = port._filesystem
        self._executive = port._executive
        self._results_directory = results_file_base_path

        self._test_results_map = test_results_map
        self._test_results = test_results_map.values()

        self._svn_repositories = svn_repositories
        if not self._svn_repositories:
            self._svn_repositories = {}

        self._test_type = test_type

        self._archived_results = None

    def generate_json_output(self):
        json_object = self.get_json()
        if not json_object:
            return False
        file_path = self._filesystem.join(self._results_directory, self.INCREMENTAL_RESULTS_FILENAME)
        write_json(self._filesystem, json_object, file_path)
        return True

    def generate_times_ms_file(self):
        times = test_timings_trie(self._port, self._test_results_map.values())
        file_path = self._filesystem.join(self._results_directory, self.TIMES_MS_FILENAME)
        write_json(self._filesystem, times, file_path)

    def get_json(self):
        """Gets the results for the results.json file."""
        results_for_builder = self._create_results_for_builder_json()
        self._insert_failure_summaries(results_for_builder)

        # Update the all failing tests with result type and time.
        tests = results_for_builder[self.TESTS]
        all_failing_tests = self._get_failed_test_names()
        all_failing_tests.update(convert_trie_to_flat_paths(tests))

        for test in all_failing_tests:
            self._insert_test_time_and_result(test, tests)

        return dict(
            results=results_for_builder,
        )

    def set_archived_results(self, archived_results):
        self._archived_results = archived_results

    def _get_test_timing(self, test_name):
        """Returns test timing data (elapsed time) in second
        for the given test_name."""
        if test_name in self._test_results_map:
            # Floor for now to get time in seconds.
            return int(self._test_results_map[test_name].test_run_time)
        return 0

    def _get_failed_test_names(self):
        """Returns a set of failed test names."""
        return set([r.test_name for r in self._test_results if r.failed])

    def _get_modifier_char(self, test_name):
        """Returns a single char (e.g. SKIP_RESULT, FAIL_RESULT,
        PASS_RESULT, NO_DATA_RESULT, etc) that indicates the test modifier
        for the given test_name.
        """
        if test_name not in self._test_results_map:
            return self.__class__.NO_DATA_RESULT

        test_result = self._test_results_map[test_name]
        if test_result.modifier in self.MODIFIER_TO_CHAR.keys():
            return self.MODIFIER_TO_CHAR[test_result.modifier]

        return self.__class__.PASS_RESULT

    def _get_result_char(self, test_name):
        """Returns a single char (e.g. SKIP_RESULT, FAIL_RESULT,
        PASS_RESULT, NO_DATA_RESULT, etc) that indicates the test result
        for the given test_name.
        """
        if test_name not in self._test_results_map:
            return self.__class__.NO_DATA_RESULT

        test_result = self._test_results_map[test_name]
        if test_result.modifier == TestResult.DISABLED:
            return self.__class__.SKIP_RESULT

        if test_result.failed:
            return self.__class__.FAIL_RESULT

        return self.__class__.PASS_RESULT

    def _insert_failure_summaries(self, results_for_builder):
        """Inserts aggregate pass/failure statistics into the JSON.
        This method reads self._test_results and generates
        FIXABLE, FIXABLE_COUNT and ALL_FIXABLE_COUNT entries.

        Args:
          results_for_builder: Dictionary containing the test results for a
              single builder.
        """
        # Insert the number of tests that failed or skipped.
        fixable_count = len([r for r in self._test_results if r.fixable()])
        self._insert_item_into_raw_list(results_for_builder,
            fixable_count, self.FIXABLE_COUNT)

        # Create a test modifiers (FAILS, FLAKY etc) summary dictionary.
        entry = {}
        for test_name in iterkeys(self._test_results_map):
            result_char = self._get_modifier_char(test_name)
            entry[result_char] = entry.get(result_char, 0) + 1

        # Insert the pass/skip/failure summary dictionary.
        self._insert_item_into_raw_list(results_for_builder, entry,
                                        self.FIXABLE)

        # Insert the number of all the tests that are supposed to pass.
        all_test_count = len(self._test_results)
        self._insert_item_into_raw_list(results_for_builder,
            all_test_count, self.ALL_FIXABLE_COUNT)

    def _insert_item_into_raw_list(self, results_for_builder, item, key):
        """Inserts the item into the list with the given key in the results for
        this builder. Creates the list if no such list exists.

        Args:
          results_for_builder: Dictionary containing the test results for a
              single builder.
          item: Number or string to insert into the list.
          key: Key in results_for_builder for the list to insert into.
        """
        if key in results_for_builder:
            raw_list = results_for_builder[key]
        else:
            raw_list = []

        raw_list.insert(0, item)
        raw_list = raw_list[:self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG]
        results_for_builder[key] = raw_list

    def _insert_item_run_length_encoded(self, item, encoded_results):
        """Inserts the item into the run-length encoded results.

        Args:
          item: String or number to insert.
          encoded_results: run-length encoded results. An array of arrays, e.g.
              [[3,'A'],[1,'Q']] encodes AAAQ.
        """
        if len(encoded_results) and item == encoded_results[0][1]:
            num_results = encoded_results[0][0]
            if num_results <= self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG:
                encoded_results[0][0] = num_results + 1
        else:
            # Use a list instead of a class for the run-length encoding since
            # we want the serialized form to be concise.
            encoded_results.insert(0, [1, item])

    def _insert_test_time_and_result(self, test_name, tests):
        """ Insert a test item with its results to the given tests dictionary.

        Args:
          tests: Dictionary containing test result entries.
        """

        result = self._get_result_char(test_name)
        time = self._get_test_timing(test_name)

        this_test = tests
        for segment in test_name.split("/"):
            if segment not in this_test:
                this_test[segment] = {}
            this_test = this_test[segment]

        if not len(this_test):
            self._populate_results_and_times_json(this_test)

        if self.RESULTS in this_test:
            self._insert_item_run_length_encoded(result, this_test[self.RESULTS])
        else:
            this_test[self.RESULTS] = [[1, result]]

        if self.TIMES in this_test:
            self._insert_item_run_length_encoded(time, this_test[self.TIMES])
        else:
            this_test[self.TIMES] = [[1, time]]

    def _populate_results_and_times_json(self, results_and_times):
        results_and_times[self.RESULTS] = []
        results_and_times[self.TIMES] = []
        return results_and_times

    def _create_results_for_builder_json(self):
        results_for_builder = {}
        results_for_builder[self.TESTS] = {}
        return results_for_builder
