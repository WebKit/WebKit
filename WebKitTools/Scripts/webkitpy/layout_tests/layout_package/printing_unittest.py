#!/usr/bin/python
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

"""Unit tests for printing.py."""

import os
import optparse
import pdb
import sys
import unittest
import logging

from webkitpy.common import array_stream
from webkitpy.common.system import logtesting
from webkitpy.layout_tests import port
from webkitpy.layout_tests.layout_package import printing
from webkitpy.layout_tests.layout_package import dump_render_tree_thread
from webkitpy.layout_tests.layout_package import test_expectations
from webkitpy.layout_tests.layout_package import test_failures
from webkitpy.layout_tests import run_webkit_tests


def get_options(args):
    print_options = printing.print_options()
    option_parser = optparse.OptionParser(option_list=print_options)
    return option_parser.parse_args(args)


class TestUtilityFunctions(unittest.TestCase):
    def test_configure_logging(self):
        options, args = get_options([])
        stream = array_stream.ArrayStream()
        handler = printing._configure_logging(options, stream)
        logging.info("this should be logged")
        self.assertFalse(stream.empty())

        stream.reset()
        logging.debug("this should not be logged")
        self.assertTrue(stream.empty())

        printing._restore_logging(handler)

        stream.reset()
        options, args = get_options(['--verbose'])
        handler = printing._configure_logging(options, stream)
        logging.debug("this should be logged")
        self.assertFalse(stream.empty())
        printing._restore_logging(handler)

    def test_print_options(self):
        options, args = get_options([])
        self.assertTrue(options is not None)

    def test_parse_print_options(self):
        def test_switches(args, verbose, child_processes, is_fully_parallel,
                          expected_switches_str):
            options, args = get_options(args)
            if expected_switches_str:
                expected_switches = set(expected_switches_str.split(','))
            else:
                expected_switches = set()
            switches = printing.parse_print_options(options.print_options,
                                                    verbose,
                                                    child_processes,
                                                    is_fully_parallel)
            self.assertEqual(expected_switches, switches)

        # test that we default to the default set of switches
        test_switches([], False, 1, False,
                      printing.PRINT_DEFAULT)

        # test that verbose defaults to everything
        test_switches([], True, 1, False,
                      printing.PRINT_EVERYTHING)

        # test that --print default does what it's supposed to
        test_switches(['--print', 'default'], False, 1, False,
                      printing.PRINT_DEFAULT)

        # test that --print nothing does what it's supposed to
        test_switches(['--print', 'nothing'], False, 1, False,
                      None)

        # test that --print everything does what it's supposed to
        test_switches(['--print', 'everything'], False, 1, False,
                      printing.PRINT_EVERYTHING)

        # this tests that '--print X' overrides '--verbose'
        test_switches(['--print', 'actual'], True, 1, False,
                      'actual')


class  Testprinter(unittest.TestCase):
    def get_printer(self, args=None, single_threaded=False,
                   is_fully_parallel=False):
        printing_options = printing.print_options()
        option_parser = optparse.OptionParser(option_list=printing_options)
        options, args = option_parser.parse_args(args)
        self._port = port.get('test', options)
        nproc = 2
        if single_threaded:
            nproc = 1

        regular_output = array_stream.ArrayStream()
        buildbot_output = array_stream.ArrayStream()
        printer = printing.Printer(self._port, options, regular_output,
                                   buildbot_output, single_threaded,
                                   is_fully_parallel)
        return printer, regular_output, buildbot_output

    def get_result(self, test, result_type=test_expectations.PASS, run_time=0):
        failures = []
        if result_type == test_expectations.TIMEOUT:
            failures = [test_failures.FailureTimeout()]
        elif result_type == test_expectations.CRASH:
            failures = [test_failures.FailureCrash()]
        path = os.path.join(self._port.layout_tests_dir(), test)
        return dump_render_tree_thread.TestResult(path, failures, run_time,
                                                  total_time_for_all_diffs=0,
                                                  time_for_diffs=0)

    def get_result_summary(self, tests, expectations_str):
        test_paths = [os.path.join(self._port.layout_tests_dir(), test) for
                      test in tests]
        expectations = test_expectations.TestExpectations(
            self._port, test_paths, expectations_str,
            self._port.test_platform_name(), is_debug_mode=False,
            is_lint_mode=False, tests_are_present=False)

        rs = run_webkit_tests.ResultSummary(expectations, test_paths)
        return test_paths, rs, expectations

    def test_help_printer(self):
        # Here and below we'll call the "regular" printer err and the
        # buildbot printer out; this corresponds to how things run on the
        # bots with stderr and stdout.
        printer, err, out = self.get_printer()

        # This routine should print something to stdout. testing what it is
        # is kind of pointless.
        printer.help_printing()
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

    def do_switch_tests(self, method_name, switch, to_buildbot,
                        message='hello', exp_err=None, exp_bot=None):
        def do_helper(method_name, switch, message, exp_err, exp_bot):
            printer, err, bot = self.get_printer(['--print', switch])
            getattr(printer, method_name)(message)
            self.assertEqual(err.get(), exp_err)
            self.assertEqual(bot.get(), exp_bot)

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

    def test_print_actual(self):
        # Actual results need to be logged to the buildbot's stream.
        self.do_switch_tests('print_actual', 'actual', to_buildbot=True)

    def test_print_actual_buildbot(self):
        # FIXME: Test that the format of the actual results matches what the
        # buildbot is expecting.
        pass

    def test_print_config(self):
        self.do_switch_tests('print_config', 'config', to_buildbot=False)

    def test_print_expected(self):
        self.do_switch_tests('print_expected', 'expected', to_buildbot=False)

    def test_print_timing(self):
        self.do_switch_tests('print_timing', 'timing', to_buildbot=False)

    def test_print_update(self):
        # Note that there shouldn't be a carriage return here; updates()
        # are meant to be overwritten.
        self.do_switch_tests('print_update', 'updates', to_buildbot=False,
                             message='hello', exp_err=['hello'])

    def test_print_one_line_summary(self):
        printer, err, out = self.get_printer(['--print', 'nothing'])
        printer.print_one_line_summary(1, 1, 0)
        self.assertTrue(err.empty())

        printer, err, out = self.get_printer(['--print', 'one-line-summary'])
        printer.print_one_line_summary(1, 1, 0)
        self.assertEquals(err.get(), ["All 1 tests ran as expected.\n", "\n"])

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.print_one_line_summary(1, 1, 0)
        self.assertEquals(err.get(), ["All 1 tests ran as expected.\n", "\n"])

        err.reset()
        printer.print_one_line_summary(2, 1, 1)
        self.assertEquals(err.get(),
                          ["1 test ran as expected, 1 didn't:\n", "\n"])

        err.reset()
        printer.print_one_line_summary(3, 2, 1)
        self.assertEquals(err.get(),
                          ["2 tests ran as expected, 1 didn't:\n", "\n"])

        err.reset()
        printer.print_one_line_summary(3, 2, 0)
        self.assertEquals(err.get(),
                          ['\n', "2 tests ran as expected (1 didn't run).\n",
                           '\n'])


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
        self.assertTrue(err.empty())

        printer, err, out = self.get_printer(['--print', 'unexpected'])
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertTrue(err.empty())
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertEquals(err.get(),
                          ['  passes/image.html -> unexpected pass\n'])

        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertTrue(err.empty())

        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertEquals(err.get(),
                          ['  passes/image.html -> unexpected pass\n'])

        printer, err, out = self.get_printer(['--print', 'nothing'])
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertTrue(err.empty())

        printer, err, out = self.get_printer(['--print',
                                              'trace-unexpected'])
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertTrue(err.empty())

        printer, err, out = self.get_printer(['--print',
                                              'trace-unexpected'])
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertFalse(err.empty())

        printer, err, out = self.get_printer(['--print',
                                              'trace-unexpected'])
        result = self.get_result("passes/text.html")
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertFalse(err.empty())

        err.reset()
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')
        self.assertFalse(err.empty())

        printer, err, out = self.get_printer(['--print', 'trace-everything'])
        printer.print_test_result(result, expected=True, exp_str='',
                                  got_str='')
        self.assertFalse(err.empty())

        err.reset()
        printer.print_test_result(result, expected=False, exp_str='',
                                  got_str='')

    def test_print_progress(self):
        expectations = ''

        # test that we print nothing
        printer, err, out = self.get_printer(['--print', 'nothing'])
        tests = ['passes/text.html', 'failures/expected/timeout.html',
                 'failures/expected/crash.html']
        paths, rs, exp = self.get_result_summary(tests, expectations)

        printer.print_progress(rs, False, paths)
        self.assertTrue(out.empty())
        self.assertTrue(err.empty())

        printer.print_progress(rs, True, paths)
        self.assertTrue(out.empty())
        self.assertTrue(err.empty())

        # test regular functionality
        printer, err, out = self.get_printer(['--print',
                                              'one-line-progress'])
        printer.print_progress(rs, False, paths)
        self.assertTrue(out.empty())
        self.assertFalse(err.empty())

        err.reset()
        out.reset()
        printer.print_progress(rs, True, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

    def test_print_progress__detailed(self):
        tests = ['passes/text.html', 'failures/expected/timeout.html',
                 'failures/expected/crash.html']
        expectations = 'failures/expected/timeout.html = TIMEOUT'

        # first, test that it is disabled properly
        # should still print one-line-progress
        printer, err, out = self.get_printer(
            ['--print', 'detailed-progress'], single_threaded=False)
        paths, rs, exp = self.get_result_summary(tests, expectations)
        printer.print_progress(rs, False, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        # now test the enabled paths
        printer, err, out = self.get_printer(
            ['--print', 'detailed-progress'], single_threaded=True)
        paths, rs, exp = self.get_result_summary(tests, expectations)
        printer.print_progress(rs, False, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        err.reset()
        out.reset()
        printer.print_progress(rs, True, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        rs.add(self.get_result('passes/text.html', test_expectations.TIMEOUT), False)
        rs.add(self.get_result('failures/expected/timeout.html'), True)
        rs.add(self.get_result('failures/expected/crash.html', test_expectations.CRASH), True)
        err.reset()
        out.reset()
        printer.print_progress(rs, False, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        # We only clear the meter when retrying w/ detailed-progress.
        err.reset()
        out.reset()
        printer.print_progress(rs, True, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        printer, err, out = self.get_printer(
            ['--print', 'detailed-progress,unexpected'], single_threaded=True)
        paths, rs, exp = self.get_result_summary(tests, expectations)
        printer.print_progress(rs, False, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        err.reset()
        out.reset()
        printer.print_progress(rs, True, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        rs.add(self.get_result('passes/text.html', test_expectations.TIMEOUT), False)
        rs.add(self.get_result('failures/expected/timeout.html'), True)
        rs.add(self.get_result('failures/expected/crash.html', test_expectations.CRASH), True)
        err.reset()
        out.reset()
        printer.print_progress(rs, False, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

        # We only clear the meter when retrying w/ detailed-progress.
        err.reset()
        out.reset()
        printer.print_progress(rs, True, paths)
        self.assertFalse(err.empty())
        self.assertTrue(out.empty())

    def test_write_nothing(self):
        printer, err, out = self.get_printer(['--print', 'nothing'])
        printer.write("foo")
        self.assertTrue(err.empty())

    def test_write_misc(self):
        printer, err, out = self.get_printer(['--print', 'misc'])
        printer.write("foo")
        self.assertFalse(err.empty())
        err.reset()
        printer.write("foo", "config")
        self.assertTrue(err.empty())

    def test_write_everything(self):
        printer, err, out = self.get_printer(['--print', 'everything'])
        printer.write("foo")
        self.assertFalse(err.empty())
        err.reset()
        printer.write("foo", "config")
        self.assertFalse(err.empty())

    def test_write_verbose(self):
        printer, err, out = self.get_printer(['--verbose'])
        printer.write("foo")
        self.assertTrue(not err.empty() and "foo" in err.get()[0])
        self.assertTrue(out.empty())

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
            paths, rs, exp = self.get_result_summary(tests, expectations)
            if expected:
                rs.add(self.get_result('passes/text.html', test_expectations.PASS),
                       expected)
                rs.add(self.get_result('failures/expected/timeout.html',
                       test_expectations.TIMEOUT), expected)
                rs.add(self.get_result('failures/expected/crash.html', test_expectations.CRASH),
                   expected)
            elif passing:
                rs.add(self.get_result('passes/text.html'), expected)
                rs.add(self.get_result('failures/expected/timeout.html'), expected)
                rs.add(self.get_result('failures/expected/crash.html'), expected)
            else:
                rs.add(self.get_result('passes/text.html', test_expectations.TIMEOUT),
                       expected)
                rs.add(self.get_result('failures/expected/timeout.html',
                       test_expectations.CRASH), expected)
                rs.add(self.get_result('failures/expected/crash.html',
                                  test_expectations.TIMEOUT),
                   expected)
            retry = rs
            if flaky:
                paths, retry, exp = self.get_result_summary(tests,
                                                expectations)
                retry.add(self.get_result('passes/text.html'), True)
                retry.add(self.get_result('failures/expected/timeout.html'), True)
                retry.add(self.get_result('failures/expected/crash.html'), True)
            unexpected_results = run_webkit_tests.summarize_unexpected_results(
                self._port, exp, rs, retry)
            return unexpected_results

        tests = ['passes/text.html', 'failures/expected/timeout.html',
                 'failures/expected/crash.html']
        expectations = ''

        printer, err, out = self.get_printer(['--print', 'nothing'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertTrue(out.empty())

        printer, err, out = self.get_printer(['--print',
                                              'unexpected-results'])

        # test everything running as expected
        ur = get_unexpected_results(expected=True, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertTrue(out.empty())

        # test failures
        err.reset()
        out.reset()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertFalse(out.empty())

        # test unexpected flaky results
        err.reset()
        out.reset()
        ur = get_unexpected_results(expected=False, passing=True, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertFalse(out.empty())

        # test unexpected passes
        err.reset()
        out.reset()
        ur = get_unexpected_results(expected=False, passing=False, flaky=True)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertFalse(out.empty())

        err.reset()
        out.reset()
        printer, err, out = self.get_printer(['--print', 'everything'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertFalse(out.empty())

        expectations = """
failures/expected/crash.html = CRASH
failures/expected/timeout.html = TIMEOUT
"""
        err.reset()
        out.reset()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertFalse(out.empty())

        err.reset()
        out.reset()
        ur = get_unexpected_results(expected=False, passing=True, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertFalse(out.empty())

        # Test handling of --verbose as well.
        err.reset()
        out.reset()
        printer, err, out = self.get_printer(['--verbose'])
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertTrue(err.empty())
        self.assertFalse(out.empty())

    def test_print_unexpected_results_buildbot(self):
        # FIXME: Test that print_unexpected_results() produces the printer the
        # buildbot is expecting.
        pass

if __name__ == '__main__':
    unittest.main()
