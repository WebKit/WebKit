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

try:
    import jsonresults
    from jsonresults import JsonResults
except ImportError:
    print "ERROR: Add the TestResultServer, google_appengine and yaml/lib directories to your PYTHONPATH"
    raise

import unittest


JSON_RESULTS_TEMPLATE = (
    '{"Webkit":{'
    '"allFixableCount":[[TESTDATA_COUNT]],'
    '"buildNumbers":[[TESTDATA_BUILDNUMBERS]],'
    '"chromeRevision":[[TESTDATA_CHROMEREVISION]],'
    '"deferredCounts":[[TESTDATA_COUNTS]],'
    '"fixableCount":[[TESTDATA_COUNT]],'
    '"fixableCounts":[[TESTDATA_COUNTS]],'
    '"secondsSinceEpoch":[[TESTDATA_TIMES]],'
    '"tests":{[TESTDATA_TESTS]},'
    '"webkitRevision":[[TESTDATA_WEBKITREVISION]],'
    '"wontfixCounts":[[TESTDATA_COUNTS]]'
    '},'
    '"version":[VERSION]'
    '}')

JSON_RESULTS_COUNTS_TEMPLATE = (
    '{'
    '"C":[TESTDATA],'
    '"F":[TESTDATA],'
    '"I":[TESTDATA],'
    '"O":[TESTDATA],'
    '"P":[TESTDATA],'
    '"T":[TESTDATA],'
    '"X":[TESTDATA],'
    '"Z":[TESTDATA]}')

JSON_RESULTS_DIRECTORY_TEMPLATE = '"[TESTDATA_DIRECTORY]":{[TESTDATA_DATA]}'

JSON_RESULTS_TESTS_TEMPLATE = (
    '"[TESTDATA_TEST_NAME]":{'
    '"results":[[TESTDATA_TEST_RESULTS]],'
    '"times":[[TESTDATA_TEST_TIMES]]}')

JSON_RESULTS_PREFIX = "ADD_RESULTS("
JSON_RESULTS_SUFFIX = ");"

JSON_RESULTS_TEST_LIST_TEMPLATE = (
    '{"Webkit":{"tests":{[TESTDATA_TESTS]}}}')


class JsonResultsTest(unittest.TestCase):
    def setUp(self):
        self._builder = "Webkit"

    def _make_test_json(self, test_data):
        if not test_data:
            return JSON_RESULTS_PREFIX + JSON_RESULTS_SUFFIX

        builds = test_data["builds"]
        tests = test_data["tests"]
        if not builds or not tests:
            return JSON_RESULTS_PREFIX + JSON_RESULTS_SUFFIX

        json = JSON_RESULTS_TEMPLATE

        counts = []
        build_numbers = []
        webkit_revision = []
        chrome_revision = []
        times = []
        for build in builds:
            counts.append(JSON_RESULTS_COUNTS_TEMPLATE.replace("[TESTDATA]", build))
            build_numbers.append("1000%s" % build)
            webkit_revision.append("2000%s" % build)
            chrome_revision.append("3000%s" % build)
            times.append("100000%s000" % build)

        json = json.replace("[TESTDATA_COUNTS]", ",".join(counts))
        json = json.replace("[TESTDATA_COUNT]", ",".join(builds))
        json = json.replace("[TESTDATA_BUILDNUMBERS]", ",".join(build_numbers))
        json = json.replace("[TESTDATA_WEBKITREVISION]", ",".join(webkit_revision))
        json = json.replace("[TESTDATA_CHROMEREVISION]", ",".join(chrome_revision))
        json = json.replace("[TESTDATA_TIMES]", ",".join(times))

        if "version" in test_data:
            json = json.replace("[VERSION]", str(test_data["version"]))
        else:
            json = json.replace("[VERSION]", "3")

        json_tests = []
        for (name, test) in sorted(tests.iteritems()):
            json_tests.append(self._parse_tests_dict(name, test))

        json = json.replace("[TESTDATA_TESTS]", ",".join(json_tests))

        return JSON_RESULTS_PREFIX + json + JSON_RESULTS_SUFFIX

    def _parse_tests_dict(self, name, test):
        if "results" in test:
            test_results = JSON_RESULTS_TESTS_TEMPLATE.replace("[TESTDATA_TEST_NAME]", name)
            test_results = test_results.replace("[TESTDATA_TEST_RESULTS]", test["results"])
            test_results = test_results.replace("[TESTDATA_TEST_TIMES]", test["times"])
            return test_results

        test_results = JSON_RESULTS_DIRECTORY_TEMPLATE.replace("[TESTDATA_DIRECTORY]", name)
        testdata = []
        for (child_name, child_test) in sorted(test.iteritems()):
            testdata.append(self._parse_tests_dict(child_name, child_test))
        test_results = test_results.replace("[TESTDATA_DATA]", ",".join(testdata))
        return test_results

    def _test_merge(self, aggregated_data, incremental_data, expected_data, max_builds=jsonresults.JSON_RESULTS_MAX_BUILDS):
        aggregated_results = self._make_test_json(aggregated_data)
        incremental_results = self._make_test_json(incremental_data)
        merged_results = JsonResults.merge(self._builder, aggregated_results, incremental_results, max_builds, sort_keys=True)

        if expected_data:
            expected_results = self._make_test_json(expected_data)
            self.assertEquals(merged_results, expected_results)
        else:
            self.assertFalse(merged_results)

    def _test_get_test_list(self, input_data, expected_data):
        input_results = self._make_test_json(input_data)

        json_tests = []
        for test in expected_data:
            json_tests.append("\"" + test + "\":{}")

        expected_results = (JSON_RESULTS_PREFIX +
            JSON_RESULTS_TEST_LIST_TEMPLATE.replace("[TESTDATA_TESTS]", ",".join(json_tests)) +
            JSON_RESULTS_SUFFIX)

        actual_results = JsonResults.get_test_list(self._builder, input_results)
        self.assertEquals(actual_results, expected_results)

    def test_merge_null_incremental_results(self):
        # Empty incremental results json.
        # Nothing to merge.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Incremental results
            None,
            # Expect no merge happens.
            None)

    def test_merge_empty_incremental_results(self):
        # No actual incremental test results (only prefix and suffix) to merge.
        # Nothing to merge.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Incremental results
            {"builds": [],
             "tests": {}},
            # Expected no merge happens.
            None)

    def test_merge_empty_aggregated_results(self):
        # No existing aggregated results.
        # Merged results == new incremental results.
        self._test_merge(
            # Aggregated results
            None,
            # Incremental results

            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Expected result
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}})

    def test_merge_incremental_single_test_single_run_same_result(self):
        # Incremental results has the latest build and same test results for
        # that run.
        # Insert the incremental results at the first place and sum number
        # of runs for "F" (200 + 1) to get merged results.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"F\"]",
                           "times": "[1,0]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[201,\"F\"]",
                           "times": "[201,0]"}}})

    def test_merge_single_test_single_run_different_result(self):
        # Incremental results has the latest build but different test results
        # for that run.
        # Insert the incremental results at the first place.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1, \"I\"]",
                           "times": "[1,1]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[1,\"I\"],[200,\"F\"]",
                           "times": "[1,1],[200,0]"}}})

    def test_merge_single_test_single_run_result_changed(self):
        # Incremental results has the latest build but results which differ from
        # the latest result (but are the same as an older result).
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"],[10,\"I\"]",
                           "times": "[200,0],[10,1]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"I\"]",
                           "times": "[1,1]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[1,\"I\"],[200,\"F\"],[10,\"I\"]",
                           "times": "[1,1],[200,0],[10,1]"}}})

    def test_merge_multiple_tests_single_run(self):
        # All tests have incremental updates.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"},
                       "002.html": {
                           "results": "[100,\"I\"]",
                           "times": "[100,1]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"F\"]",
                           "times": "[1,0]"},
                       "002.html": {
                           "results": "[1,\"I\"]",
                           "times": "[1,1]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[201,\"F\"]",
                           "times": "[201,0]"},
                       "002.html": {
                           "results": "[101,\"I\"]",
                           "times": "[101,1]"}}})

    def test_merge_multiple_tests_single_run_one_no_result(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"},
                       "002.html": {
                           "results": "[100,\"I\"]",
                           "times": "[100,1]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"002.html": {
                           "results": "[1,\"I\"]",
                           "times": "[1,1]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[1,\"N\"],[200,\"F\"]",
                           "times": "[201,0]"},
                       "002.html": {
                           "results": "[101,\"I\"]",
                           "times": "[101,1]"}}})

    def test_merge_single_test_multiple_runs(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Incremental results
            {"builds": ["4", "3"],
             "tests": {"001.html": {
                           "results": "[2, \"I\"]",
                           "times": "[2,2]"}}},
            # Expected results
            {"builds": ["4", "3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[2,\"I\"],[200,\"F\"]",
                           "times": "[2,2],[200,0]"}}})

    def test_merge_multiple_tests_multiple_runs(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"},
                       "002.html": {
                           "results": "[10,\"Z\"]",
                           "times": "[10,0]"}}},
            # Incremental results
            {"builds": ["4", "3"],
             "tests": {"001.html": {
                           "results": "[2, \"I\"]",
                           "times": "[2,2]"},
                       "002.html": {
                           "results": "[1,\"C\"]",
                           "times": "[1,1]"}}},
            # Expected results
            {"builds": ["4", "3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[2,\"I\"],[200,\"F\"]",
                           "times": "[2,2],[200,0]"},
                       "002.html": {
                           "results": "[1,\"C\"],[10,\"Z\"]",
                           "times": "[1,1],[10,0]"}}})

    def test_merge_incremental_result_older_build(self):
        # Test the build in incremental results is older than the most recent
        # build in aggregated results.
        # The incremental results should be dropped and no merge happens.
        self._test_merge(
            # Aggregated results
            {"builds": ["3", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Incremental results
            {"builds": ["2"],
             "tests": {"001.html": {
                           "results": "[1, \"F\"]",
                           "times": "[1,0]"}}},
            # Expected no merge happens.
            None)

    def test_merge_incremental_result_same_build(self):
        # Test the build in incremental results is same as the build in
        # aggregated results.
        # The incremental results should be dropped and no merge happens.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"F\"]",
                           "times": "[200,0]"}}},
            # Incremental results
            {"builds": ["3", "2"],
             "tests": {"001.html": {
                           "results": "[2, \"F\"]",
                           "times": "[2,0]"}}},
            # Expected no merge happens.
            None)

    def test_merge_remove_test_with_no_data(self):
        # Remove test where there is no data in all runs.
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"N\"]",
                           "times": "[200,0]"},
                       "002.html": {
                           "results": "[10,\"F\"]",
                           "times": "[10,0]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"N\"]",
                           "times": "[1,0]"},
                       "002.html": {
                           "results": "[1,\"P\"]",
                           "times": "[1,0]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"002.html": {
                           "results": "[1,\"P\"],[10,\"F\"]",
                           "times": "[11,0]"}}})

    def test_merge_remove_test_with_all_pass(self):
        # Remove test where all run pass and max running time < 1 seconds
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"P\"]",
                           "times": "[200,0]"},
                       "002.html": {
                           "results": "[10,\"F\"]",
                           "times": "[10,0]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"P\"]",
                           "times": "[1,0]"},
                       "002.html": {
                           "results": "[1,\"P\"]",
                           "times": "[1,0]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"002.html": {
                           "results": "[1,\"P\"],[10,\"F\"]",
                           "times": "[11,0]"}}})

    def test_merge_keep_test_with_all_pass_but_slow_time(self):
        # Do not remove test where all run pass but max running time >= 1 seconds
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"P\"]",
                           "times": "[200,0]"},
                       "002.html": {
                           "results": "[10,\"F\"]",
                           "times": "[10,0]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"P\"]",
                           "times": "[1,1]"},
                       "002.html": {
                           "results": "[1,\"P\"]",
                           "times": "[1,0]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[201,\"P\"]",
                           "times": "[1,1],[200,0]"},
                       "002.html": {
                           "results": "[1,\"P\"],[10,\"F\"]",
                           "times": "[11,0]"}}})

    def test_merge_prune_extra_results(self):
        # Remove items from test results and times that exceed the max number
        # of builds to track.
        max_builds = str(jsonresults.JSON_RESULTS_MAX_BUILDS)
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[" + max_builds + ",\"F\"],[1,\"I\"]",
                           "times": "[" + max_builds + ",0],[1,1]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"T\"]",
                           "times": "[1,1]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[1,\"T\"],[" + max_builds + ",\"F\"]",
                           "times": "[1,1],[" + max_builds + ",0]"}}})

    def test_merge_prune_extra_results_small(self):
        # Remove items from test results and times that exceed the max number
        # of builds to track, using smaller threshold.
        max_builds = str(jsonresults.JSON_RESULTS_MAX_BUILDS_SMALL)
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[" + max_builds + ",\"F\"],[1,\"I\"]",
                           "times": "[" + max_builds + ",0],[1,1]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"T\"]",
                           "times": "[1,1]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[1,\"T\"],[" + max_builds + ",\"F\"]",
                           "times": "[1,1],[" + max_builds + ",0]"}}},
            int(max_builds))

    def test_merge_prune_extra_results_with_new_result_of_same_type(self):
        # Test that merging in a new result of the same type as the last result
        # causes old results to fall off.
        max_builds = str(jsonresults.JSON_RESULTS_MAX_BUILDS_SMALL)
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"001.html": {
                           "results": "[" + max_builds + ",\"F\"],[1,\"N\"]",
                           "times": "[" + max_builds + ",0],[1,1]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"001.html": {
                           "results": "[1,\"F\"]",
                           "times": "[1,0]"}}},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[" + max_builds + ",\"F\"]",
                           "times": "[" + max_builds + ",0]"}}},
            int(max_builds))

    def test_merge_build_directory_hierarchy(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"foo/001.html": {
                           "results": "[50,\"F\"]",
                           "times": "[50,0]"},
                       "foo/002.html": {
                           "results": "[100,\"I\"]",
                           "times": "[100,0]"}}},
            # Incremental results
            {"builds": ["3"],
             "tests": {"foo": {
                           "001.html": {
                               "results": "[1,\"F\"]",
                               "times": "[1,0]"},
                           "002.html": {
                               "results": "[1,\"I\"]",
                               "times": "[1,0]"}}},
             "version": 4},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"foo/001.html": {
                           "results": "[51,\"F\"]",
                           "times": "[51,0]"},
                       "foo/002.html": {
                           "results": "[101,\"I\"]",
                           "times": "[101,0]"}},
             "version": 3})

    # FIXME(aboxhall): Add some tests for xhtml/svg test results.

    def test_get_test_name_list(self):
        # Get test name list only. Don't include non-test-list data and
        # of test result details.
        self._test_get_test_list(
            # Input results
            {"builds": ["3", "2", "1"],
             "tests": {"001.html": {
                           "results": "[200,\"P\"]",
                           "times": "[200,0]"},
                       "002.html": {
                           "results": "[10,\"F\"]",
                           "times": "[10,0]"}}},
            # Expected results
            ["001.html", "002.html"])

    def test_remove_gtest_modifiers(self):
        self._test_merge(
            # Aggregated results
            {"builds": ["2", "1"],
             "tests": {"foo.bar": {
                           "results": "[50,\"F\"]",
                           "times": "[50,0]"},
                       "foo.bar2": {
                           "results": "[100,\"I\"]",
                           "times": "[100,0]"},
                       "foo.FAILS_bar3": {
                           "results": "[100,\"I\"]",
                           "times": "[100,0]"},
                       }},
            # Incremental results
            {"builds": ["3"],
             "tests": {"foo.FLAKY_bar": {
                           "results": "[1,\"F\"]",
                           "times": "[1,0]"},
                       "foo.DISABLED_bar2": {
                           "results": "[1,\"I\"]",
                           "times": "[1,0]"},
                       "foo.FAILS_bar3": {
                           "results": "[1,\"I\"]",
                           "times": "[1,0]"},
                       "foo.MAYBE_bar4": {
                           "results": "[1,\"I\"]",
                           "times": "[1,0]"}},
             "version": 4},
            # Expected results
            {"builds": ["3", "2", "1"],
             "tests": {"foo.bar": {
                           "results": "[51,\"F\"]",
                           "times": "[51,0]"},
                       "foo.bar2": {
                           "results": "[101,\"I\"]",
                           "times": "[101,0]"},
                       "foo.bar3": {
                           "results": "[1,\"I\"]",
                           "times": "[1,0]"},
                       "foo.FAILS_bar3": {
                              "results": "[1,\"N\"],[100,\"I\"]",
                              "times": "[101,0]"},
                       "foo.bar4": {
                           "results": "[1,\"I\"]",
                           "times": "[1,0]"}},
             "version": 3})

if __name__ == '__main__':
    unittest.main()
