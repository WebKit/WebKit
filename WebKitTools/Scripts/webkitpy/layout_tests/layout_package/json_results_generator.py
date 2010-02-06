#!/usr/bin/env python
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
import os
import simplejson
import subprocess
import sys
import time
import urllib2
import xml.dom.minidom

from layout_package import test_expectations


class JSONResultsGenerator(object):

    MAX_NUMBER_OF_BUILD_RESULTS_TO_LOG = 750
    # Min time (seconds) that will be added to the JSON.
    MIN_TIME = 1
    JSON_PREFIX = "ADD_RESULTS("
    JSON_SUFFIX = ");"
    PASS_RESULT = "P"
    SKIP_RESULT = "X"
    NO_DATA_RESULT = "N"
    VERSION = 3
    VERSION_KEY = "version"
    RESULTS = "results"
    TIMES = "times"
    BUILD_NUMBERS = "buildNumbers"
    WEBKIT_SVN = "webkitRevision"
    CHROME_SVN = "chromeRevision"
    TIME = "secondsSinceEpoch"
    TESTS = "tests"

    FIXABLE_COUNT = "fixableCount"
    FIXABLE = "fixableCounts"
    ALL_FIXABLE_COUNT = "allFixableCount"

    # Note that we omit test_expectations.FAIL from this list because
    # it should never show up (it's a legacy input expectation, never
    # an output expectation).
    FAILURE_TO_CHAR = {test_expectations.CRASH: "C",
                       test_expectations.TIMEOUT: "T",
                       test_expectations.IMAGE: "I",
                       test_expectations.TEXT: "F",
                       test_expectations.MISSING: "O",
                       test_expectations.IMAGE_PLUS_TEXT: "Z"}
    FAILURE_CHARS = FAILURE_TO_CHAR.values()

    RESULTS_FILENAME = "results.json"

    def __init__(self, port, builder_name, build_name, build_number,
        results_file_base_path, builder_base_url,
        test_timings, failures, passed_tests, skipped_tests, all_tests):
        """Modifies the results.json file. Grabs it off the archive directory
        if it is not found locally.

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
        """
        self._port = port
        self._builder_name = builder_name
        self._build_name = build_name
        self._build_number = build_number
        self._builder_base_url = builder_base_url
        self._results_file_path = os.path.join(results_file_base_path,
            self.RESULTS_FILENAME)
        self._test_timings = test_timings
        self._failures = failures
        self._passed_tests = passed_tests
        self._skipped_tests = skipped_tests
        self._all_tests = all_tests

        self._generate_json_output()

    def _generate_json_output(self):
        """Generates the JSON output file."""
        json = self._get_json()
        if json:
            results_file = open(self._results_file_path, "w")
            results_file.write(json)
            results_file.close()

    def _get_svn_revision(self, in_directory):
        """Returns the svn revision for the given directory.

        Args:
          in_directory: The directory where svn is to be run.
        """
        if os.path.exists(os.path.join(in_directory, '.svn')):
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

    def _get_archived_json_results(self):
        """Reads old results JSON file if it exists.
        Returns (archived_results, error) tuple where error is None if results
        were successfully read.
        """
        results_json = {}
        old_results = None
        error = None

        if os.path.exists(self._results_file_path):
            old_results_file = open(self._results_file_path, "r")
            old_results = old_results_file.read()
        elif self._builder_base_url:
            # Check if we have the archived JSON file on the buildbot server.
            results_file_url = (self._builder_base_url +
                self._build_name + "/" + self.RESULTS_FILENAME)
            logging.error("Local results.json file does not exist. Grabbing "
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
                logging.debug("results.json was not valid JSON. Clobbering.")
                # The JSON file is not valid JSON. Just clobber the results.
                results_json = {}
        else:
            logging.debug('Old JSON results do not exist. Starting fresh.')
            results_json = {}

        return results_json, error

    def _get_json(self):
        """Gets the results for the results.json file."""
        results_json, error = self._get_archived_json_results()
        if error:
            # If there was an error don't write a results.json
            # file at all as it would lose all the information on the bot.
            logging.error("Archive directory is inaccessible. Not modifying "
                "or clobbering the results.json file: " + str(error))
            return None

        builder_name = self._builder_name
        if results_json and builder_name not in results_json:
            logging.debug("Builder name (%s) is not in the results.json file."
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
        all_failing_tests = set(self._failures.iterkeys())
        all_failing_tests.update(tests.iterkeys())
        for test in all_failing_tests:
            self._insert_test_time_and_result(test, tests)

        # Specify separators in order to get compact encoding.
        results_str = simplejson.dumps(results_json, separators=(',', ':'))
        return self.JSON_PREFIX + results_str + self.JSON_SUFFIX

    def _insert_failure_summaries(self, results_for_builder):
        """Inserts aggregate pass/failure statistics into the JSON.
        This method reads self._skipped_tests, self._passed_tests and
        self._failures and inserts FIXABLE, FIXABLE_COUNT and ALL_FIXABLE_COUNT
        entries.

        Args:
          results_for_builder: Dictionary containing the test results for a
              single builder.
        """
        # Insert the number of tests that failed.
        self._insert_item_into_raw_list(results_for_builder,
            len(set(self._failures.keys()) | self._skipped_tests),
            self.FIXABLE_COUNT)

        # Create a pass/skip/failure summary dictionary.
        entry = {}
        entry[self.SKIP_RESULT] = len(self._skipped_tests)
        entry[self.PASS_RESULT] = len(self._passed_tests)
        get = entry.get
        for failure_type in self._failures.values():
            failure_char = self.FAILURE_TO_CHAR[failure_type]
            entry[failure_char] = get(failure_char, 0) + 1

        # Insert the pass/skip/failure summary dictionary.
        self._insert_item_into_raw_list(results_for_builder, entry,
                                        self.FIXABLE)

        # Insert the number of all the tests that are supposed to pass.
        self._insert_item_into_raw_list(results_for_builder,
            len(self._skipped_tests | self._all_tests),
            self.ALL_FIXABLE_COUNT)

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

        # These next two branches test to see which source repos we can
        # pull revisions from.
        if hasattr(self._port, 'path_from_webkit_base'):
            path_to_webkit = self._port.path_from_webkit_base()
            self._insert_item_into_raw_list(results_for_builder,
                self._get_svn_revision(path_to_webkit),
                self.WEBKIT_SVN)

        if hasattr(self._port, 'path_from_chromium_base'):
            path_to_chrome = self._port.path_from_chromium_base()
            self._insert_item_into_raw_list(results_for_builder,
                self._get_svn_revision(path_to_chrome),
                self.CHROME_SVN)

        self._insert_item_into_raw_list(results_for_builder,
            int(time.time()),
            self.TIME)

    def _insert_test_time_and_result(self, test_name, tests):
        """ Insert a test item with its results to the given tests dictionary.

        Args:
          tests: Dictionary containing test result entries.
        """

        result = JSONResultsGenerator.PASS_RESULT
        time = 0

        if test_name not in self._all_tests:
            result = JSONResultsGenerator.NO_DATA_RESULT

        if test_name in self._failures:
            result = self.FAILURE_TO_CHAR[self._failures[test_name]]

        if test_name in self._test_timings:
            # Floor for now to get time in seconds.
            time = int(self._test_timings[test_name])

        if test_name not in tests:
            tests[test_name] = self._create_results_and_times_json()

        thisTest = tests[test_name]
        self._insert_item_run_length_encoded(result, thisTest[self.RESULTS])
        self._insert_item_run_length_encoded(time, thisTest[self.TIMES])
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
