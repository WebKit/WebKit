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

from datetime import datetime
from django.utils import simplejson
import logging

from model.testfile import TestFile

JSON_RESULTS_FILE = "results.json"
JSON_RESULTS_FILE_SMALL = "results-small.json"
JSON_RESULTS_PREFIX = "ADD_RESULTS("
JSON_RESULTS_SUFFIX = ");"
JSON_RESULTS_VERSION_KEY = "version"
JSON_RESULTS_BUILD_NUMBERS = "buildNumbers"
JSON_RESULTS_TESTS = "tests"
JSON_RESULTS_RESULTS = "results"
JSON_RESULTS_TIMES = "times"
JSON_RESULTS_PASS = "P"
JSON_RESULTS_NO_DATA = "N"
JSON_RESULTS_MIN_TIME = 1
JSON_RESULTS_VERSION = 3
JSON_RESULTS_MAX_BUILDS = 750
JSON_RESULTS_MAX_BUILDS_SMALL = 200


class JsonResults(object):
    @classmethod
    def _strip_prefix_suffix(cls, data):
        """Strip out prefix and suffix of json results string.

        Args:
            data: json file content.

        Returns:
            json string without prefix and suffix.
        """

        assert(data.startswith(JSON_RESULTS_PREFIX))
        assert(data.endswith(JSON_RESULTS_SUFFIX))

        return data[len(JSON_RESULTS_PREFIX):
                    len(data) - len(JSON_RESULTS_SUFFIX)]

    @classmethod
    def _generate_file_data(cls, json, sort_keys=False):
        """Given json string, generate file content data by adding
           prefix and suffix.

        Args:
            json: json string without prefix and suffix.

        Returns:
            json file data.
        """

        data = simplejson.dumps(json, separators=(',', ':'),
            sort_keys=sort_keys)
        return JSON_RESULTS_PREFIX + data + JSON_RESULTS_SUFFIX

    @classmethod
    def _load_json(cls, file_data):
        """Load json file to a python object.

        Args:
            file_data: json file content.

        Returns:
            json object or
            None on failure.
        """

        json_results_str = cls._strip_prefix_suffix(file_data)
        if not json_results_str:
            logging.warning("No json results data.")
            return None

        try:
            return simplejson.loads(json_results_str)
        except Exception, err:
            logging.debug(json_results_str)
            logging.error("Failed to load json results: %s", str(err))
            return None

    @classmethod
    def _merge_json(cls, aggregated_json, incremental_json, num_runs):
        """Merge incremental json into aggregated json results.

        Args:
            aggregated_json: aggregated json object.
            incremental_json: incremental json object.

        Returns:
            True if merge succeeds or
            False on failure.
        """

        # Merge non tests property data.
        # Tests properties are merged in _merge_tests.
        if not cls._merge_non_test_data(aggregated_json, incremental_json, num_runs):
            return False

        # Merge tests results and times
        incremental_tests = incremental_json[JSON_RESULTS_TESTS]
        if incremental_tests:
            aggregated_tests = aggregated_json[JSON_RESULTS_TESTS]
            cls._merge_tests(aggregated_tests, incremental_tests, num_runs)

        return True

    @classmethod
    def _merge_non_test_data(cls, aggregated_json, incremental_json, num_runs):
        """Merge incremental non tests property data into aggregated json results.

        Args:
            aggregated_json: aggregated json object.
            incremental_json: incremental json object.

        Returns:
            True if merge succeeds or
            False on failure.
        """

        incremental_builds = incremental_json[JSON_RESULTS_BUILD_NUMBERS]
        aggregated_builds = aggregated_json[JSON_RESULTS_BUILD_NUMBERS]
        aggregated_build_number = int(aggregated_builds[0])
        # Loop through all incremental builds, start from the oldest run.
        for index in reversed(range(len(incremental_builds))):
            build_number = int(incremental_builds[index])
            logging.debug("Merging build %s, incremental json index: %d.",
                build_number, index)

            # Return if not all build numbers in the incremental json results
            # are newer than the most recent build in the aggregated results.
            # FIXME: make this case work.
            if build_number < aggregated_build_number:
                logging.warning(("Build %d in incremental json is older than "
                    "the most recent build in aggregated results: %d"),
                    build_number, aggregated_build_number)
                return False

            # Return if the build number is duplicated.
            # FIXME: skip the duplicated build and merge rest of the results.
            #        Need to be careful on skiping the corresponding value in
            #        _merge_tests because the property data for each test could
            #        be accumulated.
            if build_number == aggregated_build_number:
                logging.warning("Duplicate build %d in incremental json",
                    build_number)
                return False

            # Merge this build into aggreagated results.
            cls._merge_one_build(aggregated_json, incremental_json, index, num_runs)

        return True

    @classmethod
    def _merge_one_build(cls, aggregated_json, incremental_json,
                         incremental_index, num_runs):
        """Merge one build of incremental json into aggregated json results.

        Args:
            aggregated_json: aggregated json object.
            incremental_json: incremental json object.
            incremental_index: index of the incremental json results to merge.
        """

        for key in incremental_json.keys():
            # Merge json results except "tests" properties (results, times etc).
            # "tests" properties will be handled separately.
            if key == JSON_RESULTS_TESTS:
                continue

            if key in aggregated_json:
                aggregated_json[key].insert(
                    0, incremental_json[key][incremental_index])
                aggregated_json[key] = \
                    aggregated_json[key][:num_runs]
            else:
                aggregated_json[key] = incremental_json[key]

    @classmethod
    def _merge_tests(cls, aggregated_json, incremental_json, num_runs):
        """Merge "tests" properties:results, times.

        Args:
            aggregated_json: aggregated json object.
            incremental_json: incremental json object.
        """

        all_tests = (set(aggregated_json.iterkeys()) |
                     set(incremental_json.iterkeys()))
        for test_name in all_tests:
            if test_name in aggregated_json:
                aggregated_test = aggregated_json[test_name]
                if test_name in incremental_json:
                    incremental_test = incremental_json[test_name]
                    results = incremental_test[JSON_RESULTS_RESULTS]
                    times = incremental_test[JSON_RESULTS_TIMES]
                else:
                    results = [[1, JSON_RESULTS_NO_DATA]]
                    times = [[1, 0]]

                cls._insert_item_run_length_encoded(
                    results, aggregated_test[JSON_RESULTS_RESULTS], num_runs)
                cls._insert_item_run_length_encoded(
                    times, aggregated_test[JSON_RESULTS_TIMES], num_runs)
                cls._normalize_results_json(test_name, aggregated_json)
            else:
                aggregated_json[test_name] = incremental_json[test_name]

    @classmethod
    def _insert_item_run_length_encoded(cls, incremental_item, aggregated_item, num_runs):
        """Inserts the incremental run-length encoded results into the aggregated
           run-length encoded results.

        Args:
            incremental_item: incremental run-length encoded results.
            aggregated_item: aggregated run-length encoded results.
        """

        for item in incremental_item:
            if len(aggregated_item) and item[1] == aggregated_item[0][1]:
                aggregated_item[0][0] = min(
                    aggregated_item[0][0] + item[0], num_runs)
            else:
                aggregated_item.insert(0, item)

    @classmethod
    def _normalize_results_json(cls, test_name, aggregated_json):
        """ Prune tests where all runs pass or tests that no longer exist and
        truncate all results to JSON_RESULTS_MAX_BUILDS.

        Args:
          test_name: Name of the test.
          aggregated_json: The JSON object with all the test results for
                           this builder.
        """

        aggregated_test = aggregated_json[test_name]
        aggregated_test[JSON_RESULTS_RESULTS] = \
            cls._remove_items_over_max_number_of_builds(
                aggregated_test[JSON_RESULTS_RESULTS])
        aggregated_test[JSON_RESULTS_TIMES] = \
            cls._remove_items_over_max_number_of_builds(
                aggregated_test[JSON_RESULTS_TIMES])

        is_all_pass = cls._is_results_all_of_type(
            aggregated_test[JSON_RESULTS_RESULTS], JSON_RESULTS_PASS)
        is_all_no_data = cls._is_results_all_of_type(
            aggregated_test[JSON_RESULTS_RESULTS], JSON_RESULTS_NO_DATA)

        max_time = max(
            [time[1] for time in aggregated_test[JSON_RESULTS_TIMES]])
        # Remove all passes/no-data from the results to reduce noise and
        # filesize. If a test passes every run, but
        # takes >= JSON_RESULTS_MIN_TIME to run, don't throw away the data.
        if (is_all_no_data or
           (is_all_pass and max_time < JSON_RESULTS_MIN_TIME)):
            del aggregated_json[test_name]

    @classmethod
    def _remove_items_over_max_number_of_builds(cls, encoded_list):
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
            if num_builds > JSON_RESULTS_MAX_BUILDS:
                return encoded_list[:index]

        return encoded_list

    @classmethod
    def _is_results_all_of_type(cls, results, type):
        """Returns whether all the results are of the given type
        (e.g. all passes).
        """

        return len(results) == 1 and results[0][1] == type

    @classmethod
    def _check_json(cls, builder, json):
        """Check whether the given json is valid.

        Args:
            builder: builder name this json is for.
            json: json object to check.

        Returns:
            True if the json is valid or
            False otherwise.
        """

        version = json[JSON_RESULTS_VERSION_KEY]
        if version > JSON_RESULTS_VERSION:
            logging.error("Results JSON version '%s' is not supported.",
                version)
            return False

        if not builder in json:
            logging.error("Builder '%s' is not in json results.", builder)
            return False

        results_for_builder = json[builder]
        if not JSON_RESULTS_BUILD_NUMBERS in results_for_builder:
            logging.error("Missing build number in json results.")
            return False

        return True

    @classmethod
    def merge(cls, builder, aggregated, incremental, num_runs, sort_keys=False):
        """Merge incremental json file data with aggregated json file data.

        Args:
            builder: builder name.
            aggregated: aggregated json file data.
            incremental: incremental json file data.
            sort_key: whether or not to sort key when dumping json results.

        Returns:
            Merged json file data if merge succeeds or
            None on failure.
        """

        if not incremental:
            logging.warning("Nothing to merge.")
            return None

        logging.info("Loading incremental json...")
        incremental_json = cls._load_json(incremental)
        if not incremental_json:
            return None

        logging.info("Checking incremental json...")
        if not cls._check_json(builder, incremental_json):
            return None

        logging.info("Loading existing aggregated json...")
        aggregated_json = cls._load_json(aggregated)
        if not aggregated_json:
            return incremental

        logging.info("Checking existing aggregated json...")
        if not cls._check_json(builder, aggregated_json):
            return incremental

        logging.info("Merging json results...")
        try:
            if not cls._merge_json(aggregated_json[builder], incremental_json[builder], num_runs):
                return None
        except Exception, err:
            logging.error("Failed to merge json results: %s", str(err))
            return None

        aggregated_json[JSON_RESULTS_VERSION_KEY] = JSON_RESULTS_VERSION

        return cls._generate_file_data(aggregated_json, sort_keys)

    @classmethod
    def update(cls, master, builder, test_type, incremental):
        """Update datastore json file data by merging it with incremental json
           file. Writes the large file and a small file. The small file just stores
           fewer runs.

        Args:
            master: master name.
            builder: builder name.
            test_type: type of test results.
            incremental: incremental json file data to merge.

        Returns:
            Large TestFile object if update succeeds or
            None on failure.
        """
        small_file_updated = cls.update_file(master, builder, test_type, incremental, JSON_RESULTS_FILE_SMALL, JSON_RESULTS_MAX_BUILDS_SMALL)
        large_file_updated = cls.update_file(master, builder, test_type, incremental, JSON_RESULTS_FILE, JSON_RESULTS_MAX_BUILDS)

        return small_file_updated and large_file_updated

    @classmethod
    def update_file(cls, master, builder, test_type, incremental, filename, num_runs):
        files = TestFile.get_files(master, builder, test_type, filename)
        if files:
            file = files[0]
            new_results = cls.merge(builder, file.data, incremental, num_runs)
        else:
            # Use the incremental data if there is no aggregated file to merge.
            file = TestFile()            
            file.master = master
            file.builder = builder
            file.test_type = test_type
            file.name = filename
            new_results = incremental
            logging.info("No existing json results, incremental json is saved.")

        if not new_results or not file.save(new_results):
            logging.info(
                "Update failed, master: %s, builder: %s, test_type: %s, name: %s." %
                (master, builder, test_type, filename))
            return False

        return True

    @classmethod
    def get_test_list(cls, builder, json_file_data):
        """Get list of test names from aggregated json file data.

        Args:
            json_file_data: json file data that has all test-data and
                            non-test-data.

        Returns:
            json file with test name list only. The json format is the same
            as the one saved in datastore, but all non-test-data and test detail
            results are removed.
        """

        logging.debug("Loading test results json...")
        json = cls._load_json(json_file_data)
        if not json:
            return None

        logging.debug("Checking test results json...")
        if not cls._check_json(builder, json):
            return None

        test_list_json = {}
        tests = json[builder][JSON_RESULTS_TESTS]
        test_list_json[builder] = {
            "tests": dict.fromkeys(tests, {})}

        return cls._generate_file_data(test_list_json)
