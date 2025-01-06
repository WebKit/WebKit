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

import datetime
import logging
import signal

from webkitpy.common.iteration_compatibility import iteritems
from webkitpy.layout_tests.models import test_expectations, test_failures

_log = logging.getLogger(__name__)

INTERRUPTED_EXIT_STATUS = signal.SIGINT + 128


class TestRunResults(object):
    def __init__(self, expectations, num_tests):
        self.total = num_tests
        self.remaining = self.total
        self.expectations = expectations
        self.expected = 0
        self.unexpected = 0
        self.unexpected_failures = 0
        self.unexpected_crashes = 0
        self.unexpected_timeouts = 0
        self.tests_by_expectation = {}
        self.tests_by_timeline = {}
        self.results_by_name = {}  # Map of test name to the last result for the test.
        self.all_results = []  # All results from a run, including every iteration of every test.
        self.expected_results_by_name = {}
        self.unexpected_results_by_name = {}
        self.failures_by_name = {}
        self.repeated_results_by_name = {}  # Map of test name to a list of results, when a tests is run more than once (like when passing --repeat-each)
        self.total_failures = 0
        self.expected_skips = 0
        for expectation in test_expectations.TestExpectations.EXPECTATIONS.values():
            self.tests_by_expectation[expectation] = set()
        for timeline in test_expectations.TestExpectations.TIMELINES.values():
            self.tests_by_timeline[timeline] = expectations.model().get_tests_with_timeline(timeline)
        self.slow_tests = set()
        self.interrupted = False
        self.keyboard_interrupted = False

    def add(self, test_result, expected):
        self.tests_by_expectation[test_result.type].add(test_result.test_name)
        if test_result.test_name not in self.repeated_results_by_name:
            self.repeated_results_by_name[test_result.test_name] = set()
        self.repeated_results_by_name[test_result.test_name].add(test_result.type)
        self.results_by_name[test_result.test_name] = self.results_by_name.get(test_result.test_name, test_result)
        if test_result.is_other_crash:
            return
        if test_result.type != test_expectations.SKIP:
            self.all_results.append(test_result)
        self.remaining -= 1
        if len(test_result.failures):
            self.total_failures += 1
            self.failures_by_name[test_result.test_name] = test_result.failures
        if expected:
            self.expected_results_by_name[test_result.test_name] = test_result
            self.expected += 1
            if test_result.type == test_expectations.SKIP:
                self.expected_skips += 1
        else:
            self.unexpected_results_by_name[test_result.test_name] = test_result
            self.unexpected += 1
            if len(test_result.failures):
                self.unexpected_failures += 1
            if test_result.type == test_expectations.CRASH:
                self.unexpected_crashes += 1
            elif test_result.type == test_expectations.TIMEOUT:
                self.unexpected_timeouts += 1
        if test_result.test_input.is_slow:
            self.slow_tests.add(test_result.test_name)

    def change_result_to_failure(self, existing_result, new_result, existing_expected, new_expected):
        assert existing_result.test_name == new_result.test_name
        if existing_result.type is new_result.type:
            return

        self.tests_by_expectation[existing_result.type].discard(existing_result.test_name)
        self.tests_by_expectation[new_result.type].add(new_result.test_name)

        had_failures = len(existing_result.failures) > 0

        existing_result.convert_to_failure(new_result)

        if not had_failures and len(existing_result.failures):
            self.total_failures += 1

        if len(existing_result.failures):
            self.failures_by_name[existing_result.test_name] = existing_result.failures

        if not existing_expected and new_expected:
            # test changed from unexpected to expected
            self.expected += 1
            self.unexpected_results_by_name.pop(existing_result.test_name, None)
            self.unexpected -= 1
            if had_failures:
                self.unexpected_failures -= 1
        elif existing_expected and not new_expected:
            # test changed from expected to unexpected
            self.expected -= 1
            self.unexpected_results_by_name[existing_result.test_name] = existing_result
            self.unexpected += 1
            if len(existing_result.failures):
                self.unexpected_failures += 1

    def merge(self, test_run_results):
        if not test_run_results:
            return self

        # self.expectations should be the same for both
        # FIXME: this isn't actually true when we run on multiple device_types
        # if self.expectations != test_run_results.expectations:
        #     raise ValueError("different TestExpectations")

        def merge_dict_sets(a, b):
            merged = {}
            keys = set(a.keys()) | set(b.keys())
            for k in keys:
                v_a = a.get(k, set())
                assert isinstance(v_a, set)
                v_b = b.get(k, set())
                assert isinstance(v_b, set)
                merged[k] = v_a | v_b
            return merged

        self.total += test_run_results.total
        self.remaining += test_run_results.remaining
        self.expected += test_run_results.expected
        self.unexpected += test_run_results.unexpected
        self.unexpected_failures += test_run_results.unexpected_failures
        self.unexpected_crashes += test_run_results.unexpected_crashes
        self.unexpected_timeouts += test_run_results.unexpected_timeouts
        self.tests_by_expectation = merge_dict_sets(self.tests_by_expectation, test_run_results.tests_by_expectation)
        self.tests_by_timeline = merge_dict_sets(self.tests_by_timeline, test_run_results.tests_by_timeline)
        self.repeated_results_by_name = merge_dict_sets(self.repeated_results_by_name, test_run_results.repeated_results_by_name)
        self.results_by_name.update(test_run_results.results_by_name)
        self.all_results += test_run_results.all_results
        self.expected_results_by_name.update(test_run_results.expected_results_by_name)
        self.unexpected_results_by_name.update(test_run_results.unexpected_results_by_name)
        self.failures_by_name.update(test_run_results.failures_by_name)
        self.total_failures += test_run_results.total_failures
        self.expected_skips += test_run_results.expected_skips
        self.slow_tests.update(test_run_results.slow_tests)

        self.interrupted |= test_run_results.interrupted
        self.keyboard_interrupted |= test_run_results.keyboard_interrupted
        return self


class RunDetails(object):
    def __init__(self, exit_code, summarized_results=None, initial_results=None, retry_results=None, enabled_pixel_tests_in_retry=False, skipped_all_tests=False):
        self.exit_code = exit_code
        self.summarized_results = summarized_results
        self.initial_results = initial_results
        self.retry_results = retry_results
        self.enabled_pixel_tests_in_retry = enabled_pixel_tests_in_retry
        self.skipped_all_tests = skipped_all_tests


def _interpret_test_failures(failures):
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

    if test_failures.FailureDocumentLeak in failure_types:
        leaks = []
        for failure in failures:
            if isinstance(failure, test_failures.FailureDocumentLeak):
                for url in failure.leaked_document_urls:
                    leaks.append({"document": url})
        test_dict['leaks'] = leaks

    if 'image_diff_percent' not in test_dict:
        for failure in failures:
            if isinstance(failure, test_failures.FailureImageHashMismatch) or isinstance(failure, test_failures.FailureReftestMismatch):
                test_dict['image_diff_percent'] = failure.image_diff_result.diff_percent

    if 'image_difference' not in test_dict:
        for failure in failures:
            if (isinstance(failure, test_failures.FailureImageHashMismatch) or isinstance(failure, test_failures.FailureReftestMismatch)) and failure.image_diff_result.fuzzy_data:
                test_dict['image_difference'] = failure.image_diff_result.fuzzy_data

    return test_dict


# These results must match ones in print_unexpected_results() in views/buildbot_results.py.
def summarize_results(port_obj, expectations_by_type, initial_results, retry_results, enabled_pixel_tests_in_retry, include_passes=False, include_time_and_modifiers=False):
    """Returns a dictionary containing a summary of the test runs, with the following fields:
        'version': a version indicator
        'fixable': The number of fixable tests (NOW - PASS)
        'skipped': The number of skipped tests (NOW & SKIPPED)
        'num_regressions': The number of non-flaky failures
        'num_flaky': The number of flaky failures
        'num_missing': The number of tests with missing results
        'num_passes': The number of unexpected passes
        'tests': a dict of tests -> {'expected': '...', 'actual': '...'}
        'date': the current date and time
    """
    results = {}
    results['version'] = 4

    device_type_list = list(expectations_by_type.keys())

    tbe = initial_results.tests_by_expectation
    tbt = initial_results.tests_by_timeline
    results['fixable'] = len(tbt[test_expectations.NOW] - tbe[test_expectations.PASS])
    results['skipped'] = len(tbt[test_expectations.NOW] & tbe[test_expectations.SKIP])

    num_passes = 0
    num_flaky = 0
    num_missing = 0
    num_regressions = 0
    keywords = {}
    for expectation_string, expectation_enum in test_expectations.TestExpectations.EXPECTATIONS.items():
        keywords[expectation_enum] = expectation_string.upper()

    for modifier_string, modifier_enum in test_expectations.TestExpectations.MODIFIERS.items():
        keywords[modifier_enum] = modifier_string.upper()

    tests = {}
    other_crashes_dict = {}

    for test_name, result in iteritems(initial_results.results_by_name):
        # Note that if a test crashed in the original run, we ignore
        # whether or not it crashed when we retried it (if we retried it),
        # and always consider the result not flaky.
        pixel_tests_enabled = enabled_pixel_tests_in_retry or port_obj._options.pixel_tests or bool(result.reftest_type)

        # We're basically trying to find the first non-skip expectation, and use that expectation object for the remainder of the loop.
        # This works because tests are run on the first device type which won't skip them, regardless of other expectations, and never re-run.
        expected = 'SKIP'
        expectations = list(expectations_by_type.values())[0]
        for element in expectations_by_type.values():
            if element.model().has_modifier(test_name, test_expectations.SKIP):
                continue

            test_expectation = element.filtered_expectations_for_test(test_name, pixel_tests_enabled, port_obj._options.world_leaks)
            expected = element.model().expectations_to_string(test_expectation)
            expectations = element
            continue

        result_type = result.type
        actual = [keywords[result_type]]

        if result_type == test_expectations.SKIP:
            continue

        if result.is_other_crash:
            other_crashes_dict[test_name] = {}
            continue

        test_dict = {}
        if result.has_stderr:
            test_dict['has_stderr'] = True

        if result.reftest_type:
            test_dict.update(reftest_type=list(result.reftest_type))

        if expectations.model().has_modifier(test_name, test_expectations.WONTFIX):
            test_dict['wontfix'] = True

        if result_type == test_expectations.PASS:
            num_passes += 1
            # FIXME: include passing tests that have stderr output.
            if expected == 'PASS' and not include_passes:
                continue
        elif result_type == test_expectations.CRASH:
            if test_name in initial_results.unexpected_results_by_name:
                num_regressions += 1
                test_dict['report'] = 'REGRESSION'
        elif result_type == test_expectations.MISSING:
            if test_name in initial_results.unexpected_results_by_name:
                num_missing += 1
                test_dict['report'] = 'MISSING'
        elif test_name in initial_results.unexpected_results_by_name:
            if retry_results and test_name in retry_results.unexpected_results_by_name:
                num_regressions += 1
                test_dict['report'] = 'REGRESSION'
                retry_result_type = retry_results.unexpected_results_by_name[test_name].type
                if result_type != retry_result_type:
                    actual.append(keywords[retry_result_type])
                    if enabled_pixel_tests_in_retry and result_type == test_expectations.TEXT and retry_result_type == test_expectations.MISSING:
                        num_missing += 1
            elif retry_results and test_name in retry_results.expected_results_by_name:
                retry_result_name = keywords[retry_results.expected_results_by_name[test_name].type]
                if retry_result_name not in actual:
                    actual.append(retry_result_name)
                num_flaky += 1
                test_dict['report'] = 'FLAKY'
            else:
                num_regressions += 1
                test_dict['report'] = 'REGRESSION'

        # If a test was run more than once on the initial_results (for example with --repeat-each),
        # check for possible flakiness there and also account in "actual" for the extra results.
        for repeated_result in initial_results.repeated_results_by_name[test_name]:
            repeated_result_name = keywords[repeated_result]
            if repeated_result_name not in actual:
                actual.append(repeated_result_name)
                if test_name in initial_results.unexpected_results_by_name:
                    test_dict['report'] = 'FLAKY'

        test_dict['expected'] = expected
        test_dict['actual'] = " ".join(actual)
        if include_time_and_modifiers:
            test_dict['time'] = round(1000 * result.test_run_time)
            # FIXME: Fix get_modifiers to return modifiers in new format.
            test_dict['modifiers'] = ' '.join(expectations.model().get_modifiers(test_name)).replace('BUGWK', 'webkit.org/b/')

        test_dict.update(_interpret_test_failures(result.failures))

        if retry_results:
            retry_result = retry_results.unexpected_results_by_name.get(test_name)
            if retry_result:
                test_dict.update(_interpret_test_failures(retry_result.failures))

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
    results['has_pretty_patch'] = port_obj.pretty_patch.pretty_patch_available()
    results['pixel_tests_enabled'] = port_obj.get_option('pixel_tests')
    results['other_crashes'] = other_crashes_dict
    results['date'] = datetime.datetime.now().strftime("%I:%M%p on %B %d, %Y")
    results['port_name'] = port_obj.name()
    results['test_configuration'] = dict(port_obj.test_configuration().items())

    # These use the first device type, because we've long ago merged the results for all
    # the device types we ran.
    results['baseline_search_path'] = [
        port_obj.host.filesystem.relpath(p, port_obj.layout_tests_dir())
        for p in port_obj.baseline_search_path(device_type=device_type_list[0])
    ]
    results['expectations_files'] = [
        port_obj.host.filesystem.relpath(p, port_obj.layout_tests_dir())
        for p in port_obj.expectations_files(device_type=device_type_list[0])
    ]

    try:
        # We only use the svn revision for using trac links in the results.html file,
        # Don't do this by default since it takes >100ms.
        # FIXME: Do we really need to populate this both here and in the json_results_generator?
        if port_obj.get_option("builder_name"):
            port_obj.host.initialize_scm()
            results['revision'] = str(port_obj.commits_for_upload()[0].get('identifier', port_obj.commits_for_upload()[0].get('hash')))
    except Exception as e:
        _log.warn("Failed to determine git revision for checkout (cwd: %s, webkit_base: %s), leaving 'revision' key blank in full_results.json.\n%s" % (port_obj._filesystem.getcwd(), port_obj.path_from_webkit_base(), e))
        # Handle cases where we're running outside of version control.
        import traceback
        _log.debug('Failed to learn head git revision:')
        _log.debug(traceback.format_exc())
        results['revision'] = ""

    return results
