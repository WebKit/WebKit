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

import json
import logging
import random
import sys
import time

from webkitpy.layout_tests.controllers.layout_test_finder import LayoutTestFinder
from webkitpy.layout_tests.controllers.layout_test_runner import LayoutTestRunner
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.layout_package import json_layout_results_generator
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models.test_input import TestInput

_log = logging.getLogger(__name__)

# Builder base URL where we have the archived test results.
BUILDER_BASE_URL = "http://build.chromium.org/buildbot/layout_test_results/"

TestExpectations = test_expectations.TestExpectations


class RunDetails(object):
    def __init__(self, exit_code, summarized_results=None, initial_results=None, retry_results=None):
        self.exit_code = exit_code
        self.summarized_results = summarized_results
        self.initial_results = initial_results
        self.retry_results = retry_results


def interpret_test_failures(failures):
    test_dict = {}
    failure_types = [type(failure) for failure in failures]
    # FIXME: get rid of all this is_* values once there is a 1:1 map between
    # TestFailure type and test_expectations.EXPECTATION.
    if test_failures.FailureMissingAudio in failure_types:
        test_dict['is_missing_audio'] = True

    if test_failures.FailureMissingResult in failure_types:
        test_dict['is_missing_text'] = True

    if test_failures.FailureMissingImage in failure_types or test_failures.FailureMissingImageHash in failure_types:
        test_dict['is_missing_image'] = True

    for failure in failures:
        if isinstance(failure, test_failures.FailureImageHashMismatch) or isinstance(failure, test_failures.FailureReftestMismatch):
            test_dict['image_diff_percent'] = failure.diff_percent

    return test_dict


def use_trac_links_in_results_html(port_obj):
    # We only use trac links on the buildbots.
    # Use existence of builder_name as a proxy for knowing we're on a bot.
    return port_obj.get_option("builder_name")


# FIXME: This should be on the Manager class (since that's the only caller)
# or split off from Manager onto another helper class, but should not be a free function.
# Most likely this should be made into its own class, and this super-long function
# split into many helper functions.
def summarize_results(port_obj, expectations, initial_results, retry_results):
    """Returns a dictionary containing a summary of the test runs, with the following fields:
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

    tbe = initial_results.tests_by_expectation
    tbt = initial_results.tests_by_timeline
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

    for test_name, result in initial_results.results_by_name.iteritems():
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

        if result.reftest_type:
            test_dict.update(reftest_type=list(result.reftest_type))

        if expectations.has_modifier(test_name, test_expectations.WONTFIX):
            test_dict['wontfix'] = True

        if result_type == test_expectations.PASS:
            num_passes += 1
            # FIXME: include passing tests that have stderr output.
            if expected == 'PASS':
                continue
        elif result_type == test_expectations.CRASH:
            if test_name in initial_results.unexpected_results_by_name:
                num_regressions += 1
        elif result_type == test_expectations.MISSING:
            if test_name in initial_results.unexpected_results_by_name:
                num_missing += 1
        elif test_name in initial_results.unexpected_results_by_name:
            if retry_results and test_name not in retry_results.unexpected_results_by_name:
                actual.extend(expectations.get_expectations_string(test_name).split(" "))
                num_flaky += 1
            elif retry_results:
                retry_result_type = retry_results.unexpected_results_by_name[test_name].type
                if result_type != retry_result_type:
                    actual.append(keywords[retry_result_type])
                    num_flaky += 1
                else:
                    num_regressions += 1
            else:
                num_regressions += 1

        test_dict['expected'] = expected
        test_dict['actual'] = " ".join(actual)

        test_dict.update(interpret_test_failures(result.failures))

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
    results['interrupted'] = initial_results.interrupted  # Does results.html have enough information to compute this itself? (by checking total number of results vs. total number of tests?)
    results['layout_tests_dir'] = port_obj.layout_tests_dir()
    results['has_wdiff'] = port_obj.wdiff_available()
    results['has_pretty_patch'] = port_obj.pretty_patch_available()
    results['pixel_tests_enabled'] = port_obj.get_option('pixel_tests')

    try:
        # We only use the svn revision for using trac links in the results.html file,
        # Don't do this by default since it takes >100ms.
        # FIXME: Do we really need to populate this both here and in the json_results_generator?
        if use_trac_links_in_results_html(port_obj):
            port_obj.host.initialize_scm()
            results['revision'] = port_obj.host.scm().head_svn_revision()
    except Exception, e:
        _log.warn("Failed to determine svn revision for checkout (cwd: %s, webkit_base: %s), leaving 'revision' key blank in full_results.json.\n%s" % (port_obj._filesystem.getcwd(), port_obj.path_from_webkit_base(), e))
        # Handle cases where we're running outside of version control.
        import traceback
        _log.debug('Failed to learn head svn revision:')
        _log.debug(traceback.format_exc())
        results['revision'] = ""

    return results


class Manager(object):
    """A class for managing running a series of tests on a series of layout
    test files."""

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
        self._expectations = None

        self.HTTP_SUBDIR = 'http' + port.TEST_PATH_SEPARATOR
        self.PERF_SUBDIR = 'perf'
        self.WEBSOCKET_SUBDIR = 'websocket' + port.TEST_PATH_SEPARATOR
        self.LAYOUT_TESTS_DIRECTORY = 'LayoutTests'

        # disable wss server. need to install pyOpenSSL on buildbots.
        # self._websocket_secure_server = websocket_server.PyWebSocket(
        #        options.results_directory, use_tls=True, port=9323)

        self._results_directory = self._port.results_directory()
        self._finder = LayoutTestFinder(self._port, self._options)
        self._runner = LayoutTestRunner(self._options, self._port, self._printer, self._results_directory, self._test_is_slow)

    def _collect_tests(self, args):
        return self._finder.find_tests(self._options, args)

    def _is_http_test(self, test):
        return self.HTTP_SUBDIR in test or self._is_websocket_test(test)

    def _is_websocket_test(self, test):
        return self.WEBSOCKET_SUBDIR in test

    def _http_tests(self, test_names):
        return set(test for test in test_names if self._is_http_test(test))

    def _is_perf_test(self, test):
        return self.PERF_SUBDIR == test or (self.PERF_SUBDIR + self._port.TEST_PATH_SEPARATOR) in test

    def _prepare_lists(self, paths, test_names):
        tests_to_skip = self._finder.skip_tests(paths, test_names, self._expectations, self._http_tests(test_names))
        tests_to_run = [test for test in test_names if test not in tests_to_skip]

        # Create a sorted list of test files so the subset chunk,
        # if used, contains alphabetically consecutive tests.
        if self._options.order == 'natural':
            tests_to_run.sort(key=self._port.test_key)
        elif self._options.order == 'random':
            random.shuffle(tests_to_run)

        tests_to_run, tests_in_other_chunks = self._finder.split_into_chunks(tests_to_run)
        self._expectations.add_skipped_tests(tests_in_other_chunks)
        tests_to_skip.update(tests_in_other_chunks)

        return tests_to_run, tests_to_skip

    def _test_input_for_file(self, test_file):
        return TestInput(test_file,
            self._options.slow_time_out_ms if self._test_is_slow(test_file) else self._options.time_out_ms,
            self._test_requires_lock(test_file))

    def _test_requires_lock(self, test_file):
        """Return True if the test needs to be locked when
        running multiple copies of NRWTs. Perf tests are locked
        because heavy load caused by running other tests in parallel
        might cause some of them to timeout."""
        return self._is_http_test(test_file) or self._is_perf_test(test_file)

    def _test_is_slow(self, test_file):
        return self._expectations.has_modifier(test_file, test_expectations.SLOW)

    def needs_servers(self, test_names):
        return any(self._test_requires_lock(test_name) for test_name in test_names) and self._options.http

    def _set_up_run(self, test_names):
        self._printer.write_update("Checking build ...")
        if not self._port.check_build(self.needs_servers(test_names)):
            _log.error("Build check failed")
            return False

        # This must be started before we check the system dependencies,
        # since the helper may do things to make the setup correct.
        if self._options.pixel_tests:
            self._printer.write_update("Starting pixel test helper ...")
            self._port.start_helper()

        # Check that the system dependencies (themes, fonts, ...) are correct.
        if not self._options.nocheck_sys_deps:
            self._printer.write_update("Checking system dependencies ...")
            if not self._port.check_sys_deps(self.needs_servers(test_names)):
                self._port.stop_helper()
                return False

        if self._options.clobber_old_results:
            self._clobber_old_results()

        # Create the output directory if it doesn't already exist.
        self._port.host.filesystem.maybe_make_directory(self._results_directory)

        self._port.setup_test_run()
        return True

    def run(self, args):
        """Run the tests and return a RunDetails object with the results."""
        self._printer.write_update("Collecting tests ...")
        try:
            paths, test_names = self._collect_tests(args)
        except IOError:
            # This is raised if --test-list doesn't exist
            return RunDetails(exit_code=-1)

        self._printer.write_update("Parsing expectations ...")
        self._expectations = test_expectations.TestExpectations(self._port, test_names)

        tests_to_run, tests_to_skip = self._prepare_lists(paths, test_names)
        self._printer.print_found(len(test_names), len(tests_to_run), self._options.repeat_each, self._options.iterations)

        # Check to make sure we're not skipping every test.
        if not tests_to_run:
            _log.critical('No tests to run.')
            return RunDetails(exit_code=-1)

        if not self._set_up_run(tests_to_run):
            return RunDetails(exit_code=-1)

        start_time = time.time()
        try:
            initial_results = self._run_tests(tests_to_run, tests_to_skip, self._options.repeat_each, self._options.iterations,
                int(self._options.child_processes), retrying=False)

            tests_to_retry = self._tests_to_retry(initial_results, include_crashes=self._port.should_retry_crashes())
            if self._options.retry_failures and tests_to_retry and not initial_results.interrupted:
                _log.info('')
                _log.info("Retrying %d unexpected failure(s) ..." % len(tests_to_retry))
                _log.info('')
                retry_results = self._run_tests(tests_to_retry, tests_to_skip=set(), repeat_each=1, iterations=1,
                    num_workers=1, retrying=True)
            else:
                retry_results = None
        finally:
            self._clean_up_run()

        end_time = time.time()

        # Some crash logs can take a long time to be written out so look
        # for new logs after the test run finishes.
        self._look_for_new_crash_logs(initial_results, start_time)
        if retry_results:
            self._look_for_new_crash_logs(retry_results, start_time)

        summarized_results = summarize_results(self._port, self._expectations, initial_results, retry_results)
        self._printer.print_results(end_time - start_time, initial_results, summarized_results)

        if not self._options.dry_run:
            self._port.print_leaks_summary()
            self._upload_json_files(summarized_results, initial_results)

            results_path = self._filesystem.join(self._results_directory, "results.html")
            self._copy_results_html_file(results_path)
            if self._options.show_results and (initial_results.unexpected_results_by_name or
                                               (self._options.full_results_html and initial_results.total_failures)):
                self._port.show_results_html_file(results_path)

        return RunDetails(self._port.exit_code_from_summarized_results(summarized_results),
                          summarized_results, initial_results, retry_results)

    def _run_tests(self, tests_to_run, tests_to_skip, repeat_each, iterations, num_workers, retrying):
        needs_http = self._port.requires_http_server() or any(self._is_http_test(test) for test in tests_to_run)
        needs_websockets = any(self._is_websocket_test(test) for test in tests_to_run)

        test_inputs = []
        for _ in xrange(iterations):
            for test in tests_to_run:
                for _ in xrange(repeat_each):
                    test_inputs.append(self._test_input_for_file(test))

        return self._runner.run_tests(self._expectations, test_inputs, tests_to_skip, num_workers, needs_http, needs_websockets, retrying)

    def _clean_up_run(self):
        """Restores the system after we're done running tests."""
        _log.debug("flushing stdout")
        sys.stdout.flush()
        _log.debug("flushing stderr")
        sys.stderr.flush()
        _log.debug("stopping helper")
        self._port.stop_helper()
        _log.debug("cleaning up port")
        self._port.clean_up_test_run()

    def _look_for_new_crash_logs(self, run_results, start_time):
        """Since crash logs can take a long time to be written out if the system is
           under stress do a second pass at the end of the test run.

           run_results: the results of the test run
           start_time: time the tests started at.  We're looking for crash
               logs after that time.
        """
        crashed_processes = []
        for test, result in run_results.unexpected_results_by_name.iteritems():
            if (result.type != test_expectations.CRASH):
                continue
            for failure in result.failures:
                if not isinstance(failure, test_failures.FailureCrash):
                    continue
                crashed_processes.append([test, failure.process_name, failure.pid])

        crash_logs = self._port.look_for_new_crash_logs(crashed_processes, start_time)
        if crash_logs:
            for test, crash_log in crash_logs.iteritems():
                writer = TestResultWriter(self._port._filesystem, self._port, self._port.results_directory(), test)
                writer.write_crash_log(crash_log)

    def _clobber_old_results(self):
        # Just clobber the actual test results directories since the other
        # files in the results directory are explicitly used for cross-run
        # tracking.
        self._printer.write_update("Clobbering old results in %s" %
                                   self._results_directory)
        layout_tests_dir = self._port.layout_tests_dir()
        possible_dirs = self._port.test_dirs()
        for dirname in possible_dirs:
            if self._filesystem.isdir(self._filesystem.join(layout_tests_dir, dirname)):
                self._filesystem.rmtree(self._filesystem.join(self._results_directory, dirname))

    def _tests_to_retry(self, run_results, include_crashes):
        return [result.test_name for result in run_results.unexpected_results_by_name.values() if
                   ((result.type != test_expectations.PASS) and
                    (result.type != test_expectations.MISSING) and
                    (result.type != test_expectations.CRASH or include_crashes))]

    def _upload_json_files(self, summarized_results, initial_results):
        """Writes the results of the test run as JSON files into the results
        dir and upload the files to the appengine server.

        Args:
          summarized_results: dict of results
          initial_results: full summary object
        """
        _log.debug("Writing JSON files in %s." % self._results_directory)

        # FIXME: Upload stats.json to the server and delete times_ms.
        times_trie = json_results_generator.test_timings_trie(self._port, initial_results.results_by_name.values())
        times_json_path = self._filesystem.join(self._results_directory, "times_ms.json")
        json_results_generator.write_json(self._filesystem, times_trie, times_json_path)

        stats_trie = self._stats_trie(initial_results)
        stats_path = self._filesystem.join(self._results_directory, "stats.json")
        self._filesystem.write_text_file(stats_path, json.dumps(stats_trie))

        full_results_path = self._filesystem.join(self._results_directory, "full_results.json")
        # We write full_results.json out as jsonp because we need to load it from a file url and Chromium doesn't allow that.
        json_results_generator.write_json(self._filesystem, summarized_results, full_results_path, callback="ADD_RESULTS")

        generator = json_layout_results_generator.JSONLayoutResultsGenerator(
            self._port, self._options.builder_name, self._options.build_name,
            self._options.build_number, self._results_directory,
            BUILDER_BASE_URL,
            self._expectations, initial_results,
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

    def _copy_results_html_file(self, destination_path):
        base_dir = self._port.path_from_webkit_base('LayoutTests', 'fast', 'harness')
        results_file = self._filesystem.join(base_dir, 'results.html')
        # Note that the results.html template file won't exist when we're using a MockFileSystem during unit tests,
        # so make sure it exists before we try to copy it.
        if self._filesystem.exists(results_file):
            self._filesystem.copyfile(results_file, destination_path)

    def _stats_trie(self, initial_results):
        def _worker_number(worker_name):
            return int(worker_name.split('/')[1]) if worker_name else -1

        stats = {}
        for result in initial_results.results_by_name.values():
            if result.type != test_expectations.SKIP:
                stats[result.test_name] = {'results': (_worker_number(result.worker_name), result.test_number, result.pid, int(result.test_run_time * 1000), int(result.total_run_time * 1000))}
        stats_trie = {}
        for name, value in stats.iteritems():
            json_results_generator.add_path_to_trie(name, value, stats_trie)
        return stats_trie
