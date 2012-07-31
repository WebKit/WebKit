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

PRINT_DEFAULT = "misc,one-line-progress,one-line-summary,unexpected,unexpected-results,updates"
PRINT_EVERYTHING = "actual,config,expected,misc,one-line-progress,one-line-summary,slowest,timing,unexpected,unexpected-results,updates"

HELP_PRINTING = """
Output for run-webkit-tests is controlled by a comma-separated list of
values passed to --print.  Values either influence the overall output, or
the output at the beginning of the run, during the run, or at the end:

Overall options:
    nothing             don't print anything. This overrides every other option
    default             include the default options. This is useful for logging
                        the default options plus additional settings.
    everything          print (almost) everything (except the trace-* options,
                        see below for the full list )
    misc                print miscellaneous things like blank lines

At the beginning of the run:
    config              print the test run configuration
    expected            print a summary of what is expected to happen
                        (# passes, # failures, etc.)

During the run:
    one-line-progress   print a one-line progress message or bar
    unexpected          print any unexpected results as they occur
    updates             print updates on which stage is executing
    trace-everything    print detailed info on every test's results
                        (baselines, expectation, time it took to run). If
                        this is specified it will override the '*-progress'
                        options, the 'trace-unexpected' option, and the
                        'unexpected' option.
    trace-unexpected    like 'trace-everything', but only for tests with
                        unexpected results. If this option is specified,
                        it will override the 'unexpected' option.

At the end of the run:
    actual              print a summary of the actual results
    slowest             print %(slowest)d slowest tests and the time they took
    timing              print timing statistics
    unexpected-results  print a list of the tests with unexpected results
    one-line-summary    print a one-line summary of the run

Notes:
    - If 'nothing' is specified, it overrides all of the other options.
    - Specifying --verbose is equivalent to --print everything plus it
      changes the format of the log messages to add timestamps and other
      information. If you specify --verbose and --print X, then X overrides
      the --print everything implied by --verbose.

--print 'everything' is equivalent to --print '%(everything)s'.

The default (--print default) is equivalent to --print '%(default)s'.
""" % {'slowest': NUM_SLOW_TESTS_TO_LOG, 'everything': PRINT_EVERYTHING,
       'default': PRINT_DEFAULT}


def print_options():
    return [
        # Note: We use print_options rather than just 'print' because print
        # is a reserved word.
        # Note: Also, we don't specify a default value so we can detect when
        # no flag is specified on the command line and use different defaults
        # based on whether or not --verbose is specified (since --print
        # overrides --verbose).
        optparse.make_option("--print", dest="print_options",
            help=("controls print output of test run. "
                  "Use --help-printing for more.")),
        optparse.make_option("--help-printing", action="store_true",
            help="show detailed help on controlling print output"),
        optparse.make_option("-v", "--verbose", action="store_true",
            default=False, help="include debug-level logging"),
   ]


def parse_print_options(print_options, verbose):
    """Parse the options provided to --print and dedup and rank them.

    Returns
        a set() of switches that govern how logging is done

    """
    if print_options:
        switches = set(print_options.split(','))
    elif verbose:
        switches = set(PRINT_EVERYTHING.split(','))
    else:
        switches = set(PRINT_DEFAULT.split(','))

    if 'nothing' in switches:
        return set()

    if 'everything' in switches:
        switches.discard('everything')
        switches.update(set(PRINT_EVERYTHING.split(',')))

    if 'default' in switches:
        switches.discard('default')
        switches.update(set(PRINT_DEFAULT.split(',')))

    if 'trace-everything' in switches:
        switches.discard('one-line-progress')
        switches.discard('trace-unexpected')
        switches.discard('unexpected')

    if 'trace-unexpected' in switches:
        switches.discard('unexpected')

    return switches


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
        """
        Args
          port               interface to port-specific routines
          options            OptionParser object with command line settings
          regular_output     stream to which output intended only for humans
                             should be written
          buildbot_output    stream to which output intended to be read by
                             the buildbots (and humans) should be written
          logger             optional logger to integrate into the stream.
        """
        self._port = port
        self._options = options
        self._buildbot_stream = buildbot_output
        self._meter = MeteredStream(regular_output, options.verbose, logger=logger)
        self.switches = parse_print_options(options.print_options, options.verbose)

    def cleanup(self):
        self._meter.cleanup()

    def __del__(self):
        self.cleanup()

    # These two routines just hide the implementation of the switches.
    def disabled(self, option):
        return not option in self.switches

    def enabled(self, option):
        return option in self.switches

    def help_printing(self):
        self._write(HELP_PRINTING)

    def print_config(self):
        """Prints the configuration for the test run."""
        self._print_config("Using port '%s'" % self._port.name())
        self._print_config("Test configuration: %s" % self._port.test_configuration())
        self._print_config("Placing test results in %s" % self._options.results_directory)

        # FIXME: should these options be in printing_options?
        if self._options.new_baseline:
            self._print_config("Placing new baselines in %s" % self._port.baseline_path())

        fs = self._port.host.filesystem
        fallback_path = [fs.split(x)[1] for x in self._port.baseline_search_path()]
        self._print_config("Baseline search path: %s -> generic" % " -> ".join(fallback_path))

        self._print_config("Using %s build" % self._options.configuration)
        if self._options.pixel_tests:
            self._print_config("Pixel tests enabled")
        else:
            self._print_config("Pixel tests disabled")

        self._print_config("Regular timeout: %s, slow test timeout: %s" %
                           (self._options.time_out_ms, self._options.slow_time_out_ms))

        self._print_config('Command line: ' + ' '.join(self._port.driver_cmd_line()))
        self._print_config('')

    def print_found(self, num_all_test_files, num_to_run, repeat_each, iterations):
        found_str = 'Found %s; running %d' % (grammar.pluralize('test', num_all_test_files), num_to_run)
        if repeat_each * iterations > 1:
            found_str += ', %d times each (--repeat-each=%d, --iterations=%d)' % (repeat_each * iterations, repeat_each, iterations)
        self._print_expected(found_str + '.')

    def print_expected(self, result_summary, tests_with_result_type_callback):
        self._print_expected_results_of_type(result_summary, test_expectations.PASS, "passes", tests_with_result_type_callback)
        self._print_expected_results_of_type(result_summary, test_expectations.FAIL, "failures", tests_with_result_type_callback)
        self._print_expected_results_of_type(result_summary, test_expectations.FLAKY, "flaky", tests_with_result_type_callback)
        self._print_expected('')

    def print_workers_and_shards(self, num_workers, num_shards, num_locked_shards):
        driver_name = self._port.driver_name()
        if num_workers == 1:
            self._print_config("Running 1 %s over %s." %
                (driver_name, grammar.pluralize('shard', num_shards)))
        else:
            self._print_config("Running %d %ss in parallel over %d shards (%d locked)." %
                (num_workers, driver_name, num_shards, num_locked_shards))
        self._print_config('')

    def _print_expected_results_of_type(self, result_summary,
                                        result_type, result_type_str, tests_with_result_type_callback):
        """Print the number of the tests in a given result class.

        Args:
          result_summary - the object containing all the results to report on
          result_type - the particular result type to report in the summary.
          result_type_str - a string description of the result_type.
          expectations - populated TestExpectations object for stats
        """
        tests = tests_with_result_type_callback(result_type)
        now = result_summary.tests_by_timeline[test_expectations.NOW]
        wontfix = result_summary.tests_by_timeline[test_expectations.WONTFIX]

        # We use a fancy format string in order to print the data out in a
        # nicely-aligned table.
        fmtstr = ("Expect: %%5d %%-8s (%%%dd now, %%%dd wontfix)"
                  % (self._num_digits(now), self._num_digits(wontfix)))
        self._print_expected(fmtstr %
            (len(tests), result_type_str, len(tests & now), len(tests & wontfix)))

    def _num_digits(self, num):
        """Returns the number of digits needed to represent the length of a
        sequence."""
        ndigits = 1
        if len(num):
            ndigits = int(math.log10(len(num))) + 1
        return ndigits

    def print_results(self, run_time, thread_timings, test_timings, individual_test_timings, result_summary, unexpected_results):
        self._print_timing_statistics(run_time, thread_timings, test_timings, individual_test_timings, result_summary)
        self._print_result_summary(result_summary)

        self.print_one_line_summary(result_summary.total - result_summary.expected_skips, result_summary.expected - result_summary.expected_skips, result_summary.unexpected)

        self.print_unexpected_results(unexpected_results)

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
        self.print_timing("Test timing:")
        self.print_timing("  %6.2f total testing time" % total_time)
        self.print_timing("")
        self.print_timing("Thread timing:")
        cuml_time = 0
        for t in thread_timings:
            self.print_timing("    %10s: %5d tests, %6.2f secs" %
                  (t['name'], t['num_tests'], t['total_time']))
            cuml_time += t['total_time']
        self.print_timing("   %6.2f cumulative, %6.2f optimal" %
              (cuml_time, cuml_time / int(self._options.child_processes)))
        self.print_timing("")

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

        self.print_timing("")
        self._print_test_list_timing("%s slowest tests that are not "
            "marked as SLOW and did not timeout/crash:" % NUM_SLOW_TESTS_TO_LOG, unexpected_slow_tests)
        self.print_timing("")
        self._print_test_list_timing("Tests marked as SLOW:", slow_tests)
        self.print_timing("")
        self._print_test_list_timing("Tests that timed out or crashed:",
                                     timeout_or_crash_tests)
        self.print_timing("")

    def _print_test_list_timing(self, title, test_list):
        """Print timing info for each test.

        Args:
          title: section heading
          test_list: tests that fall in this section
        """
        if self.disabled('slowest'):
            return

        self.print_timing(title)
        for test_tuple in test_list:
            test_run_time = round(test_tuple.test_run_time, 1)
            self.print_timing("  %s took %s seconds" % (test_tuple.test_name, test_run_time))

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

        self.print_timing("Time to process slowest subdirectories:")
        min_seconds_to_print = 10
        for timing in timings:
            if timing[0] > min_seconds_to_print:
                self.print_timing(
                    "  %s took %s seconds to run %s tests." % (timing[1],
                    timing[0], timing[2]))
        self.print_timing("")

    def _print_statistics_for_test_timings(self, title, timings):
        """Prints the median, mean and standard deviation of the values in
        timings.

        Args:
          title: Title for these timings.
          timings: A list of floats representing times.
        """
        self.print_timing(title)
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
        self.print_timing("  Median:          %6.3f" % median)
        self.print_timing("  Mean:            %6.3f" % mean)
        self.print_timing("  90th percentile: %6.3f" % percentile90)
        self.print_timing("  99th percentile: %6.3f" % percentile99)
        self.print_timing("  Standard dev:    %6.3f" % std_deviation)
        self.print_timing("")

    def _print_result_summary(self, result_summary):
        """Print a short summary about how many tests passed.

        Args:
          result_summary: information to log
        """
        failed = result_summary.total_failures
        total = result_summary.total - result_summary.expected_skips
        passed = total - failed - result_summary.remaining
        pct_passed = 0.0
        if total > 0:
            pct_passed = float(passed) * 100 / total

        self.print_actual("")
        self.print_actual("=> Results: %d/%d tests passed (%.1f%%)" %
                     (passed, total, pct_passed))
        self.print_actual("")
        self._print_result_summary_entry(result_summary,
            test_expectations.NOW, "Tests to be fixed")

        self.print_actual("")
        # FIXME: We should be skipping anything marked WONTFIX, so we shouldn't bother logging these stats.
        self._print_result_summary_entry(result_summary,
            test_expectations.WONTFIX,
            "Tests that will only be fixed if they crash (WONTFIX)")
        self.print_actual("")

    def _print_result_summary_entry(self, result_summary, timeline,
                                    heading):
        """Print a summary block of results for a particular timeline of test.

        Args:
          result_summary: summary to print results for
          timeline: the timeline to print results for (NOW, WONTFIX, etc.)
          heading: a textual description of the timeline
        """
        total = len(result_summary.tests_by_timeline[timeline])
        not_passing = (total -
           len(result_summary.tests_by_expectation[test_expectations.PASS] &
               result_summary.tests_by_timeline[timeline]))
        self.print_actual("=> %s (%d):" % (heading, not_passing))

        for result in TestExpectations.EXPECTATION_ORDER:
            if result in (test_expectations.PASS, test_expectations.SKIP):
                continue
            results = (result_summary.tests_by_expectation[result] &
                       result_summary.tests_by_timeline[timeline])
            desc = TestExpectations.EXPECTATION_DESCRIPTIONS[result]
            if not_passing and len(results):
                pct = len(results) * 100.0 / not_passing
                self.print_actual("  %5d %-24s (%4.1f%%)" %
                    (len(results), desc[len(results) != 1], pct))


    def print_actual(self, msg):
        if self.disabled('actual'):
            return
        self._buildbot_stream.write("%s\n" % msg)

    def _print_config(self, msg):
        self.write(msg, 'config')

    def _print_expected(self, msg):
        self.write(msg, 'expected')

    def print_timing(self, msg):
        self.write(msg, 'timing')

    def print_one_line_summary(self, total, expected, unexpected):
        """Print a one-line summary of the test run to stdout.

        Args:
          total: total number of tests run
          expected: number of expected results
          unexpected: number of unexpected results
        """
        if self.disabled('one-line-summary'):
            return

        incomplete = total - expected - unexpected
        incomplete_str = ''
        if incomplete:
            self._write("")
            incomplete_str = " (%d didn't run)" % incomplete

        if unexpected == 0:
            if expected == total:
                if expected > 1:
                    self._write("All %d tests ran as expected." % expected)
                else:
                    self._write("The test ran as expected.")
            else:
                self._write("%s ran as expected%s." % (grammar.pluralize('test', expected), incomplete_str))
        else:
            self._write("%s ran as expected, %d didn't%s:" % (grammar.pluralize('test', expected), unexpected, incomplete_str))
        self._write("")

    def print_finished_test(self, result, expected, exp_str, got_str, result_summary, retrying, test_files_list):
        self.print_test_result(result, expected, exp_str, got_str)
        self.print_progress(result_summary, retrying, test_files_list)

    def print_test_result(self, result, expected, exp_str, got_str):
        """Print the result of the test as determined by --print.

        This routine is used to print the details of each test as it completes.

        Args:
            result   - The actual TestResult object
            expected - Whether the result we got was an expected result
            exp_str  - What we expected to get (used for tracing)
            got_str  - What we actually got (used for tracing)

        Note that we need all of these arguments even though they seem
        somewhat redundant, in order to keep this routine from having to
        known anything about the set of expectations.
        """
        if (self.enabled('trace-everything') or
            self.enabled('trace-unexpected') and not expected):
            self._print_test_trace(result, exp_str, got_str)
        elif not expected and self.enabled('unexpected'):
            self._print_unexpected_test_result(result)

    def _print_test_trace(self, result, exp_str, got_str):
        """Print detailed results of a test (triggered by --print trace-*).
        For each test, print:
           - location of the expected baselines
           - expected results
           - actual result
           - timing info
        """
        test_name = result.test_name
        self._write('trace: %s' % test_name)

        base = self._port.lookup_virtual_test_base(test_name)
        if base:
            args = ' '.join(self._port.lookup_virtual_test_args(test_name))
            self._write(' base: %s' % base)
            self._write(' args: %s' % args)

        for extension in ('.txt', '.png', '.wav', '.webarchive'):
            self._print_baseline(test_name, extension)

        self._write('  exp: %s' % exp_str)
        self._write('  got: %s' % got_str)
        self._write(' took: %-.3f' % result.test_run_time)
        self._write('')

    def _print_baseline(self, test_name, extension):
        baseline = self._port.expected_filename(test_name, extension)
        if self._port._filesystem.exists(baseline):
            relpath = self._port.relative_test_filename(baseline)
        else:
            relpath = '<none>'
        self._write('  %s: %s' % (extension[1:], relpath))

    def _print_unexpected_test_result(self, result):
        """Prints one unexpected test result line."""
        desc = TestExpectations.EXPECTATION_DESCRIPTIONS[result.type][0]
        self.write("  %s -> unexpected %s" % (result.test_name, desc), "unexpected")

    def print_progress(self, result_summary, retrying, test_list):
        """Print progress through the tests as determined by --print."""
        if self.disabled('one-line-progress'):
            return

        if result_summary.remaining == 0:
            self._meter.write_update('')
            return

        percent_complete = 100 * (result_summary.expected +
            result_summary.unexpected) / result_summary.total
        action = "Testing"
        if retrying:
            action = "Retrying"

        self._meter.write_throttled_update("%s (%d%%): %d ran as expected, %d didn't, %d left" %
            (action, percent_complete, result_summary.expected,
             result_summary.unexpected, result_summary.remaining))

    def print_unexpected_results(self, unexpected_results):
        """Prints a list of the unexpected results to the buildbot stream."""
        if self.disabled('unexpected-results'):
            return

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
                    add_to_dict_of_lists(passes,
                                         'Expected to crash, but passed',
                                         test)
                elif 'TIMEOUT' in expected:
                    add_to_dict_of_lists(passes,
                                         'Expected to timeout, but passed',
                                          test)
                else:
                    add_to_dict_of_lists(passes,
                                         'Expected to fail, but passed',
                                         test)
            elif len(actual) > 1:
                # We group flaky tests by the first actual result we got.
                add_to_dict_of_lists(flaky, actual[0], test)
            else:
                add_to_dict_of_lists(regressions, results['actual'], test)

        resultsjsonparser.for_each_test(unexpected_results['tests'], add_result)

        if len(passes) or len(flaky) or len(regressions):
            self._buildbot_stream.write("\n")

        if len(passes):
            for key, tests in passes.iteritems():
                self._buildbot_stream.write("%s: (%d)\n" % (key, len(tests)))
                tests.sort()
                for test in tests:
                    self._buildbot_stream.write("  %s\n" % test)
                self._buildbot_stream.write("\n")
            self._buildbot_stream.write("\n")

        if len(flaky):
            descriptions = TestExpectations.EXPECTATION_DESCRIPTIONS
            for key, tests in flaky.iteritems():
                result = TestExpectations.EXPECTATIONS[key.lower()]
                self._buildbot_stream.write("Unexpected flakiness: %s (%d)\n"
                    % (descriptions[result][1], len(tests)))
                tests.sort()

                for test in tests:
                    result = resultsjsonparser.result_for_test(unexpected_results['tests'], test)
                    actual = result['actual'].split(" ")
                    expected = result['expected'].split(" ")
                    result = TestExpectations.EXPECTATIONS[key.lower()]
                    new_expectations_list = list(set(actual) | set(expected))
                    self._buildbot_stream.write("  %s = %s\n" %
                        (test, " ".join(new_expectations_list)))
                self._buildbot_stream.write("\n")
            self._buildbot_stream.write("\n")

        if len(regressions):
            descriptions = TestExpectations.EXPECTATION_DESCRIPTIONS
            for key, tests in regressions.iteritems():
                result = TestExpectations.EXPECTATIONS[key.lower()]
                self._buildbot_stream.write(
                    "Regressions: Unexpected %s : (%d)\n" % (
                    descriptions[result][1], len(tests)))
                tests.sort()
                for test in tests:
                    self._buildbot_stream.write("  %s = %s\n" % (test, key))
                self._buildbot_stream.write("\n")
            self._buildbot_stream.write("\n")

        if len(unexpected_results['tests']) and self._options.verbose:
            self._buildbot_stream.write("%s\n" % ("-" * 78))

    def write_update(self, msg):
        if self.disabled('updates'):
            return
        self._meter.write_update(msg)

    def write(self, msg, option="misc"):
        if self.disabled(option):
            return
        self._write(msg)

    def writeln(self, *args, **kwargs):
        self._meter.writeln(*args, **kwargs)

    def _write(self, msg):
        self._meter.writeln(msg)

    def flush(self):
        self._meter.flush()
