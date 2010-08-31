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

"""Package that handles non-debug, non-file output for run-webkit-tests."""

import logging
import optparse
import os
import pdb

from webkitpy.layout_tests.layout_package import metered_stream
from webkitpy.layout_tests.layout_package import test_expectations

_log = logging.getLogger("webkitpy.layout_tests.printer")

TestExpectationsFile = test_expectations.TestExpectationsFile

NUM_SLOW_TESTS_TO_LOG = 10

PRINT_DEFAULT = ("misc,one-line-progress,one-line-summary,unexpected,"
                 "unexpected-results,updates")
PRINT_EVERYTHING = ("actual,config,expected,misc,one-line-progress,"
                    "one-line-summary,slowest,timing,unexpected,"
                    "unexpected-results,updates")

HELP_PRINTING = """
Output for run-webkit-tests is controlled by a comma-separated list of
values passed to --print.  Values either influence the overall output, or
the output at the beginning of the run, during the run, or at the end:

Overall options:
    nothing             don't print anything. This overrides every other option
    default             include the default options. This is useful for logging
                        the default options plus additional settings.
    everything          print everything (except the trace-* options and the
                        detailed-progress option, see below for the full list )
    misc                print miscellaneous things like blank lines

At the beginning of the run:
    config              print the test run configuration
    expected            print a summary of what is expected to happen
                        (# passes, # failures, etc.)

During the run:
    detailed-progress   print one dot per test completed
    one-line-progress   print a one-line progress bar
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
    - 'detailed-progress' can only be used if running in a single thread
      (using --child-processes=1) or a single queue of tests (using
       --experimental-fully-parallel). If these conditions aren't true,
      'one-line-progress' will be used instead.
    - If both 'detailed-progress' and 'one-line-progress' are specified (and
      both are possible), 'detailed-progress' will be used.
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


def configure_logging(options, meter):
    """Configures the logging system."""
    log_fmt = '%(message)s'
    log_datefmt = '%y%m%d %H:%M:%S'
    log_level = logging.INFO
    if options.verbose:
        log_fmt = ('%(asctime)s %(filename)s:%(lineno)-4d %(levelname)s '
                   '%(message)s')
        log_level = logging.DEBUG

    logging.basicConfig(level=log_level, format=log_fmt,
                        datefmt=log_datefmt, stream=meter)


def parse_print_options(print_options, verbose, child_processes,
                        is_fully_parallel):
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

    if (child_processes != 1 and not is_fully_parallel and
        'detailed-progress' in switches):
        _log.warn("Can only print 'detailed-progress' if running "
                  "with --child-processes=1 or "
                  "with --experimental-fully-parallel. "
                  "Using 'one-line-progress' instead.")
        switches.discard('detailed-progress')
        switches.add('one-line-progress')

    if 'everything' in switches:
        switches.discard('everything')
        switches.update(set(PRINT_EVERYTHING.split(',')))

    if 'default' in switches:
        switches.discard('default')
        switches.update(set(PRINT_DEFAULT.split(',')))

    if 'detailed-progress' in switches:
        switches.discard('one-line-progress')

    if 'trace-everything' in switches:
        switches.discard('detailed-progress')
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
    def __init__(self, port, options, regular_output, buildbot_output,
                 child_processes, is_fully_parallel):
        """
        Args
          port               interface to port-specific routines
          options            OptionParser object with command line settings
          regular_output     stream to which output intended only for humans
                             should be written
          buildbot_output    stream to which output intended to be read by
                             the buildbots (and humans) should be written
          child_processes    number of parallel threads running (usually
                             controlled by --child-processes)
          is_fully_parallel  are the tests running in a single queue, or
                             in shards (usually controlled by
                             --experimental-fully-parallel)

        Note that the last two args are separate rather than bundled into
        the options structure so that this object does not assume any flags
        set in options that weren't returned from logging_options(), above.
        The two are used to determine whether or not we can sensibly use
        the 'detailed-progress' option, or can only use 'one-line-progress'.
        """
        self._buildbot_stream = buildbot_output
        self._options = options
        self._port = port
        self._stream = regular_output

        # These are used for --print detailed-progress to track status by
        # directory.
        self._current_dir = None
        self._current_progress_str = ""
        self._current_test_number = 0

        self._meter = metered_stream.MeteredStream(options.verbose,
                                                   regular_output)
        configure_logging(self._options, self._meter)

        self.switches = parse_print_options(options.print_options,
            options.verbose, child_processes, is_fully_parallel)

    # These two routines just hide the implmentation of the switches.
    def disabled(self, option):
        return not option in self.switches

    def enabled(self, option):
        return option in self.switches

    def help_printing(self):
        self._write(HELP_PRINTING)

    def print_actual(self, msg):
        if self.disabled('actual'):
            return
        self._buildbot_stream.write("%s\n" % msg)

    def print_config(self, msg):
        self.write(msg, 'config')

    def print_expected(self, msg):
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
        if incomplete:
            self._write("")
            incomplete_str = " (%d didn't run)" % incomplete
            expected_str = str(expected)
        else:
            incomplete_str = ""
            expected_str = "All %d" % expected

        if unexpected == 0:
            self._write("%s tests ran as expected%s." %
                        (expected_str, incomplete_str))
        elif expected == 1:
            self._write("1 test ran as expected, %d didn't%s:" %
                        (unexpected, incomplete_str))
        else:
            self._write("%d tests ran as expected, %d didn't%s:" %
                        (expected, unexpected, incomplete_str))
        self._write("")

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
        elif (not expected and self.enabled('unexpected') and
              self.disabled('detailed-progress')):
            # Note: 'detailed-progress' handles unexpected results internally,
            # so we skip it here.
            self._print_unexpected_test_result(result)

    def _print_test_trace(self, result, exp_str, got_str):
        """Print detailed results of a test (triggered by --print trace-*).
        For each test, print:
           - location of the expected baselines
           - expected results
           - actual result
           - timing info
        """
        filename = result.filename
        test_name = self._port.relative_test_filename(filename)
        self._write('trace: %s' % test_name)
        self._write('  txt: %s' %
                  self._port.relative_test_filename(
                       self._port.expected_filename(filename, '.txt')))
        png_file = self._port.expected_filename(filename, '.png')
        if os.path.exists(png_file):
            self._write('  png: %s' %
                        self._port.relative_test_filename(png_file))
        else:
            self._write('  png: <none>')
        self._write('  exp: %s' % exp_str)
        self._write('  got: %s' % got_str)
        self._write(' took: %-.3f' % result.test_run_time)
        self._write('')

    def _print_unexpected_test_result(self, result):
        """Prints one unexpected test result line."""
        desc = TestExpectationsFile.EXPECTATION_DESCRIPTIONS[result.type][0]
        self.write("  %s -> unexpected %s" %
                   (self._port.relative_test_filename(result.filename),
                    desc), "unexpected")

    def print_progress(self, result_summary, retrying, test_list):
        """Print progress through the tests as determined by --print."""
        if self.enabled('detailed-progress'):
            self._print_detailed_progress(result_summary, test_list)
        elif self.enabled('one-line-progress'):
            self._print_one_line_progress(result_summary, retrying)
        else:
            return

        if result_summary.remaining == 0:
            self._meter.update('')

    def _print_one_line_progress(self, result_summary, retrying):
        """Displays the progress through the test run."""
        percent_complete = 100 * (result_summary.expected +
            result_summary.unexpected) / result_summary.total
        action = "Testing"
        if retrying:
            action = "Retrying"
        self._meter.progress("%s (%d%%): %d ran as expected, %d didn't,"
            " %d left" % (action, percent_complete, result_summary.expected,
             result_summary.unexpected, result_summary.remaining))

    def _print_detailed_progress(self, result_summary, test_list):
        """Display detailed progress output where we print the directory name
        and one dot for each completed test. This is triggered by
        "--log detailed-progress"."""
        if self._current_test_number == len(test_list):
            return

        next_test = test_list[self._current_test_number]
        next_dir = os.path.dirname(
            self._port.relative_test_filename(next_test))
        if self._current_progress_str == "":
            self._current_progress_str = "%s: " % (next_dir)
            self._current_dir = next_dir

        while next_test in result_summary.results:
            if next_dir != self._current_dir:
                self._meter.write("%s\n" % (self._current_progress_str))
                self._current_progress_str = "%s: ." % (next_dir)
                self._current_dir = next_dir
            else:
                self._current_progress_str += "."

            if (next_test in result_summary.unexpected_results and
                self.enabled('unexpected')):
                self._meter.write("%s\n" % self._current_progress_str)
                test_result = result_summary.results[next_test]
                self._print_unexpected_test_result(test_result)
                self._current_progress_str = "%s: " % self._current_dir

            self._current_test_number += 1
            if self._current_test_number == len(test_list):
                break

            next_test = test_list[self._current_test_number]
            next_dir = os.path.dirname(
                self._port.relative_test_filename(next_test))

        if result_summary.remaining:
            remain_str = " (%d)" % (result_summary.remaining)
            self._meter.progress("%s%s" % (self._current_progress_str,
                                           remain_str))
        else:
            self._meter.progress("%s" % (self._current_progress_str))

    def print_unexpected_results(self, unexpected_results):
        """Prints a list of the unexpected results to the buildbot stream."""
        if self.disabled('unexpected-results'):
            return

        passes = {}
        flaky = {}
        regressions = {}

        for test, results in unexpected_results['tests'].iteritems():
            actual = results['actual'].split(" ")
            expected = results['expected'].split(" ")
            if actual == ['PASS']:
                if 'CRASH' in expected:
                    _add_to_dict_of_lists(passes,
                                          'Expected to crash, but passed',
                                          test)
                elif 'TIMEOUT' in expected:
                    _add_to_dict_of_lists(passes,
                                          'Expected to timeout, but passed',
                                           test)
                else:
                    _add_to_dict_of_lists(passes,
                                          'Expected to fail, but passed',
                                          test)
            elif len(actual) > 1:
                # We group flaky tests by the first actual result we got.
                _add_to_dict_of_lists(flaky, actual[0], test)
            else:
                _add_to_dict_of_lists(regressions, results['actual'], test)

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
            descriptions = TestExpectationsFile.EXPECTATION_DESCRIPTIONS
            for key, tests in flaky.iteritems():
                result = TestExpectationsFile.EXPECTATIONS[key.lower()]
                self._buildbot_stream.write("Unexpected flakiness: %s (%d)\n"
                    % (descriptions[result][1], len(tests)))
                tests.sort()

                for test in tests:
                    result = unexpected_results['tests'][test]
                    actual = result['actual'].split(" ")
                    expected = result['expected'].split(" ")
                    result = TestExpectationsFile.EXPECTATIONS[key.lower()]
                    new_expectations_list = list(set(actual) | set(expected))
                    self._buildbot_stream.write("  %s = %s\n" %
                        (test, " ".join(new_expectations_list)))
                self._buildbot_stream.write("\n")
            self._buildbot_stream.write("\n")

        if len(regressions):
            descriptions = TestExpectationsFile.EXPECTATION_DESCRIPTIONS
            for key, tests in regressions.iteritems():
                result = TestExpectationsFile.EXPECTATIONS[key.lower()]
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

    def print_update(self, msg):
        if self.disabled('updates'):
            return
        self._meter.update(msg)

    def write(self, msg, option="misc"):
        if self.disabled(option):
            return
        self._write(msg)

    def _write(self, msg):
        # FIXME: we could probably get away with calling _log.info() all of
        # the time, but there doesn't seem to be a good way to test the output
        # from the logger :(.
        if self._options.verbose:
            _log.info(msg)
        else:
            self._meter.write("%s\n" % msg)

#
# Utility routines used by the Controller class
#


def _add_to_dict_of_lists(dict, key, value):
    dict.setdefault(key, []).append(value)
