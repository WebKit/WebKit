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
import sys
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


class  Testprinter(unittest.TestCase):
    def assertEmpty(self, stream):
        self.assertFalse(stream.getvalue())

    def assertNotEmpty(self, stream):
        self.assertTrue(stream.getvalue())

    def assertWritten(self, stream, contents):
        self.assertEqual(stream.buflist, contents)

    def reset(self, stream):
        stream.buflist = []
        stream.buf = ''

    def get_printer(self, args=None):
        args = args or []
        printing_options = printing.print_options()
        option_parser = optparse.OptionParser(option_list=printing_options)
        options, args = option_parser.parse_args(args)
        host = MockHost()
        self._port = host.port_factory.get('test', options)
        nproc = 2

        regular_output = StringIO.StringIO()
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

        rs = result_summary.ResultSummary(expectations, len(test_names))
        return test_names, rs, expectations

    def test_configure_and_cleanup(self):
        # This test verifies that calling cleanup repeatedly and deleting
        # the object is safe.
        printer, err, out = self.get_printer()
        printer.cleanup()
        printer.cleanup()
        printer = None

    def test_print_config(self):
        printer, err, out = self.get_printer()
        # FIXME: it's lame that i have to set these options directly.
        printer._options.pixel_tests = True
        printer._options.new_baseline = True
        printer._options.time_out_ms = 6000
        printer._options.slow_time_out_ms = 12000
        printer.print_config('/tmp')
        self.assertTrue("Using port 'test-mac-leopard'" in err.getvalue())
        self.assertTrue('Test configuration: <leopard, x86, release>' in err.getvalue())
        self.assertTrue('Placing test results in /tmp' in err.getvalue())
        self.assertTrue('Baseline search path: test-mac-leopard -> test-mac-snowleopard -> generic' in err.getvalue())
        self.assertTrue('Using Release build' in err.getvalue())
        self.assertTrue('Pixel tests enabled' in err.getvalue())
        self.assertTrue('Command line:' in err.getvalue())
        self.assertTrue('Regular timeout: ' in err.getvalue())

        self.reset(err)
        printer._options.quiet = True
        printer.print_config('/tmp')
        self.assertFalse('Baseline search path: test-mac-leopard -> test-mac-snowleopard -> generic' in err.getvalue())

    def test_print_one_line_summary(self):
        printer, err, out = self.get_printer()
        printer._print_one_line_summary(1, 1, 0)
        self.assertWritten(err, ["The test ran as expected.\n", "\n"])

        printer, err, out = self.get_printer()
        printer._print_one_line_summary(1, 1, 0)
        self.assertWritten(err, ["The test ran as expected.\n", "\n"])

        printer, err, out = self.get_printer()
        printer._print_one_line_summary(2, 1, 1)
        self.assertWritten(err, ["\n", "1 test ran as expected, 1 didn't:\n", "\n"])

        printer, err, out = self.get_printer()
        printer._print_one_line_summary(3, 2, 1)
        self.assertWritten(err, ["\n", "2 tests ran as expected, 1 didn't:\n", "\n"])

        printer, err, out = self.get_printer()
        printer._print_one_line_summary(3, 2, 0)
        self.assertWritten(err, ['\n', "2 tests ran as expected (1 didn't run).\n", '\n'])

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
            retry = None
            if flaky:
                paths, retry, exp = self.get_result_summary(tests, expectations)
                retry.add(self.get_result('passes/text.html'), True, test_is_slow)
                retry.add(self.get_result('failures/expected/timeout.html'), True, test_is_slow)
                retry.add(self.get_result('failures/expected/crash.html'), True, test_is_slow)
            return manager.summarize_results(self._port, exp, rs, retry)

        tests = ['passes/text.html', 'failures/expected/timeout.html', 'failures/expected/crash.html']
        expectations = ''

        printer, err, out = self.get_printer()

        # test everything running as expected
        ur = get_unexpected_results(expected=True, passing=False, flaky=False)
        printer._print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertEmpty(out)

        # test failures
        printer, err, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer._print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        # test unexpected flaky
        printer, err, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=True)
        printer._print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        printer, err, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer._print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        expectations = """
BUGX : failures/expected/crash.html = CRASH
BUGX : failures/expected/timeout.html = TIMEOUT
"""
        printer, err, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer._print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

        printer, err, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=True, flaky=False)
        printer._print_unexpected_results(ur)
        self.assertEmpty(err)
        self.assertNotEmpty(out)

    def test_print_unexpected_results_buildbot(self):
        # FIXME: Test that print_unexpected_results() produces the printer the
        # buildbot is expecting.
        pass

    def test_test_status_line(self):
        printer, _, _ = self.get_printer()
        printer._meter.number_of_columns = lambda: 80
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed')
        self.assertEqual(80, len(actual))
        self.assertEqual(actual, '[0/0] fast/dom/HTMLFormElement/associa...after-index-assertion-fail1.html passed')

        printer._meter.number_of_columns = lambda: 89
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed')
        self.assertEqual(89, len(actual))
        self.assertEqual(actual, '[0/0] fast/dom/HTMLFormElement/associated-...ents-after-index-assertion-fail1.html passed')

        printer._meter.number_of_columns = lambda: sys.maxint
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed')
        self.assertEqual(90, len(actual))
        self.assertEqual(actual, '[0/0] fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html passed')

        printer._meter.number_of_columns = lambda: 18
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed')
        self.assertEqual(18, len(actual))
        self.assertEqual(actual, '[0/0] f...l passed')

        printer._meter.number_of_columns = lambda: 10
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed')
        self.assertEqual(actual, '[0/0] associated-elements-after-index-assertion-fail1.html passed')

    def test_details(self):
        printer, err, _ = self.get_printer(['--details'])
        result = self.get_result('passes/image.html')
        printer.print_started_test('passes/image.html')
        printer.print_finished_test(result, expected=False, exp_str='', got_str='')
        self.assertNotEmpty(err)
