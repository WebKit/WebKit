#!/usr/bin/python
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

"""Unit tests for printing.py."""

import optparse
import StringIO
import time
import unittest

from webkitpy.common.host_mock import MockHost

from webkitpy.common.system import logtesting
from webkitpy.layout_tests import port
from webkitpy.layout_tests.controllers import manager
from webkitpy.layout_tests.models import result_summary
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.views import printing


def get_options(args):
    print_options = printing.print_options()
    option_parser = optparse.OptionParser(option_list=print_options)
    return option_parser.parse_args(args)


class TestUtilityFunctions(unittest.TestCase):
    def test_print_options(self):
        options, args = get_options([])
        self.assertTrue(options is not None)

    def test_parse_print_options(self):
        def test_switches(args, expected_switches_str, verbose=False):
            options, args = get_options(args)
            if expected_switches_str:
                expected_switches = set(expected_switches_str.split(','))
            else:
                expected_switches = set()
            switches = printing.parse_print_options(options.print_options,
                                                    verbose)
            self.assertEqual(expected_switches, switches)

        # test that we default to the default set of switches
        test_switches([], printing.PRINT_DEFAULT)

        # test that verbose defaults to everything
        test_switches([], printing.PRINT_EVERYTHING, verbose=True)

        # test that --print default does what it's supposed to
        test_switches(['--print', 'default'], printing.PRINT_DEFAULT)

        # test that --print nothing does what it's supposed to
        test_switches(['--print', 'nothing'], None)

        # test that --print everything does what it's supposed to
        test_switches(['--print', 'everything'], printing.PRINT_EVERYTHING)

        # this tests that '--print X' overrides '--verbose'
        test_switches(['--print', 'actual'], 'actual', verbose=True)



class  Testprinter(unittest.TestCase):
    def assertEmpty(self, stream):
        self.assertFalse(stream.getvalue())

    def assertNotEmpty(self, stream):
        self.assertTrue(stream.getvalue())

    def assertWritten(self, stream, contents):
        self.assertEquals(stream.buflist, contents)

    def reset(self, stream):
        stream.buflist = []
        stream.buf = ''

    def get_printer(self, args=None, tty=False):
        args = args or []
        printing_options = printing.print_options()
        option_parser = optparse.OptionParser(option_list=printing_options)
        options, args = option_parser.parse_args(args)
        host = MockHost()
        self._port = host.port_factory.get('test', options)
        nproc = 2

        regular_output = StringIO.StringIO()
        regular_output.isatty = lambda: tty
        buildbot_output = StringIO.StringIO()
        printer = printing.Printer(self._port, options, regular_output, buildbot_output)
        return printer, regular_output, buildbot_output

    def get_result(self, test_name, result_type=test_expectations.PASS, run_time=0):
        failures = []
        if result_type == test_expectations.TIMEOUT:
            failures = [test_failures.FailureTimeout()]
        elif result_type == test_expectations.CRASH:
            failures = [test_failures.FailureCrash()]
        return test_results.TestResult(test_name, failures=failures, test_run_time=run_time)

    def get_result_summary(self, test_names, expectations_str):
        port.test_expectations = lambda: expectations_str
        port.test_expectations_overrides = lambda: None
        expectations = test_expectations.TestExpectations(self._port, test_names)

        rs = result_summary.ResultSummary(expectations, test_names)
        return test_names, rs, expectations

    def test_help_printer(self):
        # Here and below we'll call the "regular" printer err and the
        # buildbot printer out; this corresponds to how things run on the
        # bots with stderr and stdout.
        printer, err, out = self.get_printer()

        # This routine should print something to stdout. testing what it is
        # is kind of pointless.
        printer.help_printing()
        self.assertNotEmpty(err)
        self.assertEmpty(out)

    def do_switch_tests(self, method_name, switch, to_buildbot,
                        message='hello', exp_err=None, exp_bot=None):
        def do_helper(method_name, switch, message, exp_err, exp_bot):
            printer, err, bot = self.get_printer(['--print', switch], tty=True)
            getattr(printer, method_name)(message)
            self.assertEqual(err.buflist, exp_err)
            self.assertEqual(bot.buflist, exp_bot)

        if to_buildbot:
            if exp_err is None:
                exp_err = []
            if exp_bot is None:
                exp_bot = [message + "\n"]
        else:
            if exp_err is None:
                exp_err = [message + "\n"]
            if exp_bot is None:
                exp_bot = []
        do_helper(method_name, 'nothing', 'hello', [], [])
        do_helper(method_name, switch, 'hello', exp_err, exp_bot)
        do_helper(method_name, 'everything', 'hello', exp_err, exp_bot)

    def test_configure_and_cleanup(self):
        # This test verifies that calling cleanup repeatedly and deleting
        # the object is safe.
        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.cleanup()
        printer.cleanup()
        printer = None

    def test_print_actual(self):
        # Actual results need to be logged to the buildbot's stream.
        self.do_switch_tests('print_actual', 'actual', to_buildbot=True)

    def test_print_actual_buildbot(self):
        # FIXME: Test that the format of the actual results matches what the
        # buildbot is expecting.
        pass

    def test_fallback_path_in_config(self):
        printer, err, out = self.get_printer(['--print', 'everything'])
        # FIXME: it's lame that i have to set these options directly.
        printer._options.results_directory = '/tmp'
        printer._options.pixel_tests = True
        printer._options.new_baseline = True
        printer._options.time_out_ms = 6000
        printer._options.slow_time_out_ms = 12000
        printer.print_config()
        self.assertTrue('Baseline search path: test-mac-leopard -> test-mac-snowleopard -> generic' in err.getvalue())

    def test_print_config(self):
        self.do_switch_tests('_print_config', 'config', to_buildbot=False)

    def test_print_expected(self):
        self.do_switch_tests('_print_expected', 'expected', to_buildbot=False)

    def test_print_timing(self):
        self.do_switch_tests('print_timing', 'timing', to_buildbot=False)

    def test_write_update(self):
        # Note that there shouldn't be a carriage return here; updates()
        # are meant to be overwritten.
        self.do_switch_tests('write_update', 'updates', to_buildbot=False,
                             message='hello', exp_err=['hello'])

    def test_print_one_line_summary(self):
        printer, err, out = self.get_printer(['--print', 'nothing'])
        printer.print_one_line_summary(1, 1, 0)
        self.assertEmpty(err)

        printer, err, out = self.get_printer(['--print', 'one-line-summary'])
        printer.print_one_line_summary(1, 1, 0)
        self.assertWritten(err, ["The test ran as expected.\n", "\n"])

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.print_one_line_summary(1, 1, 0)
        self.assertWritten(err, ["The test ran as expected.\n", "\n"])

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.print_one_line_summary(2, 1, 1)
        self.assertWritten(err, ["1 test ran as expected, 1 didn't:\n", "\n"])

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.print_one_line_summary(3, 2, 1)
        self.assertWritten(err, ["2 tests ran as expected, 1 didn't:\n", "\n"])

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.print_one_line_summary(3, 2, 0)
        self.assertWritten(err, ['\n', "2 tests ran as expected (1 didn't run).\n", '\n'])


    def test_print_test_result(self):
        # Note here that we don't use meaningful exp_str and got_str values;
        # the actual contents of the string are treated opaquely by
        # print_test_result() when tracing, and usually we don't want
        # to test what exactly is printed, just that something
        # was printed (or that nothing was printed).
        #
        # FIXME: this is actually some goofy layering; it would be nice
        # we could refactor it so that the args weren't redundant. Maybe
        # the TestResult should contain what was expected, and the
        # strings could be derived from the TestResult?
        printer, err, out = self.get_printer(['--print', 'nothing'])
        result = self.get_result('passes/image.html')
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertEmpty(err)

        printer, err, out = self.get_printer(['--print', 'unexpected'])
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertEmpty(err)
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertWritten(err, ['  passes/image.html -> unexpected pass\n'])

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertEmpty(err)

        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertWritten(err, ['  passes/image.html -> unexpected pass\n'])

        printer, err, out = self.get_printer(['--print', 'nothing'])
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertEmpty(err)

        printer, err, out = self.get_printer(['--print',
                                              'trace-unexpected'])
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertEmpty(err)

        printer, err, out = self.get_printer(['--print',
                                              'trace-unexpected'])
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print',
                                              'trace-unexpected'])
        result = self.get_result("passes/text.html")
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print',
                                              'trace-unexpected'])
        result = self.get_result("passes/text.html")
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print', 'trace-everything'])
        result = self.get_result('passes/image.html')
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        result = self.get_result('failures/expected/missing_text.html')
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        result = self.get_result('failures/expected/missing_check.html')
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        result = self.get_result('failures/expected/missing_image.html')
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print', 'trace-everything'])
        result = self.get_result('passes/image.html')
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')

    def test_print_progress(self):
        expectations = ''

        printer, err, out = self.get_printer(['--print', 'nothing'])
        tests = ['passes/text.html', 'failures/expected/timeout.html',
                 'failures/expected/crash.html']
        paths, rs, exp = self.get_result_summary(tests, expectations)

        # First, test that we print nothing when we shouldn't print anything.
        printer.print_progress(rs, False, paths)
        self.assertEmpty(out)
        self.assertEmpty(err)

        printer.print_progress(rs, True, paths)
        self.assertEmpty(out)
        self.assertEmpty(err)

        # Now test that we do print things.
        printer, err, out = self.get_printer(['--print', 'one-line-progress'])
        printer.print_progress(rs, False, paths)
        self.assertEmpty(out)
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print', 'one-line-progress'])
        printer.print_progress(rs, True, paths)
        self.assertEmpty(out)
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print', 'one-line-progress'])
        rs.remaining = 0
        printer.print_progress(rs, False, paths)
        self.assertEmpty(out)
        self.assertNotEmpty(err)

        printer.print_progress(rs, True, paths)
        self.assertEmpty(out)
        self.assertNotEmpty(err)



    def test_write_nothing(self):
        printer, err, out = self.get_printer(['--print', 'nothing'])
        printer.write("foo")
        self.assertEmpty(err)

    def test_write_misc(self):
        printer, err, out = self.get_printer(['--print', 'misc'])
        printer.write("foo")
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print', 'misc'])
        printer.write("foo", "config")
        self.assertEmpty(err)

    def test_write_everything(self):
        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.write("foo")
        self.assertNotEmpty(err)

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.write("foo", "config")
        self.assertNotEmpty(err)

    def test_write_verbose(self):
        printer, err, out = self.get_printer(['--verbose'])
        printer.write("foo")
        self.assertTrue("foo" in err.buflist[0])
        self.assertEmpty(out)

    def test_print_unexpected_results(self):
        # This routine is the only one that prints stuff that the bots
        # care about.
        #
        # FIXME: there's some weird layering going on here. It seems
        # like we shouldn't be both using an expectations string and
        # having to specify whether or not the result was expected.
        # This whole set of tests should probably be rewritten.
        #
        # FIXME: Plus, the fact that we're having to call into
        # run_webkit_tests is clearly a layering inversion.
        def get_unexpected_results(expected, passing, flaky):
            """Return an unexpected results summary matching the input description.

            There are a lot of different combinations of test results that
            can be tested; this routine produces various combinations based
            on the values of the input flags.

            Args
                expected: whether the tests ran as expected
                passing: whether the tests should all pass
                flaky: whether the tests should be flaky (if False, they
                    produce the same results on both runs; if True, they
                    all pass on the second run).

            """
            test_is_slow = False
            paths, rs, exp = self.get_result_summary(tests, expectations)
            if expected:
                rs.add(self.get_result('passes/text.html', test_expectations.PASS), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/timeout.html', test_expectations.TIMEOUT), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/crash.html', test_expectations.CRASH), expected, test_is_slow)
            elif passing:
                rs.add(self.get_result('passes/text.html'), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/timeout.html'), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/crash.html'), expected, test_is_slow)
            else:
                rs.add(self.get_result('passes/text.html', test_expectations.TIMEOUT), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/timeout.html', test_expectations.CRASH), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/crash.html', test_expectations.TIMEOUT), expected, test_is_slow)
            retry = rs
            if flaky:
                paths, retry, exp = self.get_result_summary(tests, expectations)
                retry.add(self.get_result('passes/text.html'), True, test_is_slow)
                retry.add(self.get_result('failures/expected/timeout.html'), True, test_is_slow)
                retry.add(self.get_result('failures/expected/crash.html'), True, test_is_slow)
            unexpected_results = manager.summarize_results(self._port, exp, rs, retry, test_timings={}, only_unexpected=True, interrupted=False)
            return unexpected_results

        tests = ['passes/text.html', 'failures/expected/timeout.html', 'failures/expected/crash.html']
        expectations = ''

        printer, err, out = self.get_printer(['--print', 'nothing'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertEmpty(out)

        printer, err, out = self.get_printer(['--print', 'unexpected-results'])

        # test everything running as expected
        ur = get_unexpected_results(expected=True, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertEmpty(out)

        # test failures
        printer, err, out = self.get_printer(['--print', 'unexpected-results'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        # test unexpected flaky
        printer, err, out = self.get_printer(['--print', 'unexpected-results'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=True)
        printer.print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        printer, err, out = self.get_printer(['--print', 'everything'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        expectations = """
BUGX : failures/expected/crash.html = CRASH
BUGX : failures/expected/timeout.html = TIMEOUT
"""
        printer, err, out = self.get_printer(['--print', 'unexpected-results'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        printer, err, out = self.get_printer(['--print', 'unexpected-results'])
        ur = get_unexpected_results(expected=False, passing=True, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        # Test handling of --verbose as well.
        printer, err, out = self.get_printer(['--verbose'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        # FIXME: debug output from the port and scm objects may or may not go
        # to stderr, so there's no point in testing its contents here.
        self.assertNotEmpty(out)

    def test_print_unexpected_results_buildbot(self):
        # FIXME: Test that print_unexpected_results() produces the printer the
        # buildbot is expecting.
        pass

if __name__ == '__main__':
    unittest.main()
