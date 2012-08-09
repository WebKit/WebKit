#!/usr/bin/env python
# Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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

"""Package that handles non-debug, non-file output for run-webkit-tests."""

import math
import optparse

from webkitpy.tool import grammar
from webkitpy.common.net import resultsjsonparser
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models.test_expectations import TestExpectations
from webkitpy.layout_tests.views.metered_stream import MeteredStream


NUM_SLOW_TESTS_TO_LOG = 10


def print_options():
    return [
        optparse.make_option('-q', '--quiet', action='store_true', default=False,
                             help='run quietly (errors, warnings, and progress only)'),
        optparse.make_option('-v', '--verbose', action='store_true', default=False,
                             help='print a summarized result for every test (one line per test)'),
        optparse.make_option('--details', action='store_true', default=False,
                             help='print detailed results for every test'),
        optparse.make_option('--debug-rwt-logging', action='store_true', default=False,
                             help='print timestamps and debug information for run-webkit-tests itself'),
    ]


class Printer(object):
    """Class handling all non-debug-logging printing done by run-webkit-tests.

    Printing from run-webkit-tests falls into two buckets: general or
    regular output that is read only by humans and can be changed at any
    time, and output that is parsed by buildbots (and humans) and hence
    must be changed more carefully and in coordination with the buildbot
    parsing code (in chromium.org's buildbot/master.chromium/scripts/master/
    log_parser/webkit_test_command.py script).

    By default the buildbot-parsed code gets logged to stdout, and regular
    output gets logged to stderr."""
    def __init__(self, port, options, regular_output, buildbot_output, logger=None):
        self.num_completed = 0
        self.num_tests = 0
        self._port = port
        self._options = options
        self._buildbot_stream = buildbot_output
        self._meter = MeteredStream(regular_output, options.debug_rwt_logging, logger=logger)
        self._running_tests = []
        self._completed_tests = []

    def cleanup(self):
        self._meter.cleanup()

    def __del__(self):
        self.cleanup()

    def print_config(self):
        self._print_default("Using port '%s'" % self._port.name())
        self._print_default("Test configuration: %s" % self._port.test_configuration())
        self._print_default("Placing test results in %s" % self._options.results_directory)

        # FIXME: should these options be in printing_options?
        if self._options.new_baseline:
            self._print_default("Placing new baselines in %s" % self._port.baseline_path())

        fs = self._port.host.filesystem
        fallback_path = [fs.split(x)[1] for x in self._port.baseline_search_path()]
        self._print_default("Baseline search path: %s -> generic" % " -> ".join(fallback_path))

        self._print_default("Using %s build" % self._options.configuration)
        if self._options.pixel_tests:
            self._print_default("Pixel tests enabled")
        else:
            self._print_default("Pixel tests disabled")

        self._print_default("Regular timeout: %s, slow test timeout: %s" %
                  (self._options.time_out_ms, self._options.slow_time_out_ms))

        self._print_default('Command line: ' + ' '.join(self._port.driver_cmd_line()))
        self._print_default('')

    def print_found(self, num_all_test_files, num_to_run, repeat_each, iterations):
        found_str = 'Found %s; running %d' % (grammar.pluralize('test', num_all_test_files), num_to_run)
        if repeat_each * iterations > 1:
            found_str += ' (%d times each: --repeat-each=%d --iterations=%d)' % (repeat_each * iterations, repeat_each, iterations)
        found_str += ', skipping %d' % (num_all_test_files - num_to_run)
        self._print_default(found_str + '.')

    def print_expected(self, result_summary, tests_with_result_type_callback):
        self._print_expected_results_of_type(result_summary, test_expectations.PASS, "passes", tests_with_result_type_callback)
        self._print_expected_results_of_type(result_summary, test_expectations.FAIL, "failures", tests_with_result_type_callback)
        self._print_expected_results_of_type(result_summary, test_expectations.FLAKY, "flaky", tests_with_result_type_callback)
        self._print_debug('')

    def print_workers_and_shards(self, num_workers, num_shards, num_locked_shards):
        driver_name = self._port.driver_name()
        if num_workers == 1:
            self._print_default("Running 1 %s over %s." % (driver_name, grammar.pluralize('shard', num_shards)))
        else:
            self._print_default("Running %d %ss in parallel over %d shards (%d locked)." %
                (num_workers, driver_name, num_shards, num_locked_shards))
        self._print_default('')

    def _print_expected_results_of_type(self, result_summary, result_type, result_type_str, tests_with_result_type_callback):
        tests = tests_with_result_type_callback(result_type)
        now = result_summary.tests_by_timeline[test_expectations.NOW]
        wontfix = result_summary.tests_by_timeline[test_expectations.WONTFIX]

        # We use a fancy format string in order to print the data out in a
        # nicely-aligned table.
        fmtstr = ("Expect: %%5d %%-8s (%%%dd now, %%%dd wontfix)"
                  % (self._num_digits(now), self._num_digits(wontfix)))
        self._print_debug(fmtstr % (len(tests), result_type_str, len(tests & now), len(tests & wontfix)))

    def _num_digits(self, num):
        ndigits = 1
        if len(num):
            ndigits = int(math.log10(len(num))) + 1
        return ndigits

    def print_results(self, run_time, thread_timings, test_timings, individual_test_timings, result_summary, unexpected_results):
        self._print_timing_statistics(run_time, thread_timings, test_timings, individual_test_timings, result_summary)
        self._print_result_summary(result_summary)
        self._print_one_line_summary(result_summary.total - result_summary.expected_skips,
                                     result_summary.expected - result_summary.expected_skips,
                                     result_summary.unexpected)
        self._print_unexpected_results(unexpected_results)

    def _print_timing_statistics(self, total_time, thread_timings,
                                 directory_test_timings, individual_test_timings,
                                 result_summary):
        self._print_debug("Test timing:")
        self._print_debug("  %6.2f total testing time" % total_time)
        self._print_debug("")
        self._print_debug("Thread timing:")
        cuml_time = 0
        for t in thread_timings:
            self._print_debug("    %10s: %5d tests, %6.2f secs" % (t['name'], t['num_tests'], t['total_time']))
            cuml_time += t['total_time']
        self._print_debug("   %6.2f cumulative, %6.2f optimal" % (cuml_time, cuml_time / int(self._options.child_processes)))
        self._print_debug("")

        self._print_aggregate_test_statistics(individual_test_timings)
        self._print_individual_test_times(individual_test_timings, result_summary)
        self._print_directory_timings(directory_test_timings)

    def _print_aggregate_test_statistics(self, individual_test_timings):
        times_for_dump_render_tree = [test_stats.test_run_time for test_stats in individual_test_timings]
        self._print_statistics_for_test_timings("PER TEST TIME IN TESTSHELL (seconds):", times_for_dump_render_tree)

    def _print_individual_test_times(self, individual_test_timings, result_summary):
        # Reverse-sort by the time spent in DumpRenderTree.
        individual_test_timings.sort(lambda a, b: cmp(b.test_run_time, a.test_run_time))
        num_printed = 0
        slow_tests = []
        timeout_or_crash_tests = []
        unexpected_slow_tests = []
        for test_tuple in individual_test_timings:
            test_name = test_tuple.test_name
            is_timeout_crash_or_slow = False
            if test_name in result_summary.slow_tests:
                is_timeout_crash_or_slow = True
                slow_tests.append(test_tuple)

            if test_name in result_summary.failures:
                result = result_summary.results[test_name].type
                if (result == test_expectations.TIMEOUT or
                    result == test_expectations.CRASH):
                    is_timeout_crash_or_slow = True
                    timeout_or_crash_tests.append(test_tuple)

            if (not is_timeout_crash_or_slow and num_printed < NUM_SLOW_TESTS_TO_LOG):
                num_printed = num_printed + 1
                unexpected_slow_tests.append(test_tuple)

        self._print_debug("")
        self._print_test_list_timing("%s slowest tests that are not marked as SLOW and did not timeout/crash:" %
            NUM_SLOW_TESTS_TO_LOG, unexpected_slow_tests)
        self._print_debug("")
        self._print_test_list_timing("Tests marked as SLOW:", slow_tests)
        self._print_debug("")
        self._print_test_list_timing("Tests that timed out or crashed:", timeout_or_crash_tests)
        self._print_debug("")

    def _print_test_list_timing(self, title, test_list):
        self._print_debug(title)
        for test_tuple in test_list:
            test_run_time = round(test_tuple.test_run_time, 1)
            self._print_debug("  %s took %s seconds" % (test_tuple.test_name, test_run_time))

    def _print_directory_timings(self, directory_test_timings):
        timings = []
        for directory in directory_test_timings:
            num_tests, time_for_directory = directory_test_timings[directory]
            timings.append((round(time_for_directory, 1), directory, num_tests))
        timings.sort()

        self._print_debug("Time to process slowest subdirectories:")
        min_seconds_to_print = 10
        for timing in timings:
            if timing[0] > min_seconds_to_print:
                self._print_debug("  %s took %s seconds to run %s tests." % (timing[1], timing[0], timing[2]))
        self._print_debug("")

    def _print_statistics_for_test_timings(self, title, timings):
        self._print_debug(title)
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
        self._print_debug("  Median:          %6.3f" % median)
        self._print_debug("  Mean:            %6.3f" % mean)
        self._print_debug("  90th percentile: %6.3f" % percentile90)
        self._print_debug("  99th percentile: %6.3f" % percentile99)
        self._print_debug("  Standard dev:    %6.3f" % std_deviation)
        self._print_debug("")

    def _print_result_summary(self, result_summary):
        if not self._options.debug_rwt_logging:
            return

        failed = result_summary.total_failures
        total = result_summary.total - result_summary.expected_skips
        passed = total - failed - result_summary.remaining
        pct_passed = 0.0
        if total > 0:
            pct_passed = float(passed) * 100 / total

        self._print_for_bot("=> Results: %d/%d tests passed (%.1f%%)" % (passed, total, pct_passed))
        self._print_for_bot("")
        self._print_result_summary_entry(result_summary, test_expectations.NOW, "Tests to be fixed")

        self._print_for_bot("")
        # FIXME: We should be skipping anything marked WONTFIX, so we shouldn't bother logging these stats.
        self._print_result_summary_entry(result_summary, test_expectations.WONTFIX,
            "Tests that will only be fixed if they crash (WONTFIX)")
        self._print_for_bot("")

    def _print_result_summary_entry(self, result_summary, timeline, heading):
        total = len(result_summary.tests_by_timeline[timeline])
        not_passing = (total -
           len(result_summary.tests_by_expectation[test_expectations.PASS] &
               result_summary.tests_by_timeline[timeline]))
        self._print_for_bot("=> %s (%d):" % (heading, not_passing))

        for result in TestExpectations.EXPECTATION_ORDER:
            if result in (test_expectations.PASS, test_expectations.SKIP):
                continue
            results = (result_summary.tests_by_expectation[result] &
                       result_summary.tests_by_timeline[timeline])
            desc = TestExpectations.EXPECTATION_DESCRIPTIONS[result]
            if not_passing and len(results):
                pct = len(results) * 100.0 / not_passing
                self._print_for_bot("  %5d %-24s (%4.1f%%)" % (len(results), desc[0], pct))

    def _print_one_line_summary(self, total, expected, unexpected):
        incomplete = total - expected - unexpected
        incomplete_str = ''
        if incomplete:
            self._print_default("")
            incomplete_str = " (%d didn't run)" % incomplete

        if self._options.verbose or self._options.debug_rwt_logging or unexpected:
            self.writeln("")

        summary = ''
        if unexpected == 0:
            if expected == total:
                if expected > 1:
                    summary = "All %d tests ran as expected." % expected
                else:
                    summary = "The test ran as expected."
            else:
                summary = "%s ran as expected%s." % (grammar.pluralize('test', expected), incomplete_str)
        else:
            summary = "%s ran as expected, %d didn't%s:" % (grammar.pluralize('test', expected), unexpected, incomplete_str)

        self._print_quiet(summary)
        self._print_quiet("")

    def print_started_test(self, test_name):
        self._running_tests.append(test_name)
        if len(self._running_tests) > 1:
            suffix = ' (+%d)' % (len(self._running_tests) - 1)
        else:
            suffix = ''
        if self._options.verbose:
            write = self._meter.write_update
        else:
            write = self._meter.write_throttled_update
        write('[%d/%d] %s%s' % (self.num_completed, self.num_tests, test_name, suffix))

    def print_finished_test(self, result, expected, exp_str, got_str):
        self.num_completed += 1
        test_name = result.test_name
        if self._options.details:
            self._print_test_trace(result, exp_str, got_str)
        elif (self._options.verbose and not self._options.debug_rwt_logging) or not expected:
            desc = TestExpectations.EXPECTATION_DESCRIPTIONS[result.type]
            suffix = ' ' + desc[1]
            if not expected:
                suffix += ' unexpectedly' + desc[2]
            self.writeln("[%d/%d] %s%s" % (self.num_completed, self.num_tests, test_name, suffix))
        elif self.num_completed == self.num_tests:
            self._meter.write_update('')
        else:
            desc = TestExpectations.EXPECTATION_DESCRIPTIONS[result.type]
            suffix = ' ' + desc[1]
            if test_name == self._running_tests[0]:
                self._completed_tests.insert(0, [test_name, suffix])
            else:
                self._completed_tests.append([test_name, suffix])

            for test_name, suffix in self._completed_tests:
                self._meter.write_throttled_update('[%d/%d] %s%s' % (self.num_completed, self.num_tests, test_name, suffix))
            self._completed_tests = []
        self._running_tests.remove(test_name)

    def _print_test_trace(self, result, exp_str, got_str):
        test_name = result.test_name
        self._print_default('[%d/%d] %s' % (self.num_completed, self.num_tests, test_name))

        base = self._port.lookup_virtual_test_base(test_name)
        if base:
            args = ' '.join(self._port.lookup_virtual_test_args(test_name))
            self._print_default(' base: %s' % base)
            self._print_default(' args: %s' % args)

        for extension in ('.txt', '.png', '.wav', '.webarchive'):
            self._print_baseline(test_name, extension)

        self._print_default('  exp: %s' % exp_str)
        self._print_default('  got: %s' % got_str)
        self._print_default(' took: %-.3f' % result.test_run_time)
        self._print_default('')

    def _print_baseline(self, test_name, extension):
        baseline = self._port.expected_filename(test_name, extension)
        if self._port._filesystem.exists(baseline):
            relpath = self._port.relative_test_filename(baseline)
        else:
            relpath = '<none>'
        self._print_default('  %s: %s' % (extension[1:], relpath))

    def _print_progress(self, result_summary, retrying, test_list):
        """Print progress through the tests as determined by --print."""
    def _print_unexpected_results(self, unexpected_results):
        # Prints to the buildbot stream
        passes = {}
        flaky = {}
        regressions = {}

        def add_to_dict_of_lists(dict, key, value):
            dict.setdefault(key, []).append(value)

        def add_result(test, results, passes=passes, flaky=flaky, regressions=regressions):
            actual = results['actual'].split(" ")
            expected = results['expected'].split(" ")
            if actual == ['PASS']:
                if 'CRASH' in expected:
                    add_to_dict_of_lists(passes, 'Expected to crash, but passed', test)
                elif 'TIMEOUT' in expected:
                    add_to_dict_of_lists(passes, 'Expected to timeout, but passed', test)
                else:
                    add_to_dict_of_lists(passes, 'Expected to fail, but passed', test)
            elif len(actual) > 1:
                # We group flaky tests by the first actual result we got.
                add_to_dict_of_lists(flaky, actual[0], test)
            else:
                add_to_dict_of_lists(regressions, results['actual'], test)

        resultsjsonparser.for_each_test(unexpected_results['tests'], add_result)

        if len(passes) or len(flaky) or len(regressions):
            self._print_for_bot("")
        if len(passes):
            for key, tests in passes.iteritems():
                self._print_for_bot("%s: (%d)" % (key, len(tests)))
                tests.sort()
                for test in tests:
                    self._print_for_bot("  %s" % test)
                self._print_for_bot("")
            self._print_for_bot("")

        if len(flaky):
            descriptions = TestExpectations.EXPECTATION_DESCRIPTIONS
            for key, tests in flaky.iteritems():
                result = TestExpectations.EXPECTATIONS[key.lower()]
                self._print_for_bot("Unexpected flakiness: %s (%d)" % (descriptions[result][0], len(tests)))
                tests.sort()

                for test in tests:
                    result = resultsjsonparser.result_for_test(unexpected_results['tests'], test)
                    actual = result['actual'].split(" ")
                    expected = result['expected'].split(" ")
                    result = TestExpectations.EXPECTATIONS[key.lower()]
                    new_expectations_list = list(set(actual) | set(expected))
                    self._print_for_bot("  %s = %s" % (test, " ".join(new_expectations_list)))
                self._print_for_bot("")
            self._print_for_bot("")

        if len(regressions):
            descriptions = TestExpectations.EXPECTATION_DESCRIPTIONS
            for key, tests in regressions.iteritems():
                result = TestExpectations.EXPECTATIONS[key.lower()]
                self._print_for_bot("Regressions: Unexpected %s : (%d)" % (descriptions[result][0], len(tests)))
                tests.sort()
                for test in tests:
                    self._print_for_bot("  %s = %s" % (test, key))
                self._print_for_bot("")

        if len(unexpected_results['tests']) and self._options.debug_rwt_logging:
            self._print_for_bot("%s" % ("-" * 78))

    def _print_quiet(self, msg):
        self.writeln(msg)

    def _print_default(self, msg):
        if not self._options.quiet:
            self.writeln(msg)

    def _print_debug(self, msg):
        if self._options.debug_rwt_logging:
            self.writeln(msg)

    def _print_for_bot(self, msg):
        self._buildbot_stream.write(msg + "\n")

    def write_update(self, msg):
        self._meter.write_update(msg)

    def writeln(self, msg):
        self._meter.writeln(msg)

    def flush(self):
        self._meter.flush()
