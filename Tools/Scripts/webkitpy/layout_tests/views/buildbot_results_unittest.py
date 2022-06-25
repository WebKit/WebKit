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

import unittest

from webkitcorepy import StringIO

from webkitpy.common.host_mock import MockHost
from webkitpy.tool.mocktool import MockOptions
from webkitpy.layout_tests.models import test_run_results
from webkitpy.layout_tests.models import test_run_results_unittest
from webkitpy.layout_tests.views import buildbot_results


class BuildBotPrinterTests(unittest.TestCase):
    def assertEmpty(self, stream):
        self.assertFalse(stream.getvalue())

    def assertNotEmpty(self, stream):
        self.assertTrue(stream.getvalue())

    def get_printer(self):
        stream = StringIO()
        printer = buildbot_results.BuildBotPrinter(stream, debug_logging=True)
        return printer, stream

    def test_print_unexpected_results(self):
        port = MockHost().port_factory.get('test', options=MockOptions(pixel_tests=False, world_leaks=False))
        printer, out = self.get_printer()

        # test everything running as expected
        DASHED_LINE = "-" * 78 + "\n"
        summary = test_run_results_unittest.summarized_results(port, expected=True, passing=False, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertEqual(out.getvalue(), DASHED_LINE)

        # test failures
        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertNotEmpty(out)

        # test unexpected flaky
        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=False, passing=False, flaky=True)
        printer.print_unexpected_results(summary)
        self.assertNotEmpty(out)

        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertNotEmpty(out)

        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=False, passing=False, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertNotEmpty(out)

        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=False, passing=True, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertNotEmpty(out)

    def test_print_unexpected_results_with_options(self):
        port = MockHost().port_factory.get('test', options=MockOptions(pixel_tests=False, world_leaks=False))

        DASHED_LINE = "-" * 78 + "\n"

        port._options.pixel_tests = True
        port._options.world_leaks = False
        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=True, passing=False, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertEqual(out.getvalue(), DASHED_LINE)

        port._options.pixel_tests = False
        port._options.world_leaks = True
        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=True, passing=False, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertEqual(out.getvalue(), DASHED_LINE)

        port._options.pixel_tests = True
        port._options.world_leaks = True
        printer, out = self.get_printer()
        summary = test_run_results_unittest.summarized_results(port, expected=True, passing=False, flaky=False)
        printer.print_unexpected_results(summary)
        self.assertEqual(out.getvalue(), DASHED_LINE)

    def test_print_results(self):
        port = MockHost().port_factory.get('test', options=MockOptions(pixel_tests=False, world_leaks=False))
        printer, out = self.get_printer()
        initial_results = test_run_results_unittest.run_results(port)
        summary = test_run_results_unittest.summarized_results(port, expected=False, passing=True, flaky=False)
        details = test_run_results.RunDetails(summary['num_regressions'], summary, initial_results, None)
        printer.print_results(details)
        self.assertNotEmpty(out)
