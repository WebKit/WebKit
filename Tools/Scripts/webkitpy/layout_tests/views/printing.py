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

import optparse

from webkitpy.common.net import resultsjsonparser
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

    def print_update(self, msg):
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
