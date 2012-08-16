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

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.port.driver import DriverOutput
from webkitpy.layout_tests.port.test import TestDriver
from webkitpy.layout_tests.port.test import TestPort
from webkitpy.performance_tests.perftest import ChromiumStylePerfTest
from webkitpy.performance_tests.perftest import PageLoadingPerfTest
from webkitpy.performance_tests.perftest import PerfTest
from webkitpy.performance_tests.perftest import PerfTestFactory
from webkitpy.performance_tests.perftest import ReplayPerfTest


class MainTest(unittest.TestCase):
    def test_parse_output(self):
        output = DriverOutput('\n'.join([
            'Running 20 times',
            'Ignoring warm-up run (1115)',
            '',
            'Time:',
            'avg 1100 ms',
            'median 1101 ms',
            'stdev 11 ms',
            'min 1080 ms',
            'max 1120 ms']), image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(None, 'some-test', '/path/some-dir/some-test')
            self.assertEqual(test.parse_output(output),
                {'some-test': {'avg': 1100.0, 'median': 1101.0, 'min': 1080.0, 'max': 1120.0, 'stdev': 11.0, 'unit': 'ms'}})
        finally:
            pass
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
            'Time:'
            'avg 1100 ms',
            'median 1101 ms',
            'stdev 11 ms',
            'min 1080 ms',
            'max 1120 ms']), image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(None, 'some-test', '/path/some-dir/some-test')
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

        def run_test(self, input, stop_when_done):
            value = self._values[self._index]
            self._index += 1
            if isinstance(value, str):
                return DriverOutput('some output', image=None, image_hash=None, audio=None, error=value)
            else:
                return DriverOutput('some output', image=None, image_hash=None, audio=None, test_time=self._values[self._index - 1])

    def test_run(self):
        test = PageLoadingPerfTest(None, 'some-test', '/path/some-dir/some-test')
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
            test = PageLoadingPerfTest(None, 'some-test', '/path/some-dir/some-test')
            driver = TestPageLoadingPerfTest.MockDriver([1, 2, 3, 4, 5, 6, 7, 'some error', 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20])
            self.assertEqual(test.run(driver, None), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'error: some-test\nsome error\n')


class TestReplayPerfTest(unittest.TestCase):

    class ReplayTestPort(TestPort):
        def __init__(self, custom_run_test=None):

            class ReplayTestDriver(TestDriver):
                def run_test(self, text_input, stop_when_done):
                    return custom_run_test(text_input, stop_when_done) if custom_run_test else None

            self._custom_driver_class = ReplayTestDriver
            super(self.__class__, self).__init__(host=MockHost())

        def _driver_class(self):
            return self._custom_driver_class

    class MockReplayServer(object):
        def __init__(self, wait_until_ready=True):
            self.wait_until_ready = lambda: wait_until_ready

        def stop(self):
            pass

    def _add_file(self, port, dirname, filename, content=True):
        port.host.filesystem.maybe_make_directory(dirname)
        port.host.filesystem.write_binary_file(port.host.filesystem.join(dirname, filename), content)

    def _setup_test(self, run_test=None):
        test_port = self.ReplayTestPort(run_test)
        self._add_file(test_port, '/path/some-dir', 'some-test.replay', 'http://some-test/')
        test = ReplayPerfTest(test_port, 'some-test.replay', '/path/some-dir/some-test.replay')
        test._start_replay_server = lambda archive, record: self.__class__.MockReplayServer()
        return test, test_port

    def test_run_single(self):
        output_capture = OutputCapture()
        output_capture.capture_output()

        loaded_pages = []

        def run_test(test_input, stop_when_done):
            if test_input.test_name != "about:blank":
                self.assertEqual(test_input.test_name, 'http://some-test/')
            loaded_pages.append(test_input)
            self._add_file(port, '/path/some-dir', 'some-test.wpr', 'wpr content')
            return DriverOutput('actual text', 'actual image', 'actual checksum',
                audio=None, crash=False, timeout=False, error=False)

        test, port = self._setup_test(run_test)
        test._archive_path = '/path/some-dir/some-test.wpr'
        test._url = 'http://some-test/'

        try:
            driver = port.create_driver(worker_number=1, no_timeout=True)
            self.assertTrue(test.run_single(driver, '/path/some-dir/some-test.replay', time_out_ms=100))
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        self.assertEqual(len(loaded_pages), 2)
        self.assertEqual(loaded_pages[0].test_name, 'about:blank')
        self.assertEqual(loaded_pages[1].test_name, 'http://some-test/')
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, '')
        self.assertEqual(port.host.filesystem.read_binary_file('/path/some-dir/some-test-actual.png'), 'actual image')

    def test_run_single_fails_without_webpagereplay(self):
        output_capture = OutputCapture()
        output_capture.capture_output()

        test, port = self._setup_test()
        test._start_replay_server = lambda archive, record: None
        test._archive_path = '/path/some-dir.wpr'
        test._url = 'http://some-test/'

        try:
            driver = port.create_driver(worker_number=1, no_timeout=True)
            self.assertEqual(test.run_single(driver, '/path/some-dir/some-test.replay', time_out_ms=100), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, "Web page replay didn't start.\n")

    def test_prepare_fails_when_wait_until_ready_fails(self):
        output_capture = OutputCapture()
        output_capture.capture_output()

        test, port = self._setup_test()
        test._start_replay_server = lambda archive, record: self.__class__.MockReplayServer(wait_until_ready=False)
        test._archive_path = '/path/some-dir.wpr'
        test._url = 'http://some-test/'

        try:
            driver = port.create_driver(worker_number=1, no_timeout=True)
            self.assertEqual(test.run_single(driver, '/path/some-dir/some-test.replay', time_out_ms=100), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, "Web page replay didn't start.\n")

    def test_run_single_fails_when_output_has_error(self):
        output_capture = OutputCapture()
        output_capture.capture_output()

        loaded_pages = []

        def run_test(test_input, stop_when_done):
            loaded_pages.append(test_input)
            self._add_file(port, '/path/some-dir', 'some-test.wpr', 'wpr content')
            return DriverOutput('actual text', 'actual image', 'actual checksum',
                audio=None, crash=False, timeout=False, error='some error')

        test, port = self._setup_test(run_test)
        test._archive_path = '/path/some-dir.wpr'
        test._url = 'http://some-test/'

        try:
            driver = port.create_driver(worker_number=1, no_timeout=True)
            self.assertEqual(test.run_single(driver, '/path/some-dir/some-test.replay', time_out_ms=100), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        self.assertEqual(len(loaded_pages), 2)
        self.assertEqual(loaded_pages[0].test_name, 'about:blank')
        self.assertEqual(loaded_pages[1].test_name, 'http://some-test/')
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'error: some-test.replay\nsome error\n')

    def test_prepare(self):
        output_capture = OutputCapture()
        output_capture.capture_output()

        def run_test(test_input, stop_when_done):
            self._add_file(port, '/path/some-dir', 'some-test.wpr', 'wpr content')
            return DriverOutput('actual text', 'actual image', 'actual checksum',
                audio=None, crash=False, timeout=False, error=False)

        test, port = self._setup_test(run_test)

        try:
            self.assertEqual(test.prepare(time_out_ms=100), True)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'Preparing replay for some-test.replay\nPrepared replay for some-test.replay\n')
        self.assertEqual(port.host.filesystem.read_binary_file('/path/some-dir/some-test-expected.png'), 'actual image')

    def test_prepare_calls_run_single(self):
        output_capture = OutputCapture()
        output_capture.capture_output()
        called = [False]

        def run_single(driver, url, time_out_ms, record):
            self.assertTrue(record)
            self.assertEqual(url, '/path/some-dir/some-test.wpr')
            called[0] = True
            return False

        test, port = self._setup_test()
        test.run_single = run_single

        try:
            self.assertEqual(test.prepare(time_out_ms=100), False)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertTrue(called[0])
        self.assertEqual(test._archive_path, '/path/some-dir/some-test.wpr')
        self.assertEqual(test._url, 'http://some-test/')
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, "Preparing replay for some-test.replay\nFailed to prepare a replay for some-test.replay\n")

class TestPerfTestFactory(unittest.TestCase):
    def test_regular_test(self):
        test = PerfTestFactory.create_perf_test(None, 'some-dir/some-test', '/path/some-dir/some-test')
        self.assertEqual(test.__class__, PerfTest)

    def test_inspector_test(self):
        test = PerfTestFactory.create_perf_test(None, 'inspector/some-test', '/path/inspector/some-test')
        self.assertEqual(test.__class__, ChromiumStylePerfTest)

    def test_page_loading_test(self):
        test = PerfTestFactory.create_perf_test(None, 'PageLoad/some-test', '/path/PageLoad/some-test')
        self.assertEqual(test.__class__, PageLoadingPerfTest)


if __name__ == '__main__':
    unittest.main()
