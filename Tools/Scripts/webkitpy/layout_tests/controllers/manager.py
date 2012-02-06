#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

"""
The Manager runs a series of tests (TestType interface) against a set
of test files.  If a test file fails a TestType, it returns a list of TestFailure
objects to the Manager. The Manager then aggregates the TestFailures to
create a final report.
"""

from __future__ import with_statement

import errno
import logging
import math
import Queue
import random
import re
import sys
import time

from webkitpy.layout_tests.controllers import manager_worker_broker
from webkitpy.layout_tests.controllers import worker
from webkitpy.layout_tests.layout_package import json_layout_results_generator
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models.test_input import TestInput
from webkitpy.layout_tests.models.result_summary import ResultSummary
from webkitpy.layout_tests.views import printing

from webkitpy.tool import grammar

_log = logging.getLogger(__name__)

# Builder base URL where we have the archived test results.
BUILDER_BASE_URL = "http://build.chromium.org/buildbot/layout_test_results/"

TestExpectations = test_expectations.TestExpectations


def interpret_test_failures(port, test_name, failures):
    """Interpret test failures and returns a test result as dict.

    Args:
        port: interface to port-specific hooks
        test_name: test name relative to layout_tests directory
        failures: list of test failures
    Returns:
        A dictionary like {'is_reftest': True, ...}
    """
    test_dict = {}
    failure_types = [type(failure) for failure in failures]
    # FIXME: get rid of all this is_* values once there is a 1:1 map between
    # TestFailure type and test_expectations.EXPECTATION.
    if test_failures.FailureMissingAudio in failure_types:
        test_dict['is_missing_audio'] = True

    for failure in failures:
        if isinstance(failure, test_failures.FailureImageHashMismatch):
            test_dict['image_diff_percent'] = failure.diff_percent
        elif isinstance(failure, test_failures.FailureReftestMismatch):
            test_dict['is_reftest'] = True
            test_dict['ref_file'] = port.relative_test_filename(failure.reference_filename)
        elif isinstance(failure, test_failures.FailureReftestMismatchDidNotOccur):
            test_dict['is_mismatch_reftest'] = True
            test_dict['ref_file'] = port.relative_test_filename(failure.reference_filename)

    if test_failures.FailureMissingResult in failure_types:
        test_dict['is_missing_text'] = True

    if test_failures.FailureMissingImage in failure_types or test_failures.FailureMissingImageHash in failure_types:
        test_dict['is_missing_image'] = True
    return test_dict


def use_trac_links_in_results_html(port_obj):
    # We only use trac links on the buildbots.
    # Use existence of builder_name as a proxy for knowing we're on a bot.
    return port_obj.get_option("builder_name")

# FIXME: This should be on the Manager class (since that's the only caller)
# or split off from Manager onto another helper class, but should not be a free function.
# Most likely this should be made into its own class, and this super-long function
# split into many helper functions.
def summarize_results(port_obj, expectations, result_summary, retry_summary, test_timings, only_unexpected, interrupted):
    """Summarize failing results as a dict.

    FIXME: split this data structure into a separate class?

    Args:
        port_obj: interface to port-specific hooks
        expectations: test_expectations.TestExpectations object
        result_summary: summary object from initial test runs
        retry_summary: summary object from final test run of retried tests
        test_timings: a list of TestResult objects which contain test runtimes in seconds
        only_unexpected: whether to return a summary only for the unexpected results
    Returns:
        A dictionary containing a summary of the unexpected results from the
        run, with the following fields:
        'version': a version indicator
        'fixable': The number of fixable tests (NOW - PASS)
        'skipped': The number of skipped tests (NOW & SKIPPED)
        'num_regressions': The number of non-flaky failures
        'num_flaky': The number of flaky failures
        'num_missing': The number of tests with missing results
        'num_passes': The number of unexpected passes
        'tests': a dict of tests -> {'expected': '...', 'actual': '...'}
    """
    results = {}
    results['version'] = 3

    tbe = result_summary.tests_by_expectation
    tbt = result_summary.tests_by_timeline
    results['fixable'] = len(tbt[test_expectations.NOW] - tbe[test_expectations.PASS])
    results['skipped'] = len(tbt[test_expectations.NOW] & tbe[test_expectations.SKIP])

    num_passes = 0
    num_flaky = 0
    num_missing = 0
    num_regressions = 0
    keywords = {}
    for expecation_string, expectation_enum in TestExpectations.EXPECTATIONS.iteritems():
        keywords[expectation_enum] = expecation_string.upper()

    for modifier_string, modifier_enum in TestExpectations.MODIFIERS.iteritems():
        keywords[modifier_enum] = modifier_string.upper()

    tests = {}
    original_results = result_summary.unexpected_results if only_unexpected else result_summary.results

    for test_name, result in original_results.iteritems():
        # Note that if a test crashed in the original run, we ignore
        # whether or not it crashed when we retried it (if we retried it),
        # and always consider the result not flaky.
        expected = expectations.get_expectations_string(test_name)
        result_type = result.type
        actual = [keywords[result_type]]

        if result_type == test_expectations.SKIP:
            continue

        test_dict = {}
        if result.has_stderr:
            test_dict['has_stderr'] = True

        if result_type == test_expectations.PASS:
            num_passes += 1
            # FIXME: include passing tests that have stderr output.
            if expected == 'PASS':
                continue
        elif result_type == test_expectations.CRASH:
            num_regressions += 1
        elif result_type == test_expectations.MISSING:
            if test_name in result_summary.unexpected_results:
                num_missing += 1
        elif test_name in result_summary.unexpected_results:
            if test_name not in retry_summary.unexpected_results:
                actual.extend(expectations.get_expectations_string(test_name).split(" "))
                num_flaky += 1
            else:
                retry_result_type = retry_summary.unexpected_results[test_name].type
                if result_type != retry_result_type:
                    actual.append(keywords[retry_result_type])
                    num_flaky += 1
                else:
                    num_regressions += 1

        test_dict['expected'] = expected
        test_dict['actual'] = " ".join(actual)
        # FIXME: Set this correctly once https://webkit.org/b/37739 is fixed
        # and only set it if there actually is stderr data.

        test_dict.update(interpret_test_failures(port_obj, test_name, result.failures))

        # Store test hierarchically by directory. e.g.
        # foo/bar/baz.html: test_dict
        # foo/bar/baz1.html: test_dict
        #
        # becomes
        # foo: {
        #     bar: {
        #         baz.html: test_dict,
        #         baz1.html: test_dict
        #     }
        # }
        parts = test_name.split('/')
        current_map = tests
        for i, part in enumerate(parts):
            if i == (len(parts) - 1):
                current_map[part] = test_dict
                break
            if part not in current_map:
                current_map[part] = {}
            current_map = current_map[part]

    results['tests'] = tests
    results['num_passes'] = num_passes
    results['num_flaky'] = num_flaky
    results['num_missing'] = num_missing
    results['num_regressions'] = num_regressions
    results['uses_expectations_file'] = port_obj.uses_test_expectations_file()
    results['interrupted'] = interrupted  # Does results.html have enough information to compute this itself? (by checking total number of results vs. total number of tests?)
    results['layout_tests_dir'] = port_obj.layout_tests_dir()
    results['has_wdiff'] = port_obj.wdiff_available()
    results['has_pretty_patch'] = port_obj.pretty_patch_available()
    try:
        # We only use the svn revision for using trac links in the results.html file,
        # Don't do this by default since it takes >100ms.
        if use_trac_links_in_results_html(port_obj):
            results['revision'] = port_obj.host.scm().head_svn_revision()
    except Exception, e:
        _log.warn("Failed to determine svn revision for checkout (cwd: %s, webkit_base: %s), leaving 'revision' key blank in full_results.json.\n%s" % (port_obj._filesystem.getcwd(), port_obj.path_from_webkit_base(), e))
        # Handle cases where we're running outside of version control.
        import traceback
        _log.debug('Failed to learn head svn revision:')
        _log.debug(traceback.format_exc())
        results['revision'] = ""

    return results


class TestRunInterruptedException(Exception):
    """Raised when a test run should be stopped immediately."""
    def __init__(self, reason):
        Exception.__init__(self)
        self.reason = reason
        self.msg = reason

    def __reduce__(self):
        return self.__class__, (self.reason,)


class WorkerException(Exception):
    """Raised when we receive an unexpected/unknown exception from a worker."""
    pass


class TestShard(object):
    """A test shard is a named list of TestInputs."""

    # FIXME: Make this class visible, used by workers as well.
    def __init__(self, name, test_inputs):
        self.name = name
        self.test_inputs = test_inputs

    def __repr__(self):
        return "TestShard(name='%s', test_inputs=%s'" % (self.name, self.test_inputs)

    def __eq__(self, other):
        return self.name == other.name and self.test_inputs == other.test_inputs


class Manager(object):
    """A class for managing running a series of tests on a series of layout
    test files."""


    # The per-test timeout in milliseconds, if no --time-out-ms option was
    # given to run_webkit_tests. This should correspond to the default timeout
    # in DumpRenderTree.
    DEFAULT_TEST_TIMEOUT_MS = 6 * 1000

    def __init__(self, port, options, printer):
        """Initialize test runner data structures.

        Args:
          port: an object implementing port-specific
          options: a dictionary of command line options
          printer: a Printer object to record updates to.
        """
        self._port = port
        self._filesystem = port.host.filesystem
        self._options = options
        self._printer = printer
        self._message_broker = None
        self._expectations = None

        self.HTTP_SUBDIR = 'http' + port.TEST_PATH_SEPARATOR
        self.WEBSOCKET_SUBDIR = 'websocket' + port.TEST_PATH_SEPARATOR
        self.LAYOUT_TESTS_DIRECTORY = 'LayoutTests'
        self._has_http_lock = False

        self._remaining_locked_shards = []

        # disable wss server. need to install pyOpenSSL on buildbots.
        # self._websocket_secure_server = websocket_server.PyWebSocket(
        #        options.results_directory, use_tls=True, port=9323)

        # a set of test files, and the same tests as a list

        # FIXME: Rename to test_names.
        self._test_files = set()
        self._test_files_list = None
        self._result_queue = Queue.Queue()
        self._retrying = False
        self._results_directory = self._port.results_directory()

        self._all_results = []
        self._group_stats = {}
        self._current_result_summary = None

        # This maps worker names to the state we are tracking for each of them.
        self._worker_states = {}

    def collect_tests(self, args):
        """Find all the files to test.

        Args:
          args: list of test arguments from the command line

        """
        paths = self._strip_test_dir_prefixes(args)
        if self._options.test_list:
            paths += self._strip_test_dir_prefixes(read_test_files(self._filesystem, self._options.test_list, self._port.TEST_PATH_SEPARATOR))
        self._test_files = self._port.tests(paths)

    def _strip_test_dir_prefixes(self, paths):
        return [self._strip_test_dir_prefix(path) for path in paths if path]

    def _strip_test_dir_prefix(self, path):
        # Handle both "LayoutTests/foo/bar.html" and "LayoutTests\foo\bar.html" if
        # the filesystem uses '\\' as a directory separator.
        if path.startswith(self.LAYOUT_TESTS_DIRECTORY + self._port.TEST_PATH_SEPARATOR):
            return path[len(self.LAYOUT_TESTS_DIRECTORY + self._port.TEST_PATH_SEPARATOR):]
        if path.startswith(self.LAYOUT_TESTS_DIRECTORY + self._filesystem.sep):
            return path[len(self.LAYOUT_TESTS_DIRECTORY + self._filesystem.sep):]
        return path

    def _is_http_test(self, test):
        return self.HTTP_SUBDIR in test or self.WEBSOCKET_SUBDIR in test

    def _http_tests(self):
        return set(test for test in self._test_files if self._is_http_test(test))

    def parse_expectations(self):
        """Parse the expectations from the test_list files and return a data
        structure holding them. Throws an error if the test_list files have
        invalid syntax."""
        port = self._port
        self._expectations = test_expectations.TestExpectations(
            port,
            self._test_files,
            port.test_expectations(),
            port.test_configuration(),
            self._options.lint_test_files,
            port.test_expectations_overrides())

    def _split_into_chunks_if_necessary(self, skipped):
        if not self._options.run_chunk and not self._options.run_part:
            return skipped

        # If the user specifies they just want to run a subset of the tests,
        # just grab a subset of the non-skipped tests.
        chunk_value = self._options.run_chunk or self._options.run_part
        test_files = self._test_files_list
        try:
            (chunk_num, chunk_len) = chunk_value.split(":")
            chunk_num = int(chunk_num)
            assert(chunk_num >= 0)
            test_size = int(chunk_len)
            assert(test_size > 0)
        except AssertionError:
            _log.critical("invalid chunk '%s'" % chunk_value)
            return None

        # Get the number of tests
        num_tests = len(test_files)

        # Get the start offset of the slice.
        if self._options.run_chunk:
            chunk_len = test_size
            # In this case chunk_num can be really large. We need
            # to make the slave fit in the current number of tests.
            slice_start = (chunk_num * chunk_len) % num_tests
        else:
            # Validate the data.
            assert(test_size <= num_tests)
            assert(chunk_num <= test_size)

            # To count the chunk_len, and make sure we don't skip
            # some tests, we round to the next value that fits exactly
            # all the parts.
            rounded_tests = num_tests
            if rounded_tests % test_size != 0:
                rounded_tests = (num_tests + test_size - (num_tests % test_size))

            chunk_len = rounded_tests / test_size
            slice_start = chunk_len * (chunk_num - 1)
            # It does not mind if we go over test_size.

        # Get the end offset of the slice.
        slice_end = min(num_tests, slice_start + chunk_len)

        files = test_files[slice_start:slice_end]

        tests_run_msg = 'Running: %d tests (chunk slice [%d:%d] of %d)' % ((slice_end - slice_start), slice_start, slice_end, num_tests)
        self._printer.print_expected(tests_run_msg)

        # If we reached the end and we don't have enough tests, we run some
        # from the beginning.
        if slice_end - slice_start < chunk_len:
            extra = chunk_len - (slice_end - slice_start)
            extra_msg = ('   last chunk is partial, appending [0:%d]' % extra)
            self._printer.print_expected(extra_msg)
            tests_run_msg += "\n" + extra_msg
            files.extend(test_files[0:extra])
        tests_run_filename = self._filesystem.join(self._results_directory, "tests_run.txt")
        self._filesystem.write_text_file(tests_run_filename, tests_run_msg)

        len_skip_chunk = int(len(files) * len(skipped) / float(len(self._test_files)))
        skip_chunk_list = list(skipped)[0:len_skip_chunk]
        skip_chunk = set(skip_chunk_list)

        # FIXME: This is a total hack.
        # Update expectations so that the stats are calculated correctly.
        # We need to pass a list that includes the right # of skipped files
        # to ParseExpectations so that ResultSummary() will get the correct
        # stats. So, we add in the subset of skipped files, and then
        # subtract them back out.
        self._test_files_list = files + skip_chunk_list
        self._test_files = set(self._test_files_list)

        self.parse_expectations()

        self._test_files = set(files)
        self._test_files_list = files

        return skip_chunk

    # FIXME: This method is way too long and needs to be broken into pieces.
    def prepare_lists_and_print_output(self):
        """Create appropriate subsets of test lists and returns a
        ResultSummary object. Also prints expected test counts.
        """

        # Remove skipped - both fixable and ignored - files from the
        # top-level list of files to test.
        num_all_test_files = len(self._test_files)
        self._printer.print_expected("Found:  %d tests" % (len(self._test_files)))
        if not num_all_test_files:
            _log.critical('No tests to run.')
            return None

        skipped = set()

        if not self._options.http:
            skipped = skipped.union(self._http_tests())

        if num_all_test_files > 1 and not self._options.force:
            skipped = skipped.union(self._expectations.get_tests_with_result_type(test_expectations.SKIP))
            if self._options.skip_failing_tests:
                failing = self._expectations.get_tests_with_result_type(test_expectations.FAIL)
                self._test_files -= failing

        self._test_files -= skipped

        # Create a sorted list of test files so the subset chunk,
        # if used, contains alphabetically consecutive tests.
        self._test_files_list = list(self._test_files)
        if self._options.randomize_order:
            random.shuffle(self._test_files_list)
        else:
            self._test_files_list.sort(key=lambda test: test_key(self._port, test))

        skipped = self._split_into_chunks_if_necessary(skipped)

        # FIXME: It's unclear how --repeat-each and --iterations should interact with chunks?
        if self._options.repeat_each:
            list_with_repetitions = []
            for test in self._test_files_list:
                list_with_repetitions += ([test] * self._options.repeat_each)
            self._test_files_list = list_with_repetitions

        if self._options.iterations:
            self._test_files_list = self._test_files_list * self._options.iterations

        iterations =  \
            (self._options.repeat_each if self._options.repeat_each else 1) * \
            (self._options.iterations if self._options.iterations else 1)
        result_summary = ResultSummary(self._expectations, self._test_files | skipped, iterations)
        self._print_expected_results_of_type(result_summary, test_expectations.PASS, "passes")
        self._print_expected_results_of_type(result_summary, test_expectations.FAIL, "failures")
        self._print_expected_results_of_type(result_summary, test_expectations.FLAKY, "flaky")
        self._print_expected_results_of_type(result_summary, test_expectations.SKIP, "skipped")

        if self._options.force:
            self._printer.print_expected('Running all tests, including skips (--force)')
        else:
            # Note that we don't actually run the skipped tests (they were
            # subtracted out of self._test_files, above), but we stub out the
            # results here so the statistics can remain accurate.
            for test in skipped:
                result = test_results.TestResult(test)
                result.type = test_expectations.SKIP
                iterations =  \
                    (self._options.repeat_each if self._options.repeat_each else 1) * \
                    (self._options.iterations if self._options.iterations else 1)
                for iteration in range(iterations):
                    result_summary.add(result, expected=True)
        self._printer.print_expected('')

        # Check to make sure we didn't filter out all of the tests.
        if not len(self._test_files):
            _log.info("All tests are being skipped")
            return None

        return result_summary

    def _get_dir_for_test_file(self, test_file):
        """Returns the highest-level directory by which to shard the given
        test file."""
        directory, test_file = self._port.split_test(test_file)

        # The http tests are very stable on mac/linux.
        # TODO(ojan): Make the http server on Windows be apache so we can
        # turn shard the http tests there as well. Switching to apache is
        # what made them stable on linux/mac.
        return directory

    def _get_test_input_for_file(self, test_file):
        """Returns the appropriate TestInput object for the file. Mostly this
        is used for looking up the timeout value (in ms) to use for the given
        test."""
        if self._test_is_slow(test_file):
            return TestInput(test_file, self._options.slow_time_out_ms)
        return TestInput(test_file, self._options.time_out_ms)

    def _test_requires_lock(self, test_file):
        """Return True if the test needs to be locked when
        running multiple copies of NRWTs."""
        return self._is_http_test(test_file)

    def _test_is_slow(self, test_file):
        return self._expectations.has_modifier(test_file, test_expectations.SLOW)

    def _shard_tests(self, test_files, num_workers, fully_parallel):
        """Groups tests into batches.
        This helps ensure that tests that depend on each other (aka bad tests!)
        continue to run together as most cross-tests dependencies tend to
        occur within the same directory.
        Return:
            Two list of TestShards. The first contains tests that must only be
            run under the server lock, the second can be run whenever.
        """

        # FIXME: Move all of the sharding logic out of manager into its
        # own class or module. Consider grouping it with the chunking logic
        # in prepare_lists as well.
        if num_workers == 1:
            return self._shard_in_two(test_files)
        elif fully_parallel:
            return self._shard_every_file(test_files)
        return self._shard_by_directory(test_files, num_workers)

    def _shard_in_two(self, test_files):
        """Returns two lists of shards, one with all the tests requiring a lock and one with the rest.

        This is used when there's only one worker, to minimize the per-shard overhead."""
        locked_inputs = []
        unlocked_inputs = []
        for test_file in test_files:
            test_input = self._get_test_input_for_file(test_file)
            if self._test_requires_lock(test_file):
                locked_inputs.append(test_input)
            else:
                unlocked_inputs.append(test_input)

        locked_shards = []
        unlocked_shards = []
        if locked_inputs:
            locked_shards = [TestShard('locked_tests', locked_inputs)]
        if unlocked_inputs:
            unlocked_shards = [TestShard('unlocked_tests', unlocked_inputs)]

        return locked_shards, unlocked_shards

    def _shard_every_file(self, test_files):
        """Returns two lists of shards, each shard containing a single test file.

        This mode gets maximal parallelism at the cost of much higher flakiness."""
        locked_shards = []
        unlocked_shards = []
        for test_file in test_files:
            test_input = self._get_test_input_for_file(test_file)

            # Note that we use a '.' for the shard name; the name doesn't really
            # matter, and the only other meaningful value would be the filename,
            # which would be really redundant.
            if self._test_requires_lock(test_file):
                locked_shards.append(TestShard('.', [test_input]))
            else:
                unlocked_shards.append(TestShard('.', [test_input]))

        return locked_shards, unlocked_shards

    def _shard_by_directory(self, test_files, num_workers):
        """Returns two lists of shards, each shard containing all the files in a directory.

        This is the default mode, and gets as much parallelism as we can while
        minimizing flakiness caused by inter-test dependencies."""
        locked_shards = []
        unlocked_shards = []
        tests_by_dir = {}
        # FIXME: Given that the tests are already sorted by directory,
        # we can probably rewrite this to be clearer and faster.
        for test_file in test_files:
            directory = self._get_dir_for_test_file(test_file)
            test_input = self._get_test_input_for_file(test_file)
            tests_by_dir.setdefault(directory, [])
            tests_by_dir[directory].append(test_input)

        for directory, test_inputs in tests_by_dir.iteritems():
            shard = TestShard(directory, test_inputs)
            if self._test_requires_lock(directory):
                locked_shards.append(shard)
            else:
                unlocked_shards.append(shard)

        # Sort the shards by directory name.
        locked_shards.sort(key=lambda shard: shard.name)
        unlocked_shards.sort(key=lambda shard: shard.name)

        return (self._resize_shards(locked_shards, self._max_locked_shards(num_workers),
                                    'locked_shard'),
                unlocked_shards)

    def _max_locked_shards(self, num_workers):
        # Put a ceiling on the number of locked shards, so that we
        # don't hammer the servers too badly.

        # FIXME: For now, limit to one shard or set it
        # with the --max-locked-shards. After testing to make sure we
        # can handle multiple shards, we should probably do something like
        # limit this to no more than a quarter of all workers, e.g.:
        # return max(math.ceil(num_workers / 4.0), 1)
        if self._options.max_locked_shards:
            num_of_locked_shards = self._options.max_locked_shards
        else:
            num_of_locked_shards = 1

        return num_of_locked_shards

    def _resize_shards(self, old_shards, max_new_shards, shard_name_prefix):
        """Takes a list of shards and redistributes the tests into no more
        than |max_new_shards| new shards."""

        # This implementation assumes that each input shard only contains tests from a
        # single directory, and that tests in each shard must remain together; as a
        # result, a given input shard is never split between output shards.
        #
        # Each output shard contains the tests from one or more input shards and
        # hence may contain tests from multiple directories.

        def divide_and_round_up(numerator, divisor):
            return int(math.ceil(float(numerator) / divisor))

        def extract_and_flatten(shards):
            test_inputs = []
            for shard in shards:
                test_inputs.extend(shard.test_inputs)
            return test_inputs

        def split_at(seq, index):
            return (seq[:index], seq[index:])

        num_old_per_new = divide_and_round_up(len(old_shards), max_new_shards)
        new_shards = []
        remaining_shards = old_shards
        while remaining_shards:
            some_shards, remaining_shards = split_at(remaining_shards, num_old_per_new)
            new_shards.append(TestShard('%s_%d' % (shard_name_prefix, len(new_shards) + 1),
                                        extract_and_flatten(some_shards)))
        return new_shards

    def _log_num_workers(self, num_workers, num_shards, num_locked_shards):
        driver_name = self._port.driver_name()
        if num_workers == 1:
            self._printer.print_config("Running 1 %s over %s" %
                (driver_name, grammar.pluralize('shard', num_shards)))
        else:
            self._printer.print_config("Running %d %ss in parallel over %d shards (%d locked)" %
                (num_workers, driver_name, num_shards, num_locked_shards))

    def _run_tests(self, file_list, result_summary):
        """Runs the tests in the file_list.

        Return: A tuple (interrupted, keyboard_interrupted, thread_timings,
            test_timings, individual_test_timings)
            interrupted is whether the run was interrupted
            keyboard_interrupted is whether the interruption was because someone
              typed Ctrl^C
            thread_timings is a list of dicts with the total runtime
              of each thread with 'name', 'num_tests', 'total_time' properties
            test_timings is a list of timings for each sharded subdirectory
              of the form [time, directory_name, num_tests]
            individual_test_timings is a list of run times for each test
              in the form {filename:filename, test_run_time:test_run_time}
            result_summary: summary object to populate with the results
        """
        self._current_result_summary = result_summary
        self._all_results = []
        self._group_stats = {}
        self._worker_states = {}

        keyboard_interrupted = False
        interrupted = False
        thread_timings = []

        self._printer.print_update('Sharding tests ...')
        locked_shards, unlocked_shards = self._shard_tests(file_list, int(self._options.child_processes), self._options.experimental_fully_parallel)

        # FIXME: We don't have a good way to coordinate the workers so that
        # they don't try to run the shards that need a lock if we don't actually
        # have the lock. The easiest solution at the moment is to grab the
        # lock at the beginning of the run, and then run all of the locked
        # shards first. This minimizes the time spent holding the lock, but
        # means that we won't be running tests while we're waiting for the lock.
        # If this becomes a problem in practice we'll need to change this.

        all_shards = locked_shards + unlocked_shards
        self._remaining_locked_shards = locked_shards
        if locked_shards:
            self.start_servers_with_lock()

        num_workers = min(int(self._options.child_processes), len(all_shards))
        self._log_num_workers(num_workers, len(all_shards), len(locked_shards))

        manager_connection = manager_worker_broker.get(self._port, self._options, self, worker.Worker)

        if self._options.dry_run:
            return (keyboard_interrupted, interrupted, thread_timings, self._group_stats, self._all_results)

        self._printer.print_update('Starting %s ...' % grammar.pluralize('worker', num_workers))
        for worker_number in xrange(num_workers):
            worker_connection = manager_connection.start_worker(worker_number, self.results_directory())
            worker_state = _WorkerState(worker_number, worker_connection)
            self._worker_states[worker_connection.name] = worker_state

            # FIXME: If we start workers up too quickly, DumpRenderTree appears
            # to thrash on something and time out its first few tests. Until
            # we can figure out what's going on, sleep a bit in between
            # workers. This needs a bug filed.
            time.sleep(0.1)

        self._printer.print_update("Starting testing ...")
        for shard in all_shards:
            # FIXME: Change 'test_list' to 'shard', make sharding public.
            manager_connection.post_message('test_list', shard.name, shard.test_inputs)

        # We post one 'stop' message for each worker. Because the stop message
        # are sent after all of the tests, and because each worker will stop
        # reading messsages after receiving a stop, we can be sure each
        # worker will get a stop message and hence they will all shut down.
        for _ in xrange(num_workers):
            manager_connection.post_message('stop')

        try:
            while not self.is_done():
                manager_connection.run_message_loop(delay_secs=1.0)

            # Make sure all of the workers have shut down (if possible).
            for worker_state in self._worker_states.values():
                if worker_state.worker_connection.is_alive():
                    _log.debug('Waiting for worker %d to exit' % worker_state.number)
                    worker_state.worker_connection.join(5.0)
                    if worker_state.worker_connection.is_alive():
                        _log.error('Worker %d did not exit in time.' % worker_state.number)

        except KeyboardInterrupt:
            self._printer.print_update('Interrupted, exiting ...')
            self.cancel_workers()
            keyboard_interrupted = True
        except TestRunInterruptedException, e:
            _log.warning(e.reason)
            self.cancel_workers()
            interrupted = True
        except WorkerException:
            self.cancel_workers()
            raise
        except:
            # Unexpected exception; don't try to clean up workers.
            _log.error("Exception raised, exiting")
            self.cancel_workers()
            raise
        finally:
            self.stop_servers_with_lock()

        thread_timings = [worker_state.stats for worker_state in self._worker_states.values()]

        # FIXME: should this be a class instead of a tuple?
        return (interrupted, keyboard_interrupted, thread_timings, self._group_stats, self._all_results)

    def results_directory(self):
        if not self._retrying:
            return self._results_directory
        else:
            self._filesystem.maybe_make_directory(self._filesystem.join(self._results_directory, 'retries'))
            return self._filesystem.join(self._results_directory, 'retries')

    def update(self):
        self.update_summary(self._current_result_summary)

    def _collect_timing_info(self, threads):
        test_timings = {}
        individual_test_timings = []
        thread_timings = []

        for thread in threads:
            thread_timings.append({'name': thread.getName(),
                                   'num_tests': thread.get_num_tests(),
                                   'total_time': thread.get_total_time()})
            test_timings.update(thread.get_test_group_timing_stats())
            individual_test_timings.extend(thread.get_test_results())

        return (thread_timings, test_timings, individual_test_timings)

    def needs_servers(self):
        return any(self._test_requires_lock(test_name) for test_name in self._test_files) and self._options.http

    def set_up_run(self):
        """Configures the system to be ready to run tests.

        Returns a ResultSummary object if we should continue to run tests,
        or None if we should abort.

        """
        # This must be started before we check the system dependencies,
        # since the helper may do things to make the setup correct.
        self._printer.print_update("Starting helper ...")
        self._port.start_helper()

        # Check that the system dependencies (themes, fonts, ...) are correct.
        if not self._options.nocheck_sys_deps:
            self._printer.print_update("Checking system dependencies ...")
            if not self._port.check_sys_deps(self.needs_servers()):
                self._port.stop_helper()
                return None

        if self._options.clobber_old_results:
            self._clobber_old_results()

        # Create the output directory if it doesn't already exist.
        self._port.maybe_make_directory(self._results_directory)

        self._port.setup_test_run()

        self._printer.print_update("Preparing tests ...")
        result_summary = self.prepare_lists_and_print_output()
        if not result_summary:
            return None

        return result_summary

    def run(self, result_summary):
        """Run all our tests on all our test files.

        For each test file, we run each test type. If there are any failures,
        we collect them for reporting.

        Args:
          result_summary: a summary object tracking the test results.

        Return:
          The number of unexpected results (0 == success)
        """
        # collect_tests() must have been called first to initialize us.
        # If we didn't find any files to test, we've errored out already in
        # prepare_lists_and_print_output().
        assert(len(self._test_files))

        start_time = time.time()

        interrupted, keyboard_interrupted, thread_timings, test_timings, individual_test_timings = self._run_tests(self._test_files_list, result_summary)

        # We exclude the crashes from the list of results to retry, because
        # we want to treat even a potentially flaky crash as an error.
        failures = self._get_failures(result_summary, include_crashes=False, include_missing=False)
        retry_summary = result_summary
        while (len(failures) and self._options.retry_failures and not self._retrying and not interrupted and not keyboard_interrupted):
            _log.info('')
            _log.info("Retrying %d unexpected failure(s) ..." % len(failures))
            _log.info('')
            self._retrying = True
            retry_summary = ResultSummary(self._expectations, failures.keys())
            # Note that we intentionally ignore the return value here.
            self._run_tests(failures.keys(), retry_summary)
            failures = self._get_failures(retry_summary, include_crashes=True, include_missing=True)

        end_time = time.time()

        self._print_timing_statistics(end_time - start_time, thread_timings, test_timings, individual_test_timings, result_summary)
        self._print_result_summary(result_summary)

        sys.stdout.flush()
        sys.stderr.flush()

        self._printer.print_one_line_summary(result_summary.total, result_summary.expected, result_summary.unexpected)

        unexpected_results = summarize_results(self._port, self._expectations, result_summary, retry_summary, individual_test_timings, only_unexpected=True, interrupted=interrupted)
        self._printer.print_unexpected_results(unexpected_results)

        # Re-raise a KeyboardInterrupt if necessary so the caller can handle it.
        if keyboard_interrupted:
            raise KeyboardInterrupt

        # FIXME: remove record_results. It's just used for testing. There's no need
        # for it to be a commandline argument.
        if (self._options.record_results and not self._options.dry_run and not keyboard_interrupted):
            self._port.print_leaks_summary()
            # Write the same data to log files and upload generated JSON files to appengine server.
            summarized_results = summarize_results(self._port, self._expectations, result_summary, retry_summary, individual_test_timings, only_unexpected=False, interrupted=interrupted)
            self._upload_json_files(summarized_results, result_summary, individual_test_timings)

        # Write the summary to disk (results.html) and display it if requested.
        if not self._options.dry_run:
            self._copy_results_html_file()
            if self._options.show_results:
                self._show_results_html_file(result_summary)

        return self._port.exit_code_from_summarized_results(unexpected_results)

    def start_servers_with_lock(self):
        assert(self._options.http)
        self._printer.print_update('Acquiring http lock ...')
        self._port.acquire_http_lock()
        self._printer.print_update('Starting HTTP server ...')
        self._port.start_http_server()
        self._printer.print_update('Starting WebSocket server ...')
        self._port.start_websocket_server()
        self._has_http_lock = True

    def stop_servers_with_lock(self):
        if self._has_http_lock:
            self._printer.print_update('Stopping HTTP server ...')
            self._port.stop_http_server()
            self._printer.print_update('Stopping WebSocket server ...')
            self._port.stop_websocket_server()
            self._printer.print_update('Releasing server lock ...')
            self._port.release_http_lock()
            self._has_http_lock = False

    def clean_up_run(self):
        """Restores the system after we're done running tests."""

        _log.debug("flushing stdout")
        sys.stdout.flush()
        _log.debug("flushing stderr")
        sys.stderr.flush()
        _log.debug("stopping helper")
        self._port.stop_helper()

    def update_summary(self, result_summary):
        """Update the summary and print results with any completed tests."""
        while True:
            try:
                result = test_results.TestResult.loads(self._result_queue.get_nowait())
            except Queue.Empty:
                self._printer.print_progress(result_summary, self._retrying, self._test_files_list)
                return

            self._update_summary_with_result(result_summary, result)

    def _interrupt_if_at_failure_limits(self, result_summary):
        # Note: The messages in this method are constructed to match old-run-webkit-tests
        # so that existing buildbot grep rules work.
        def interrupt_if_at_failure_limit(limit, failure_count, result_summary, message):
            if limit and failure_count >= limit:
                message += " %d tests run." % (result_summary.expected + result_summary.unexpected)
                raise TestRunInterruptedException(message)

        interrupt_if_at_failure_limit(
            self._options.exit_after_n_failures,
            result_summary.unexpected_failures,
            result_summary,
            "Exiting early after %d failures." % result_summary.unexpected_failures)
        interrupt_if_at_failure_limit(
            self._options.exit_after_n_crashes_or_timeouts,
            result_summary.unexpected_crashes + result_summary.unexpected_timeouts,
            result_summary,
            # This differs from ORWT because it does not include WebProcess crashes.
            "Exiting early after %d crashes and %d timeouts." % (result_summary.unexpected_crashes, result_summary.unexpected_timeouts))

    def _update_summary_with_result(self, result_summary, result):
        if result.type == test_expectations.SKIP:
            result_summary.add(result, expected=True)
        else:
            expected = self._expectations.matches_an_expected_result(result.test_name, result.type, self._options.pixel_tests or test_failures.is_reftest_failure(result.failures))
            result_summary.add(result, expected)
            exp_str = self._expectations.get_expectations_string(result.test_name)
            got_str = self._expectations.expectation_to_string(result.type)
            self._printer.print_test_result(result, expected, exp_str, got_str)
        self._printer.print_progress(result_summary, self._retrying, self._test_files_list)
        self._interrupt_if_at_failure_limits(result_summary)

    def _clobber_old_results(self):
        # Just clobber the actual test results directories since the other
        # files in the results directory are explicitly used for cross-run
        # tracking.
        self._printer.print_update("Clobbering old results in %s" %
                                   self._results_directory)
        layout_tests_dir = self._port.layout_tests_dir()
        possible_dirs = self._port.test_dirs()
        for dirname in possible_dirs:
            if self._filesystem.isdir(self._filesystem.join(layout_tests_dir, dirname)):
                self._filesystem.rmtree(self._filesystem.join(self._results_directory, dirname))

    def _get_failures(self, result_summary, include_crashes, include_missing):
        """Filters a dict of results and returns only the failures.

        Args:
          result_summary: the results of the test run
          include_crashes: whether crashes are included in the output.
            We use False when finding the list of failures to retry
            to see if the results were flaky. Although the crashes may also be
            flaky, we treat them as if they aren't so that they're not ignored.
        Returns:
          a dict of files -> results
        """
        failed_results = {}
        for test, result in result_summary.unexpected_results.iteritems():
            if (result.type == test_expectations.PASS or
                (result.type == test_expectations.CRASH and not include_crashes) or
                (result.type == test_expectations.MISSING and not include_missing)):
                continue
            failed_results[test] = result.type

        return failed_results

    def _char_for_result(self, result):
        result = result.lower()
        if result in TestExpectations.EXPECTATIONS:
            result_enum_value = TestExpectations.EXPECTATIONS[result]
        else:
            result_enum_value = TestExpectations.MODIFIERS[result]
        return json_layout_results_generator.JSONLayoutResultsGenerator.FAILURE_TO_CHAR[result_enum_value]

    def _upload_json_files(self, summarized_results, result_summary, individual_test_timings):
        """Writes the results of the test run as JSON files into the results
        dir and upload the files to the appengine server.

        Args:
          unexpected_results: dict of unexpected results
          summarized_results: dict of results
          result_summary: full summary object
          individual_test_timings: list of test times (used by the flakiness
            dashboard).
        """
        _log.debug("Writing JSON files in %s." % self._results_directory)

        times_trie = json_results_generator.test_timings_trie(self._port, individual_test_timings)
        times_json_path = self._filesystem.join(self._results_directory, "times_ms.json")
        json_results_generator.write_json(self._filesystem, times_trie, times_json_path)

        full_results_path = self._filesystem.join(self._results_directory, "full_results.json")
        # We write full_results.json out as jsonp because we need to load it from a file url and Chromium doesn't allow that.
        json_results_generator.write_json(self._filesystem, summarized_results, full_results_path, callback="ADD_RESULTS")

        generator = json_layout_results_generator.JSONLayoutResultsGenerator(
            self._port, self._options.builder_name, self._options.build_name,
            self._options.build_number, self._results_directory,
            BUILDER_BASE_URL, individual_test_timings,
            self._expectations, result_summary, self._test_files_list,
            self._options.test_results_server,
            "layout-tests",
            self._options.master_name)

        _log.debug("Finished writing JSON files.")

        json_files = ["incremental_results.json", "full_results.json", "times_ms.json"]

        generator.upload_json_files(json_files)

        incremental_results_path = self._filesystem.join(self._results_directory, "incremental_results.json")

        # Remove these files from the results directory so they don't take up too much space on the buildbot.
        # The tools use the version we uploaded to the results server anyway.
        self._filesystem.remove(times_json_path)
        self._filesystem.remove(incremental_results_path)

    def print_config(self):
        """Prints the configuration for the test run."""
        p = self._printer
        p.print_config("Using port '%s'" % self._port.name())
        p.print_config("Test configuration: %s" % self._port.test_configuration())
        p.print_config("Placing test results in %s" % self._results_directory)
        if self._options.new_baseline:
            p.print_config("Placing new baselines in %s" %
                           self._port.baseline_path())

        fallback_path = [self._filesystem.split(x)[1] for x in self._port.baseline_search_path()]
        p.print_config("Baseline search path: %s -> generic" % " -> ".join(fallback_path))

        p.print_config("Using %s build" % self._options.configuration)
        if self._options.pixel_tests:
            p.print_config("Pixel tests enabled")
        else:
            p.print_config("Pixel tests disabled")

        p.print_config("Regular timeout: %s, slow test timeout: %s" %
                       (self._options.time_out_ms,
                        self._options.slow_time_out_ms))

        p.print_config('Command line: ' +
                       ' '.join(self._port.driver_cmd_line()))
        p.print_config("Worker model: %s" % self._options.worker_model)
        p.print_config("")

    def _print_expected_results_of_type(self, result_summary,
                                        result_type, result_type_str):
        """Print the number of the tests in a given result class.

        Args:
          result_summary - the object containing all the results to report on
          result_type - the particular result type to report in the summary.
          result_type_str - a string description of the result_type.
        """
        tests = self._expectations.get_tests_with_result_type(result_type)
        now = result_summary.tests_by_timeline[test_expectations.NOW]
        wontfix = result_summary.tests_by_timeline[test_expectations.WONTFIX]

        # We use a fancy format string in order to print the data out in a
        # nicely-aligned table.
        fmtstr = ("Expect: %%5d %%-8s (%%%dd now, %%%dd wontfix)"
                  % (self._num_digits(now), self._num_digits(wontfix)))
        self._printer.print_expected(fmtstr %
            (len(tests), result_type_str, len(tests & now), len(tests & wontfix)))

    def _num_digits(self, num):
        """Returns the number of digits needed to represent the length of a
        sequence."""
        ndigits = 1
        if len(num):
            ndigits = int(math.log10(len(num))) + 1
        return ndigits

    def _print_timing_statistics(self, total_time, thread_timings,
                               directory_test_timings, individual_test_timings,
                               result_summary):
        """Record timing-specific information for the test run.

        Args:
          total_time: total elapsed time (in seconds) for the test run
          thread_timings: wall clock time each thread ran for
          directory_test_timings: timing by directory
          individual_test_timings: timing by file
          result_summary: summary object for the test run
        """
        self._printer.print_timing("Test timing:")
        self._printer.print_timing("  %6.2f total testing time" % total_time)
        self._printer.print_timing("")
        self._printer.print_timing("Thread timing:")
        cuml_time = 0
        for t in thread_timings:
            self._printer.print_timing("    %10s: %5d tests, %6.2f secs" %
                  (t['name'], t['num_tests'], t['total_time']))
            cuml_time += t['total_time']
        self._printer.print_timing("   %6.2f cumulative, %6.2f optimal" %
              (cuml_time, cuml_time / int(self._options.child_processes)))
        self._printer.print_timing("")

        self._print_aggregate_test_statistics(individual_test_timings)
        self._print_individual_test_times(individual_test_timings,
                                          result_summary)
        self._print_directory_timings(directory_test_timings)

    def _print_aggregate_test_statistics(self, individual_test_timings):
        """Prints aggregate statistics (e.g. median, mean, etc.) for all tests.
        Args:
          individual_test_timings: List of TestResults for all tests.
        """
        times_for_dump_render_tree = [test_stats.test_run_time for test_stats in individual_test_timings]
        self._print_statistics_for_test_timings("PER TEST TIME IN TESTSHELL (seconds):",
                                                times_for_dump_render_tree)

    def _print_individual_test_times(self, individual_test_timings,
                                  result_summary):
        """Prints the run times for slow, timeout and crash tests.
        Args:
          individual_test_timings: List of TestStats for all tests.
          result_summary: summary object for test run
        """
        # Reverse-sort by the time spent in DumpRenderTree.
        individual_test_timings.sort(lambda a, b:
            cmp(b.test_run_time, a.test_run_time))

        num_printed = 0
        slow_tests = []
        timeout_or_crash_tests = []
        unexpected_slow_tests = []
        for test_tuple in individual_test_timings:
            test_name = test_tuple.test_name
            is_timeout_crash_or_slow = False
            if self._test_is_slow(test_name):
                is_timeout_crash_or_slow = True
                slow_tests.append(test_tuple)

            if test_name in result_summary.failures:
                result = result_summary.results[test_name].type
                if (result == test_expectations.TIMEOUT or
                    result == test_expectations.CRASH):
                    is_timeout_crash_or_slow = True
                    timeout_or_crash_tests.append(test_tuple)

            if (not is_timeout_crash_or_slow and
                num_printed < printing.NUM_SLOW_TESTS_TO_LOG):
                num_printed = num_printed + 1
                unexpected_slow_tests.append(test_tuple)

        self._printer.print_timing("")
        self._print_test_list_timing("%s slowest tests that are not "
            "marked as SLOW and did not timeout/crash:" %
            printing.NUM_SLOW_TESTS_TO_LOG, unexpected_slow_tests)
        self._printer.print_timing("")
        self._print_test_list_timing("Tests marked as SLOW:", slow_tests)
        self._printer.print_timing("")
        self._print_test_list_timing("Tests that timed out or crashed:",
                                     timeout_or_crash_tests)
        self._printer.print_timing("")

    def _print_test_list_timing(self, title, test_list):
        """Print timing info for each test.

        Args:
          title: section heading
          test_list: tests that fall in this section
        """
        if self._printer.disabled('slowest'):
            return

        self._printer.print_timing(title)
        for test_tuple in test_list:
            test_run_time = round(test_tuple.test_run_time, 1)
            self._printer.print_timing("  %s took %s seconds" % (test_tuple.test_name, test_run_time))

    def _print_directory_timings(self, directory_test_timings):
        """Print timing info by directory for any directories that
        take > 10 seconds to run.

        Args:
          directory_test_timing: time info for each directory
        """
        timings = []
        for directory in directory_test_timings:
            num_tests, time_for_directory = directory_test_timings[directory]
            timings.append((round(time_for_directory, 1), directory,
                            num_tests))
        timings.sort()

        self._printer.print_timing("Time to process slowest subdirectories:")
        min_seconds_to_print = 10
        for timing in timings:
            if timing[0] > min_seconds_to_print:
                self._printer.print_timing(
                    "  %s took %s seconds to run %s tests." % (timing[1],
                    timing[0], timing[2]))
        self._printer.print_timing("")

    def _print_statistics_for_test_timings(self, title, timings):
        """Prints the median, mean and standard deviation of the values in
        timings.

        Args:
          title: Title for these timings.
          timings: A list of floats representing times.
        """
        self._printer.print_timing(title)
        timings.sort()

        num_tests = len(timings)
        if not num_tests:
            return
        percentile90 = timings[int(.9 * num_tests)]
        percentile99 = timings[int(.99 * num_tests)]

        if num_tests % 2 == 1:
            median = timings[((num_tests - 1) / 2) - 1]
        else:
            lower = timings[num_tests / 2 - 1]
            upper = timings[num_tests / 2]
            median = (float(lower + upper)) / 2

        mean = sum(timings) / num_tests

        for timing in timings:
            sum_of_deviations = math.pow(timing - mean, 2)

        std_deviation = math.sqrt(sum_of_deviations / num_tests)
        self._printer.print_timing("  Median:          %6.3f" % median)
        self._printer.print_timing("  Mean:            %6.3f" % mean)
        self._printer.print_timing("  90th percentile: %6.3f" % percentile90)
        self._printer.print_timing("  99th percentile: %6.3f" % percentile99)
        self._printer.print_timing("  Standard dev:    %6.3f" % std_deviation)
        self._printer.print_timing("")

    def _print_result_summary(self, result_summary):
        """Print a short summary about how many tests passed.

        Args:
          result_summary: information to log
        """
        failed = result_summary.total_failures
        skipped = result_summary.total_tests_by_expectation[test_expectations.SKIP]
        total = result_summary.total
        passed = total - failed - skipped
        pct_passed = 0.0
        if total > 0:
            pct_passed = float(passed) * 100 / total

        self._printer.print_actual("")
        self._printer.print_actual("=> Results: %d/%d tests passed (%.1f%%)" %
                     (passed, total, pct_passed))
        self._printer.print_actual("")
        self._print_result_summary_entry(result_summary,
            test_expectations.NOW, "Tests to be fixed")

        self._printer.print_actual("")
        self._print_result_summary_entry(result_summary,
            test_expectations.WONTFIX,
            "Tests that will only be fixed if they crash (WONTFIX)")
        self._printer.print_actual("")

    def _print_result_summary_entry(self, result_summary, timeline,
                                    heading):
        """Print a summary block of results for a particular timeline of test.

        Args:
          result_summary: summary to print results for
          timeline: the timeline to print results for (NOT, WONTFIX, etc.)
          heading: a textual description of the timeline
        """
        total = len(result_summary.tests_by_timeline[timeline])
        not_passing = (total -
           len(result_summary.tests_by_expectation[test_expectations.PASS] &
               result_summary.tests_by_timeline[timeline]))
        self._printer.print_actual("=> %s (%d):" % (heading, not_passing))

        for result in TestExpectations.EXPECTATION_ORDER:
            if result == test_expectations.PASS:
                continue
            results = (result_summary.tests_by_expectation[result] &
                       result_summary.tests_by_timeline[timeline])
            desc = TestExpectations.EXPECTATION_DESCRIPTIONS[result]
            if not_passing and len(results):
                pct = len(results) * 100.0 / not_passing
                self._printer.print_actual("  %5d %-24s (%4.1f%%)" %
                    (len(results), desc[len(results) != 1], pct))

    def _copy_results_html_file(self):
        base_dir = self._port.path_from_webkit_base('LayoutTests', 'fast', 'harness')
        results_file = self._filesystem.join(base_dir, 'results.html')
        # FIXME: What should we do if this doesn't exist (e.g., in unit tests)?
        if self._filesystem.exists(results_file):
            self._filesystem.copyfile(results_file, self._filesystem.join(self._results_directory, "results.html"))

    def _show_results_html_file(self, result_summary):
        """Shows the results.html page."""
        if self._options.full_results_html:
            test_files = result_summary.failures.keys()
        else:
            unexpected_failures = self._get_failures(result_summary, include_crashes=True, include_missing=True)
            test_files = unexpected_failures.keys()

        if not len(test_files):
            return

        results_filename = self._filesystem.join(self._results_directory, "results.html")
        self._port.show_results_html_file(results_filename)

    def name(self):
        return 'Manager'

    def is_done(self):
        worker_states = self._worker_states.values()
        return worker_states and all(self._worker_is_done(worker_state) for worker_state in worker_states)

    # FIXME: Inline this function.
    def _worker_is_done(self, worker_state):
        return worker_state.done

    def cancel_workers(self):
        for worker_state in self._worker_states.values():
            worker_state.worker_connection.cancel()

    def handle_started_test(self, source, test_info, hang_timeout):
        worker_state = self._worker_states[source]
        worker_state.current_test_name = test_info.test_name
        worker_state.next_timeout = time.time() + hang_timeout

    def handle_done(self, source):
        worker_state = self._worker_states[source]
        worker_state.done = True

    def handle_exception(self, source, exception_type, exception_value, stack):
        if exception_type in (KeyboardInterrupt, TestRunInterruptedException):
            raise exception_type(exception_value)
        _log.error("%s raised %s('%s'):" % (
                   source,
                   exception_value.__class__.__name__,
                   str(exception_value)))
        self._log_worker_stack(stack)
        raise WorkerException(str(exception_value))

    def handle_finished_list(self, source, list_name, num_tests, elapsed_time):
        self._group_stats[list_name] = (num_tests, elapsed_time)

        def find(name, test_lists):
            for i in range(len(test_lists)):
                if test_lists[i].name == name:
                    return i
            return -1

        index = find(list_name, self._remaining_locked_shards)
        if index >= 0:
            self._remaining_locked_shards.pop(index)
            if not self._remaining_locked_shards:
                self.stop_servers_with_lock()

    def handle_finished_test(self, source, result, elapsed_time):
        worker_state = self._worker_states[source]
        worker_state.next_timeout = None
        worker_state.current_test_name = None
        worker_state.stats['total_time'] += elapsed_time
        worker_state.stats['num_tests'] += 1

        self._all_results.append(result)
        self._update_summary_with_result(self._current_result_summary, result)

    def _log_worker_stack(self, stack):
        webkitpydir = self._port.path_from_webkit_base('Tools', 'Scripts', 'webkitpy') + self._filesystem.sep
        for filename, line_number, function_name, text in stack:
            if filename.startswith(webkitpydir):
                filename = filename.replace(webkitpydir, '')
            _log.error('  %s:%u (in %s)' % (filename, line_number, function_name))
            _log.error('    %s' % text)


def read_test_files(fs, filenames, test_path_separator):
    tests = []
    for filename in filenames:
        try:
            if test_path_separator != fs.sep:
                filename = filename.replace(test_path_separator, fs.sep)
            file_contents = fs.read_text_file(filename).split('\n')
            for line in file_contents:
                line = test_expectations.strip_comments(line)
                if line:
                    tests.append(line)
        except IOError, e:
            if e.errno == errno.ENOENT:
                _log.critical('')
                _log.critical('--test-list file "%s" not found' % file)
            raise
    return tests


# FIXME: These two free functions belong either on manager (since it's the only one
# which uses them) or in a different file (if they need to be re-used).
def test_key(port, test_name):
    """Turns a test name into a list with two sublists, the natural key of the
    dirname, and the natural key of the basename.

    This can be used when sorting paths so that files in a directory.
    directory are kept together rather than being mixed in with files in
    subdirectories."""
    dirname, basename = port.split_test(test_name)
    return (natural_sort_key(dirname + port.TEST_PATH_SEPARATOR), natural_sort_key(basename))


def natural_sort_key(string_to_split):
    """ Turn a string into a list of string and number chunks.
        "z23a" -> ["z", 23, "a"]

        Can be used to implement "natural sort" order. See:
            http://www.codinghorror.com/blog/2007/12/sorting-for-humans-natural-sort-order.html
            http://nedbatchelder.com/blog/200712.html#e20071211T054956
    """
    def tryint(val):
        try:
            return int(val)
        except ValueError:
            return val

    return [tryint(chunk) for chunk in re.split('(\d+)', string_to_split)]


class _WorkerState(object):
    """A class for the manager to use to track the current state of the workers."""
    def __init__(self, number, worker_connection):
        self.worker_connection = worker_connection
        self.number = number
        self.done = False
        self.current_test_name = None
        self.next_timeout = None
        self.stats = {}
        self.stats['name'] = worker_connection.name
        self.stats['num_tests'] = 0
        self.stats['total_time'] = 0

    def __repr__(self):
        return "_WorkerState(" + str(self.__dict__) + ")"
