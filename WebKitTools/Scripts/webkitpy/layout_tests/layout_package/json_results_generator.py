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

from __future__ import with_statement

import codecs
import logging
import os
import subprocess
import sys
import time
import urllib2
import xml.dom.minidom

from webkitpy.layout_tests.layout_package import test_results_uploader

import webkitpy.thirdparty.simplejson as simplejson

# A JSON results generator for generic tests.
# FIXME: move this code out of the layout_package directory.

_log = logging.getLogger("webkitpy.layout_tests.layout_package.json_results_generator")


class TestResult(object):
    """A simple class that represents a single test result."""
    def __init__(self, name, failed=False, skipped=False, elapsed_time=0):
        self.name = name
        self.failed = failed
        self.skipped = skipped
        self.time = elapsed_time

    def fixable(self):
        return self.failed or self.skipped


class JSONResultsGeneratorBase(object):
    """A JSON results generator for generic tests."""

    MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG = 1500
    # Min time (seconds) that will be added to the JSON.
    MIN_TIME = 1
    JSON_PREFIX = "ADD_RESULTS("
    JSON_SUFFIX = ");"
    PASS_RESULT = "P"
    SKIP_RESULT = "X"
    FAIL_RESULT = "F"
    NO_DATA_RESULT = "N"
    VERSION = 3
    VERSION_KEY = "version"
    RESULTS = "results"
    TIMES = "times"
    BUILD_NUMBERS = "buildNumbers"
    TIME = "secondsSinceEpoch"
    TESTS = "tests"

    FIXABLE_COUNT = "fixableCount"
    FIXABLE = "fixableCounts"
    ALL_FIXABLE_COUNT = "allFixableCount"

    RESULTS_FILENAME = "results.json"
    INCREMENTAL_RESULTS_FILENAME = "incremental_results.json"

    URL_FOR_TEST_LIST_JSON = \
        "http://%s/testfile?builder=%s&name=%s&testlistjson=1&testtype=%s"

    def __init__(self, builder_name, build_name, build_number,
        results_file_base_path, builder_base_url,
        test_results_map, svn_repositories=None,
        generate_incremental_results=False,
        test_results_server=None,
        test_type=""):
        """Modifies the results.json file. Grabs it off the archive directory
        if it is not found locally.

        Args
          builder_name: the builder name (e.g. Webkit).
          build_name: the build name (e.g. webkit-rel).
          build_number: the build number.
          results_file_base_path: Absolute path to the directory containing the
              results json file.
          builder_base_url: the URL where we have the archived test results.
              If this is None no archived results will be retrieved.
          test_results_map: A dictionary that maps test_name to TestResult.
          svn_repositories: A (json_field_name, svn_path) pair for SVN
              repositories that tests rely on.  The SVN revision will be
              included in the JSON with the given json_field_name.
          generate_incremental_results: If true, generate incremental json file
              from current run results.
          test_results_server: server that hosts test results json.
        """
        self._builder_name = builder_name
        self._build_name = build_name
        self._build_number = build_number
        self._builder_base_url = builder_base_url
        self._results_file_path = os.path.join(results_file_base_path,
            self.RESULTS_FILENAME)
        self._incremental_results_file_path = os.path.join(
            results_file_base_path, self.INCREMENTAL_RESULTS_FILENAME)

        self._test_results_map = test_results_map
        self._test_results = test_results_map.values()
        self._generate_incremental_results = generate_incremental_results

        self._svn_repositories = svn_repositories
        if not self._svn_repositories:
            self._svn_repositories = {}

        self._test_results_server = test_results_server
        self._test_type = test_type

        self._json = None
        self._archived_results = None

    def generate_json_output(self):
        """Generates the JSON output file."""

        # Generate the JSON output file that has full results.
        # FIXME: stop writing out the full results file once all bots use
        # incremental results.
        if not self._json:
            self._json = self.get_json()
        if self._json:
            self._generate_json_file(self._json, self._results_file_path)

        # Generate the JSON output file that only has incremental results.
        if self._generate_incremental_results:
            json = self.get_json(incremental=True)
            if json:
                self._generate_json_file(
                    json, self._incremental_results_file_path)

    def get_json(self, incremental=False):
        """Gets the results for the results.json file."""
        results_json = {}
        if not incremental:
            if self._json:
                return self._json

            if self._archived_results:
                results_json = self._archived_results

        if not results_json:
            results_json, error = self._get_archived_json_results(incremental)
            if error:
                # If there was an error don't write a results.json
                # file at all as it would lose all the information on the
                # bot.
                _log.error("Archive directory is inaccessible. Not "
                           "modifying or clobbering the results.json "
                           "file: " + str(error))
                return None

        builder_name = self._builder_name
        if results_json and builder_name not in results_json:
            _log.debug("Builder name (%s) is not in the results.json file."
                       % builder_name)

        self._convert_json_to_current_version(results_json)

        if builder_name not in results_json:
            results_json[builder_name] = (
                self._create_results_for_builder_json())

        results_for_builder = results_json[builder_name]

        self._insert_generic_metadata(results_for_builder)

        self._insert_failure_summaries(results_for_builder)

        # Update the all failing tests with result type and time.
        tests = results_for_builder[self.TESTS]
        all_failing_tests = self._get_failed_test_names()
        all_failing_tests.update(tests.iterkeys())
        for test in all_failing_tests:
            self._insert_test_time_and_result(test, tests, incremental)

        return results_json

    def set_archived_results(self, archived_results):
        self._archived_results = archived_results

    def _generate_json_file(self, json, file_path):
        # Specify separators in order to get compact encoding.
        json_data = simplejson.dumps(json, separators=(',', ':'))
        json_string = self.JSON_PREFIX + json_data + self.JSON_SUFFIX

        results_file = codecs.open(file_path, "w", "utf-8")
        results_file.write(json_string)
        results_file.close()

    def _get_test_timing(self, test_name):
        """Returns test timing data (elapsed time) in second
        for the given test_name."""
        if test_name in self._test_results_map:
            # Floor for now to get time in seconds.
            return int(self._test_results_map[test_name].time)
        return 0

    def _get_failed_test_names(self):
        """Returns a set of failed test names."""
        return set([r.name for r in self._test_results if r.failed])

    def _get_result_type_char(self, test_name):
        """Returns a single char (e.g. SKIP_RESULT, FAIL_RESULT,
        PASS_RESULT, NO_DATA_RESULT, etc) that indicates the test result
        for the given test_name.
        """
        if test_name not in self._test_results_map:
            return JSONResultsGenerator.NO_DATA_RESULT

        test_result = self._test_results_map[test_name]
        if test_result.skipped:
            return JSONResultsGenerator.SKIP_RESULT
        if test_result.failed:
            return JSONResultsGenerator.FAIL_RESULT

        return JSONResultsGenerator.PASS_RESULT

    # FIXME: Callers should use scm.py instead.
    # FIXME: Identify and fix the run-time errors that were observed on Windows
    # chromium buildbot when we had updated this code to use scm.py once before.
    def _get_svn_revision(self, in_directory):
        """Returns the svn revision for the given directory.

        Args:
          in_directory: The directory where svn is to be run.
        """
        if os.path.exists(os.path.join(in_directory, '.svn')):
            # Note: Not thread safe: http://bugs.python.org/issue2320
            output = subprocess.Popen(["svn", "info", "--xml"],
                                      cwd=in_directory,
                                      shell=(sys.platform == 'win32'),
                                      stdout=subprocess.PIPE).communicate()[0]
            try:
                dom = xml.dom.minidom.parseString(output)
                return dom.getElementsByTagName('entry')[0].getAttribute(
                    'revision')
            except xml.parsers.expat.ExpatError:
                return ""
        return ""

    def _get_archived_json_results(self, for_incremental=False):
        """Reads old results JSON file if it exists.
        Returns (archived_results, error) tuple where error is None if results
        were successfully read.

        if for_incremental is True, download JSON file that only contains test
        name list from test-results server. This is for generating incremental
        JSON so the file generated has info for tests that failed before but
        pass or are skipped from current run.
        """
        results_json = {}
        old_results = None
        error = None

        if os.path.exists(self._results_file_path) and not for_incremental:
            with codecs.open(self._results_file_path, "r", "utf-8") as file:
                old_results = file.read()
        elif self._builder_base_url or for_incremental:
            if for_incremental:
                if not self._test_results_server:
                    # starting from fresh if no test results server specified.
                    return {}, None

                results_file_url = (self.URL_FOR_TEST_LIST_JSON %
                    (urllib2.quote(self._test_results_server),
                     urllib2.quote(self._builder_name),
                     self.RESULTS_FILENAME,
                     urllib2.quote(self._test_type)))
            else:
                # Check if we have the archived JSON file on the buildbot
                # server.
                results_file_url = (self._builder_base_url +
                    self._build_name + "/" + self.RESULTS_FILENAME)
                _log.error("Local results.json file does not exist. Grabbing "
                           "it off the archive at " + results_file_url)

            try:
                results_file = urllib2.urlopen(results_file_url)
                info = results_file.info()
                old_results = results_file.read()
            except urllib2.HTTPError, http_error:
                # A non-4xx status code means the bot is hosed for some reason
                # and we can't grab the results.json file off of it.
                if (http_error.code < 400 and http_error.code >= 500):
                    error = http_error
            except urllib2.URLError, url_error:
                error = url_error

        if old_results:
            # Strip the prefix and suffix so we can get the actual JSON object.
            old_results = old_results[len(self.JSON_PREFIX):
                                      len(old_results) - len(self.JSON_SUFFIX)]

            try:
                results_json = simplejson.loads(old_results)
            except:
                _log.debug("results.json was not valid JSON. Clobbering.")
                # The JSON file is not valid JSON. Just clobber the results.
                results_json = {}
        else:
            _log.debug('Old JSON results do not exist. Starting fresh.')
            results_json = {}

        return results_json, error

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

        # Create a pass/skip/failure summary dictionary.
        entry = {}
        for test_name in self._test_results_map.iterkeys():
            result_char = self._get_result_type_char(test_name)
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

    def _insert_generic_metadata(self, results_for_builder):
        """ Inserts generic metadata (such as version number, current time etc)
        into the JSON.

        Args:
          results_for_builder: Dictionary containing the test results for
              a single builder.
        """
        self._insert_item_into_raw_list(results_for_builder,
            self._build_number, self.BUILD_NUMBERS)

        # Include SVN revisions for the given repositories.
        for (name, path) in self._svn_repositories:
            self._insert_item_into_raw_list(results_for_builder,
                self._get_svn_revision(path),
                name + 'Revision')

        self._insert_item_into_raw_list(results_for_builder,
            int(time.time()),
            self.TIME)

    def _insert_test_time_and_result(self, test_name, tests, incremental=False):
        """ Insert a test item with its results to the given tests dictionary.

        Args:
          tests: Dictionary containing test result entries.
        """

        result = self._get_result_type_char(test_name)
        time = self._get_test_timing(test_name)

        if test_name not in tests:
            tests[test_name] = self._create_results_and_times_json()

        thisTest = tests[test_name]
        if self.RESULTS in thisTest:
            self._insert_item_run_length_encoded(result, thisTest[self.RESULTS])
        else:
            thisTest[self.RESULTS] = [[1, result]]

        if self.TIMES in thisTest:
            self._insert_item_run_length_encoded(time, thisTest[self.TIMES])
        else:
            thisTest[self.TIMES] = [[1, time]]

        # Don't normalize the incremental results json because we need results
        # for tests that pass or have no data from current run.
        if not incremental:
            self._normalize_results_json(thisTest, test_name, tests)

    def _convert_json_to_current_version(self, results_json):
        """If the JSON does not match the current version, converts it to the
        current version and adds in the new version number.
        """
        if (self.VERSION_KEY in results_json and
            results_json[self.VERSION_KEY] == self.VERSION):
            return

        results_json[self.VERSION_KEY] = self.VERSION

    def _create_results_and_times_json(self):
        results_and_times = {}
        results_and_times[self.RESULTS] = []
        results_and_times[self.TIMES] = []
        return results_and_times

    def _create_results_for_builder_json(self):
        results_for_builder = {}
        results_for_builder[self.TESTS] = {}
        return results_for_builder

    def _remove_items_over_max_number_of_builds(self, encoded_list):
        """Removes items from the run-length encoded list after the final
        item that exceeds the max number of builds to track.

        Args:
          encoded_results: run-length encoded results. An array of arrays, e.g.
              [[3,'A'],[1,'Q']] encodes AAAQ.
        """
        num_builds = 0
        index = 0
        for result in encoded_list:
            num_builds = num_builds + result[0]
            index = index + 1
            if num_builds > self.MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG:
                return encoded_list[:index]
        return encoded_list

    def _normalize_results_json(self, test, test_name, tests):
        """ Prune tests where all runs pass or tests that no longer exist and
        truncate all results to maxNumberOfBuilds.

        Args:
          test: ResultsAndTimes object for this test.
          test_name: Name of the test.
          tests: The JSON object with all the test results for this builder.
        """
        test[self.RESULTS] = self._remove_items_over_max_number_of_builds(
            test[self.RESULTS])
        test[self.TIMES] = self._remove_items_over_max_number_of_builds(
            test[self.TIMES])

        is_all_pass = self._is_results_all_of_type(test[self.RESULTS],
                                                   self.PASS_RESULT)
        is_all_no_data = self._is_results_all_of_type(test[self.RESULTS],
            self.NO_DATA_RESULT)
        max_time = max([time[1] for time in test[self.TIMES]])

        # Remove all passes/no-data from the results to reduce noise and
        # filesize. If a test passes every run, but takes > MIN_TIME to run,
        # don't throw away the data.
        if is_all_no_data or (is_all_pass and max_time <= self.MIN_TIME):
            del tests[test_name]

    def _is_results_all_of_type(self, results, type):
        """Returns whether all the results are of the given type
        (e.g. all passes)."""
        return len(results) == 1 and results[0][1] == type


# A wrapper class for JSONResultsGeneratorBase.
# Note: There's a script outside the WebKit codebase calling this script.
# FIXME: Please keep the interface until the other script is cleaned up.
# (http://src.chromium.org/viewvc/chrome/trunk/src/webkit/tools/layout_tests/webkitpy/layout_tests/test_output_xml_to_json.py?view=markup)
class JSONResultsGenerator(JSONResultsGeneratorBase):
    # The flag is for backward compatibility.
    output_json_in_init = True

    def _upload_json_files(self):
        if not self._test_results_server or not self._test_type:
            return

        _log.info("Uploading JSON files for %s to the server: %s",
                  self._builder_name, self._test_results_server)
        attrs = [("builder", self._builder_name), ("testtype", self._test_type)]
        json_files = [self.INCREMENTAL_RESULTS_FILENAME]

        files = [(file, os.path.join(self._results_directory, file))
            for file in json_files]
        uploader = test_results_uploader.TestResultsUploader(
            self._test_results_server)
        try:
            # Set uploading timeout in case appengine server is having problem.
            # 120 seconds are more than enough to upload test results.
            uploader.upload(attrs, files, 120)
        except Exception, err:
            _log.error("Upload failed: %s" % err)
            return

        _log.info("JSON files uploaded.")

    def __init__(self, port, builder_name, build_name, build_number,
        results_file_base_path, builder_base_url,
        test_timings, failures, passed_tests, skipped_tests, all_tests,
        test_results_server=None, test_type=None):
        """Generates a JSON results file.

        Args
          builder_name: the builder name (e.g. Webkit).
          build_name: the build name (e.g. webkit-rel).
          build_number: the build number.
          results_file_base_path: Absolute path to the directory containing the
              results json file.
          builder_base_url: the URL where we have the archived test results.
          test_timings: Map of test name to a test_run-time.
          failures: Map of test name to a failure type (of test_expectations).
          passed_tests: A set containing all the passed tests.
          skipped_tests: A set containing all the skipped tests.
          all_tests: List of all the tests that were run.  This should not
              include skipped tests.
          test_results_server: server that hosts test results json.
          test_type: the test type.
        """

        self._test_type = test_type
        self._results_directory = results_file_base_path

        # Create a map of (name, TestResult).
        test_results_map = dict()
        get = test_results_map.get
        for (test, time) in test_timings.iteritems():
            test_results_map[test] = TestResult(test, elapsed_time=time)
        for test in failures.iterkeys():
            test_results_map[test] = test_result = get(test, TestResult(test))
            test_result.failed = True
        for test in skipped_tests:
            test_results_map[test] = test_result = get(test, TestResult(test))
            test_result.skipped = True
        for test in passed_tests:
            test_results_map[test] = test_result = get(test, TestResult(test))
            test_result.failed = False
            test_result.skipped = False
        for test in all_tests:
            if test not in test_results_map:
                test_results_map[test] = TestResult(test)

        # Generate the JSON with incremental flag enabled.
        # (This should also output the full result for now.)
        super(JSONResultsGenerator, self).__init__(
            builder_name, build_name, build_number,
            results_file_base_path, builder_base_url, test_results_map,
            svn_repositories=port.test_repository_paths(),
            generate_incremental_results=True,
            test_results_server=test_results_server,
            test_type=test_type)

        if self.__class__.output_json_in_init:
            self.generate_json_output()
            self._upload_json_files()
