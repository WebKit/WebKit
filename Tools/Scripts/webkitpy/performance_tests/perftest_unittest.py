#!/usr/bin/python
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
import math
import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.port.driver import DriverOutput
from webkitpy.performance_tests.perftest import ChromiumStylePerfTest
from webkitpy.performance_tests.perftest import PageLoadingPerfTest
from webkitpy.performance_tests.perftest import PerfTest
from webkitpy.performance_tests.perftest import PerfTestFactory


class MainTest(unittest.TestCase):
    def test_parse_output(self):
        output = DriverOutput('\n'.join([
            'Running 20 times',
            'Ignoring warm-up run (1115)',
            '',
            'avg 1100',
            'median 1101',
            'stdev 11',
            'min 1080',
            'max 1120']), image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest('some-test', '/path/some-dir/some-test')
            self.assertEqual(test.parse_output(output),
                {'some-test': {'avg': 1100.0, 'median': 1101.0, 'min': 1080.0, 'max': 1120.0, 'stdev': 11.0, 'unit': 'ms'}})
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'RESULT some-test= 1100.0 ms\nmedian= 1101.0 ms, stdev= 11.0 ms, min= 1080.0 ms, max= 1120.0 ms\n')

    def test_parse_output_with_failing_line(self):
        output = DriverOutput('\n'.join([
            'Running 20 times',
            'Ignoring warm-up run (1115)',
            '',
            'some-unrecognizable-line',
            '',
            'avg 1100',
            'median 1101',
            'stdev 11',
            'min 1080',
            'max 1120']), image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest('some-test', '/path/some-dir/some-test')
            self.assertEqual(test.parse_output(output), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'some-unrecognizable-line\n')


class TestPageLoadingPerfTest(unittest.TestCase):
    class MockDriver(object):
        def __init__(self, values):
            self._values = values
            self._index = 0

        def run_test(self, input):
            value = self._values[self._index]
            self._index += 1
            if isinstance(value, str):
                return DriverOutput('some output', image=None, image_hash=None, audio=None, error=value)
            else:
                return DriverOutput('some output', image=None, image_hash=None, audio=None, test_time=self._values[self._index - 1])

    def test_run(self):
        test = PageLoadingPerfTest('some-test', '/path/some-dir/some-test')
        driver = TestPageLoadingPerfTest.MockDriver([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20])
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            self.assertEqual(test.run(driver, None),
                {'some-test': {'max': 20000, 'avg': 11000.0, 'median': 11000, 'stdev': math.sqrt(570 * 1000 * 1000), 'min': 2000, 'unit': 'ms'}})
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'RESULT some-test= 11000.0 ms\nmedian= 11000 ms, stdev= 23874.6727726 ms, min= 2000 ms, max= 20000 ms\n')

    def test_run_with_bad_output(self):
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PageLoadingPerfTest('some-test', '/path/some-dir/some-test')
            driver = TestPageLoadingPerfTest.MockDriver([1, 2, 3, 4, 5, 6, 7, 'some error', 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20])
            self.assertEqual(test.run(driver, None), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'error: some-test\nsome error\n')


class TestPerfTestFactory(unittest.TestCase):
    def test_regular_test(self):
        test = PerfTestFactory.create_perf_test('some-dir/some-test', '/path/some-dir/some-test')
        self.assertEqual(test.__class__, PerfTest)

    def test_inspector_test(self):
        test = PerfTestFactory.create_perf_test('inspector/some-test', '/path/inspector/some-test')
        self.assertEqual(test.__class__, ChromiumStylePerfTest)

    def test_page_loading_test(self):
        test = PerfTestFactory.create_perf_test('PageLoad/some-test', '/path/PageLoad/some-test')
        self.assertEqual(test.__class__, PageLoadingPerfTest)


if __name__ == '__main__':
    unittest.main()
