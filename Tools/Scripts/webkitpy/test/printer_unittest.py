# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import unittest

from webkitcorepy import StringIO

from webkitpy.test.printer import Printer
from webkitpy.tool.mocktool import MockOptions


class PrinterTest(unittest.TestCase):
    def setUp(self):
        # Printer.configure() emits an INFO log message to the root logger.
        # Remove existing handlers so it doesn't appear in test-webkitpy's output.
        root_logger = logging.getLogger()
        handlers = root_logger.handlers[:]
        for handler in handlers:
            root_logger.removeHandler(handler)
        self.addCleanup(lambda: [root_logger.addHandler(h) for h in handlers])

    def _make_printer(self, num_tests=1):
        options = MockOptions(verbose=0, timing=False, quiet=False, pass_through=False)
        stream = StringIO()
        printer = Printer(stream, options)
        printer.num_tests = num_tests
        stream.truncate(0)
        stream.seek(0)
        return printer, stream

    def _drive(self, printer, stream, failures=None, errors=None, expected_failures=None, unexpected_successes=None):
        printer.print_started_test(None, 'test1')
        stream.truncate(0)
        stream.seek(0)
        printer.print_finished_test(None, 'test1', 0.0, failures or [], errors or [], expected_failures, unexpected_successes)
        finished_output = stream.getvalue()
        stream.truncate(0)
        stream.seek(0)
        printer.print_result(0.0)
        result_output = stream.getvalue()
        return finished_output, result_output

    def test_passed(self):
        printer, stream = self._make_printer()
        finished_output, result_output = self._drive(printer, stream)
        self.assertEqual(finished_output, '')
        self.assertEqual(printer.num_started, 1)
        self.assertEqual(printer.num_failures, 0)
        self.assertEqual(printer.num_errors, 0)
        self.assertEqual(result_output, 'Ran 1 test in 0.000s\n\nOK\n')

    def test_failed(self):
        printer, stream = self._make_printer()
        finished_output, result_output = self._drive(printer, stream, failures=['boom'])
        self.assertEqual(finished_output, '[1/1] test1 failed:\n  boom\n')
        self.assertEqual(printer.num_failures, 1)
        self.assertEqual(printer.num_errors, 0)
        self.assertEqual(result_output, 'Ran 1 test in 0.000s\nFAILED (failures=1, errors=0)\n')

    def test_errored(self):
        printer, stream = self._make_printer()
        finished_output, result_output = self._drive(printer, stream, errors=['boom'])
        self.assertEqual(finished_output, '[1/1] test1 erred:\n  boom\n  \n')
        self.assertEqual(printer.num_failures, 0)
        self.assertEqual(printer.num_errors, 1)
        self.assertEqual(result_output, 'Ran 1 test in 0.000s\nFAILED (failures=0, errors=1)\n')

    def test_unexpected_success(self):
        printer, stream = self._make_printer()
        finished_output, result_output = self._drive(printer, stream, unexpected_successes=['test1'])
        self.assertEqual(finished_output, '[1/1] test1 failed:\n  UNEXPECTED SUCCESS\n')
        self.assertEqual(printer.num_failures, 1)
        self.assertEqual(printer.num_errors, 0)
        self.assertEqual(result_output, 'Ran 1 test in 0.000s\nFAILED (failures=1, errors=0)\n')

    def test_expected_failure(self):
        printer, stream = self._make_printer()
        finished_output, result_output = self._drive(printer, stream, expected_failures=['test1'])
        self.assertEqual(finished_output, '[1/1] test1 expected failure\n')
        self.assertEqual(printer.num_failures, 0)
        self.assertEqual(printer.num_errors, 0)
        self.assertEqual(result_output, 'Ran 1 test in 0.000s\n\nOK\n')

    def test_failed_with_expected_failures_list(self):
        printer, stream = self._make_printer()
        finished_output, result_output = self._drive(printer, stream, failures=['boom'], expected_failures=['test1'])
        self.assertEqual(finished_output, '[1/1] test1 failed:\n  boom\n')
        self.assertEqual(printer.num_failures, 1)
        self.assertEqual(result_output, 'Ran 1 test in 0.000s\nFAILED (failures=1, errors=0)\n')
