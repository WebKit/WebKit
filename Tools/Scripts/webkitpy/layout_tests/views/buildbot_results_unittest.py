#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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

import StringIO
import unittest

from webkitpy.common.host_mock import MockHost

from webkitpy.layout_tests.controllers import manager
from webkitpy.layout_tests.models import result_summary
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.views import buildbot_results


class BuildBotPrinterTests(unittest.TestCase):
    def assertEmpty(self, stream):
        self.assertFalse(stream.getvalue())

    def assertNotEmpty(self, stream):
        self.assertTrue(stream.getvalue())

    def get_printer(self):
        stream = StringIO.StringIO()
        printer = buildbot_results.BuildBotPrinter(stream, debug_logging=True)
        return printer, stream

    def get_result(self, test_name, result_type=test_expectations.PASS, run_time=0):
        failures = []
        if result_type == test_expectations.TIMEOUT:
            failures = [test_failures.FailureTimeout()]
        elif result_type == test_expectations.CRASH:
            failures = [test_failures.FailureCrash()]
        elif result_type == test_expectations.AUDIO:
            failures = [test_failures.FailureAudioMismatch()]
        return test_results.TestResult(test_name, failures=failures, test_run_time=run_time)

    def get_result_summary(self, port, test_names, expectations_str):
        port.test_expectations = lambda: expectations_str
        port.test_expectations_overrides = lambda: None
        expectations = test_expectations.TestExpectations(port, test_names)
        rs = result_summary.ResultSummary(expectations, len(test_names))
        return test_names, rs, expectations

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
        tests = ['passes/text.html', 'failures/expected/timeout.html', 'failures/expected/crash.html', 'failures/expected/audio.html']
        expectations = ''

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
            port = MockHost().port_factory.get('test')
            test_is_slow = False
            paths, rs, exp = self.get_result_summary(port, tests, expectations)
            if expected:
                rs.add(self.get_result('passes/text.html', test_expectations.PASS), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/audio.html', test_expectations.AUDIO), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/timeout.html', test_expectations.TIMEOUT), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/crash.html', test_expectations.CRASH), expected, test_is_slow)
            elif passing:
                rs.add(self.get_result('passes/text.html'), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/audio.html'), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/timeout.html'), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/crash.html'), expected, test_is_slow)
            else:
                rs.add(self.get_result('passes/text.html', test_expectations.TIMEOUT), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/audio.html', test_expectations.CRASH), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/timeout.html', test_expectations.CRASH), expected, test_is_slow)
                rs.add(self.get_result('failures/expected/crash.html', test_expectations.TIMEOUT), expected, test_is_slow)
            retry = rs
            if flaky:
                paths, retry, exp = self.get_result_summary(port, tests, expectations)
                retry.add(self.get_result('passes/text.html'), True, test_is_slow)
                retry.add(self.get_result('failures/expected/audio.html'), True, test_is_slow)
                retry.add(self.get_result('failures/expected/timeout.html'), True, test_is_slow)
                retry.add(self.get_result('failures/expected/crash.html'), True, test_is_slow)
            return manager.summarize_results(port, exp, rs, retry)

        printer, out = self.get_printer()

        # test everything running as expected
        DASHED_LINE = "-" * 78 + "\n"
        ur = get_unexpected_results(expected=True, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertEqual(out.getvalue(), DASHED_LINE)

        # test failures
        printer, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertNotEmpty(out)

        # test unexpected flaky
        printer, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=True)
        printer.print_unexpected_results(ur)
        self.assertNotEmpty(out)

        printer, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertNotEmpty(out)

        expectations = """
BUGX : failures/expected/crash.html = CRASH
BUGX : failures/expected/timeout.html = TIMEOUT
"""
        printer, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertNotEmpty(out)

        printer, out = self.get_printer()
        ur = get_unexpected_results(expected=False, passing=True, flaky=False)
        printer.print_unexpected_results(ur)
        self.assertNotEmpty(out)
