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
import sys
import types
import unittest

from webkitcorepy import StringIO

from webkitpy.common.host_mock import MockHost
from webkitpy.common.program_status_mock import TTYStringIO, encoded
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
        self.assertIsNotNone(options)


class  Testprinter(unittest.TestCase):
    def assertEmpty(self, stream):
        self.assertFalse(stream.getvalue())

    def assertNotEmpty(self, stream):
        self.assertTrue(stream.getvalue())

    def assertWritten(self, stream, contents):
        self.assertEqual(stream.getvalue(), contents)

    def reset(self, stream):
        stream.truncate(0)
        stream.seek(0)

    def get_printer(self, args=None, regular_output=None):
        args = args or []
        printing_options = printing.print_options()
        option_parser = optparse.OptionParser(option_list=printing_options)
        options, args = option_parser.parse_args(args)
        host = MockHost()
        self._port = host.port_factory.get('test', options)
        nproc = 2

        regular_output = regular_output or StringIO()
        printer = printing.Printer(self._port, options, regular_output)
        return printer, regular_output

    def get_result(self, test_name, result_type=test_expectations.PASS, run_time=0):
        failures = []
        if result_type == test_expectations.TIMEOUT:
            failures = [test_failures.FailureTimeout()]
        elif result_type == test_expectations.CRASH:
            failures = [test_failures.FailureCrash()]
        return test_results.TestResult(test_name, failures=failures, test_run_time=run_time)

    def test_configure_and_cleanup(self):
        # This test verifies that calling cleanup repeatedly and deleting
        # the object is safe.
        printer, err = self.get_printer()
        printer.cleanup()
        printer.cleanup()
        printer = None

    def test_print_config(self):
        printer, err = self.get_printer()
        # FIXME: it's lame that i have to set these options directly.
        printer._options.pixel_tests = True
        printer._options.new_baseline = True
        printer._options.time_out_ms = 6000
        printer._options.slow_time_out_ms = 12000
        printer._options.additional_header = 'useEphemeralSession=false'
        printer.print_config('/tmp')
        self.assertIn("Using port 'test-mac-leopard'", err.getvalue())
        self.assertIn('Test configuration: <leopard, x86, release>', err.getvalue())
        self.assertIn('Placing test results in /tmp', err.getvalue())
        self.assertIn('Using Release build', err.getvalue())
        self.assertIn('Using additional header: ', err.getvalue())
        self.assertIn('Pixel tests enabled', err.getvalue())
        self.assertIn('Command line:', err.getvalue())
        self.assertIn('Regular timeout: ', err.getvalue())

    def test_print_baseline_search_path(self):
        printer, err = self.get_printer()
        printer.print_baseline_search_path()

        self.assertIn('Verbose baseline search path: platform/test-mac-leopard -> platform/test-mac-snowleopard -> generic', err.getvalue())
        self.assertIn('Baseline search path: platform/test-mac-leopard -> generic', err.getvalue())

        self.assertIn('Verbose test expectations: platform/test-wk2/TestExpectations -> platform/wk2/TestExpectations -> platform/test-mac-leopard/TestExpectations -> platform/test/TestExpectations -> TestExpectations', err.getvalue())
        self.assertIn('Test expectations: platform/test/TestExpectations', err.getvalue())

        self.reset(err)
        printer._options.quiet = True
        printer.print_baseline_search_path()
        self.assertNotIn('Baseline search path: platform/test-mac-leopard -> platform/test-mac-snowleopard -> generic', err.getvalue())

    def test_print_one_line_summary(self):
        printer, err = self.get_printer()
        printer._print_one_line_summary(1, 1, 0)
        self.assertWritten(err, "The test ran as expected.\n\n")

        printer, err = self.get_printer()
        printer._print_one_line_summary(1, 1, 0)
        self.assertWritten(err, "The test ran as expected.\n\n")

        printer, err = self.get_printer()
        printer._print_one_line_summary(2, 1, 1)
        self.assertWritten(err, "\n1 test ran as expected, 1 didn't:\n\n")

        printer, err = self.get_printer()
        printer._print_one_line_summary(3, 2, 1)
        self.assertWritten(err, "\n2 tests ran as expected, 1 didn't:\n\n")

        printer, err = self.get_printer()
        printer._print_one_line_summary(3, 2, 0)
        self.assertWritten(err, "\n2 tests ran as expected (1 didn't run).\n\n")

    def test_test_status_line(self):
        printer, _ = self.get_printer()
        printer._meter.number_of_columns = lambda: 80
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed')
        self.assertEqual(80, len(actual))
        self.assertEqual(actual, '[0/0] fast/dom/HTMLFormElement/associa...after-index-assertion-fail1.html passed')

        printer._meter.number_of_columns = lambda: 80
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed', truncate=False)
        self.assertEqual(90, len(actual))
        self.assertEqual(actual, '[0/0] fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html passed')

        printer._meter.number_of_columns = lambda: 89
        actual = printer._test_status_line('fast/dom/HTMLFormElement/associated-elements-after-index-assertion-fail1.html', ' passed')
        self.assertEqual(89, len(actual))
        self.assertEqual(actual, '[0/0] fast/dom/HTMLFormElement/associated-...ents-after-index-assertion-fail1.html passed')

        printer._meter.number_of_columns = lambda: sys.maxsize
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
        printer, err = self.get_printer(['--details'])
        result = self.get_result('passes/image.html')
        printer.print_started_test('passes/image.html')
        printer.print_finished_test(result, expected=False, exp_str='', got_str='')
        self.assertNotEmpty(err)

    def test_program_status_reports_progress_as_tests_finish(self):
        printer, err = self.get_printer(regular_output=TTYStringIO())
        printer.begin_tests(4)
        for test_name in ('passes/a.html', 'passes/b.html'):
            printer.print_started_test(test_name)
            printer.print_finished_test(self.get_result(test_name), expected=True, exp_str='', got_str='')
        self.assertEqual(err.program_status_reports(), [
            'state=working:progress=0:app=run-webkit-tests',
            'state=working:progress=25:app=run-webkit-tests',
            'state=working:progress=50:app=run-webkit-tests',
        ])

    def test_program_status_restarts_progress_for_each_run(self):
        printer, err = self.get_printer(regular_output=TTYStringIO())
        printer.begin_tests(1)
        printer.print_started_test('passes/a.html')
        printer.print_finished_test(self.get_result('passes/a.html'), expected=True, exp_str='', got_str='')
        printer.begin_tests(2)
        self.assertEqual(err.program_status_reports()[-2:], [
            'state=working:progress=100:app=run-webkit-tests',
            'state=working:progress=0:app=run-webkit-tests',
        ])

    def test_program_status_reports_phases_before_tests_start(self):
        printer, err = self.get_printer(regular_output=TTYStringIO())
        printer.write_update('Collecting tests ...')
        self.assertEqual(err.program_status_reports(), ['state=working:app=run-webkit-tests:msg=' + encoded('Collecting tests ...')])

    def test_program_status_reports_unexpected_results_so_far(self):
        printer, err = self.get_printer(regular_output=TTYStringIO())
        printer.begin_tests(3)
        printer.print_started_test('passes/a.html')
        printer.print_finished_test(self.get_result('passes/a.html'), expected=False, exp_str='', got_str='')
        self.assertEqual(err.program_status_reports(), [
            'state=working:progress=0:app=run-webkit-tests',
            'state=working:progress=33:app=run-webkit-tests:msg=' + encoded('1 unexpected result'),
        ])

    def test_program_status_labels_the_retry_run(self):
        printer, err = self.get_printer(regular_output=TTYStringIO())
        printer.begin_tests(2, retrying=True)
        self.assertEqual(err.program_status_reports(), ['state=working:progress=0:app=run-webkit-tests:msg=' + encoded('Retrying 2 tests')])

    def test_program_status_ignores_updates_after_the_run_finished(self):
        printer, err = self.get_printer(regular_output=TTYStringIO())
        printer._report_finished(types.SimpleNamespace(keyboard_interrupted=False, interrupted=False, unexpected=0, expected=1, expected_skips=0))
        printer.write_update('Uploading results ...')
        self.assertEqual(err.program_status_reports(), ['state=done:app=run-webkit-tests:msg=' + encoded('1 test ran as expected')])

    def test_program_status_is_silent_without_a_tty(self):
        printer, err = self.get_printer()
        printer.begin_tests(1)
        printer.print_started_test('passes/a.html')
        printer.print_finished_test(self.get_result('passes/a.html'), expected=True, exp_str='', got_str='')
        self.assertNotIn('\x1b]7501;', err.getvalue())

    def test_program_status_goes_idle_on_cleanup_if_the_run_never_finished(self):
        printer, err = self.get_printer(regular_output=TTYStringIO())
        printer.begin_tests(2)
        printer.cleanup()
        self.assertEqual(err.program_status_reports(), [
            'state=working:progress=0:app=run-webkit-tests',
            'state=idle:app=run-webkit-tests',
        ])

    def test_program_status_reports_how_the_run_finished(self):
        def run_results(**overrides):
            return types.SimpleNamespace(**{'keyboard_interrupted': False, 'interrupted': False, 'unexpected': 0, 'expected': 5, 'expected_skips': 1, **overrides})

        cases = [
            (run_results(), 'state=done:app=run-webkit-tests:msg=' + encoded('4 tests ran as expected')),
            (run_results(unexpected=1), 'state=error:app=run-webkit-tests:msg=' + encoded('1 unexpected result')),
            (run_results(unexpected=2, interrupted=True), 'state=error:app=run-webkit-tests:msg=' + encoded('2 unexpected results, exited early')),
            (run_results(keyboard_interrupted=True), 'state=idle:app=run-webkit-tests'),
        ]
        for results, expected_report in cases:
            with self.subTest(expected_report=expected_report):
                printer, err = self.get_printer(regular_output=TTYStringIO())
                printer._report_finished(results)
                self.assertEqual(err.program_status_reports(), [expected_report])

    def test_print_found(self):
        printer, err = self.get_printer()

        printer.print_found(100, 10, 1, 1)
        self.assertWritten(err, "Found 100 tests; running 10, skipping 90.\n")

        self.reset(err)
        printer.print_found(100, 10, 2, 3)
        self.assertWritten(err, "Found 100 tests; running 10 (6 times each: --repeat-each=2 --iterations=3), skipping 90.\n")
