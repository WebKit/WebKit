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
import json
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
from webkitpy.performance_tests.perftest import PerfTestMetric
from webkitpy.performance_tests.perftest import PerfTestFactory
from webkitpy.performance_tests.perftest import ReplayPerfTest


class MockPort(TestPort):
    def __init__(self, custom_run_test=None):
        super(MockPort, self).__init__(host=MockHost(), custom_run_test=custom_run_test)


class TestPerfTestMetric(unittest.TestCase):
    def test_init_set_missing_unit(self):
        self.assertEqual(PerfTestMetric('Time', iterations=[1, 2, 3, 4, 5]).to_dict()['unit'], 'ms')
        self.assertEqual(PerfTestMetric('Malloc', iterations=[1, 2, 3, 4, 5]).to_dict()['unit'], 'bytes')
        self.assertEqual(PerfTestMetric('JSHeap', iterations=[1, 2, 3, 4, 5]).to_dict()['unit'], 'bytes')

    def test_legacy_chromium_bot_compatible_test_name(self):
        self.assertEqual(PerfTestMetric('Time').legacy_chromium_bot_compatible_test_name('test'), 'test')
        self.assertEqual(PerfTestMetric('Malloc').legacy_chromium_bot_compatible_test_name('test'), 'test:Malloc')
        self.assertEqual(PerfTestMetric('JSHeap').legacy_chromium_bot_compatible_test_name('test'), 'test:JSHeap')
        self.assertEqual(PerfTestMetric('FontSize', unit='em').legacy_chromium_bot_compatible_test_name('test'), 'test:FontSize')

    def test_has_values(self):
        self.assertFalse(PerfTestMetric('Time').has_values())
        self.assertTrue(PerfTestMetric('Time', iterations=[1]).has_values())

    def test_append(self):
        metric = PerfTestMetric('Time')
        metric2 = PerfTestMetric('Time')
        self.assertFalse(metric.has_values())
        self.assertFalse(metric2.has_values())

        metric.append(1)
        self.assertTrue(metric.has_values())
        self.assertFalse(metric2.has_values())
        self.assertEqual(metric.to_dict()['values'], [1])
        metric.append(2)
        self.assertEqual(metric.to_dict()['values'], [1, 2])

        metric2.append(3)
        self.assertTrue(metric2.has_values())
        self.assertEqual(metric.to_dict()['values'], [1, 2])
        self.assertEqual(metric2.to_dict()['values'], [3])

    def test_compute_statistics(self):
        def compute_statistics(values):
            statistics = PerfTestMetric.compute_statistics(map(lambda x: float(x), values))
            return json.loads(json.dumps(statistics))

        statistics = compute_statistics([10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11])
        self.assertEqual(sorted(statistics.keys()), ['avg', 'max', 'median', 'min', 'stdev'])
        self.assertEqual(statistics['avg'], 10.5)
        self.assertEqual(statistics['min'], 1)
        self.assertEqual(statistics['max'], 20)
        self.assertEqual(statistics['median'], 10.5)
        self.assertEqual(compute_statistics([8, 9, 10, 11, 12])['avg'], 10)
        self.assertEqual(compute_statistics([8, 9, 10, 11, 12] * 4)['avg'], 10)
        self.assertEqual(compute_statistics([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19])['avg'], 10)
        self.assertEqual(compute_statistics([1, 5, 2, 8, 7])['median'], 5)
        self.assertEqual(compute_statistics([1, 6, 2, 8, 7, 2])['median'], 4)
        self.assertAlmostEqual(statistics['stdev'], math.sqrt(35))
        self.assertAlmostEqual(compute_statistics([1])['stdev'], 0)
        self.assertAlmostEqual(compute_statistics([1, 2, 3, 4, 5, 6])['stdev'], math.sqrt(3.5))
        self.assertAlmostEqual(compute_statistics([4, 2, 5, 8, 6])['stdev'], math.sqrt(5))


class TestPerfTest(unittest.TestCase):
    def _assert_results_are_correct(self, test, output):
        test._filter_output(output)
        parsed_results = test.parse_output(output)
        self.assertEqual(len(parsed_results), 1)
        some_test_results = parsed_results[0].to_dict()
        self.assertEqual(sorted(some_test_results.keys()), ['avg', 'max', 'median', 'min', 'stdev', 'unit', 'values'])
        self.assertEqual(some_test_results['values'], [1080, 1120, 1095, 1101, 1104])
        self.assertEqual(some_test_results['min'], 1080)
        self.assertEqual(some_test_results['max'], 1120)
        self.assertEqual(some_test_results['avg'], 1100)
        self.assertEqual(some_test_results['median'], 1101)
        self.assertAlmostEqual(some_test_results['stdev'], 14.50862, places=5)
        self.assertEqual(some_test_results['unit'], 'ms')

    def test_parse_output(self):
        output = DriverOutput("""
Running 20 times
Ignoring warm-up run (1115)

Time:
values 1080, 1120, 1095, 1101, 1104 ms
avg 1100 ms
median 1101 ms
stdev 14.50862 ms
min 1080 ms
max 1120 ms
""", image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
            self._assert_results_are_correct(test, output)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, '')

    def test_parse_output_with_failing_line(self):
        output = DriverOutput("""
Running 20 times
Ignoring warm-up run (1115)

some-unrecognizable-line

Time:
values 1080, 1120, 1095, 1101, 1104 ms
avg 1100 ms
median 1101 ms
stdev 14.50862 ms
min 1080 ms
max 1120 ms
""", image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
            test._filter_output(output)
            self.assertEqual(test.parse_output(output), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'ERROR: some-unrecognizable-line\n')

    def test_parse_output_with_description(self):
        output = DriverOutput("""
Description: this is a test description.

Running 20 times
Ignoring warm-up run (1115)

Time:
values 1080, 1120, 1095, 1101, 1104 ms
avg 1100 ms
median 1101 ms
stdev 14.50862 ms
min 1080 ms
max 1120 ms""", image=None, image_hash=None, audio=None)
        test = PerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
        self._assert_results_are_correct(test, output)
        self.assertEqual(test.description(), 'this is a test description.')

    def test_ignored_stderr_lines(self):
        test = PerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
        ignored_lines = [
            "Unknown option: --foo-bar",
            "[WARNING:proxy_service.cc] bad moon a-rising",
            "[INFO:SkFontHost_android.cpp(1158)] Use Test Config File Main /data/local/tmp/drt/android_main_fonts.xml, Fallback /data/local/tmp/drt/android_fallback_fonts.xml, Font Dir /data/local/tmp/drt/fonts/",
        ]
        for line in ignored_lines:
            self.assertTrue(test._should_ignore_line_in_stderr(line))

        non_ignored_lines = [
            "Should not be ignored",
            "[WARNING:chrome.cc] Something went wrong",
            "[ERROR:main.cc] The sky has fallen",
        ]
        for line in non_ignored_lines:
            self.assertFalse(test._should_ignore_line_in_stderr(line))

    def test_parse_output_with_subtests(self):
        output = DriverOutput("""
Running 20 times
some test: [1, 2, 3, 4, 5]
other test = else: [6, 7, 8, 9, 10]
Ignoring warm-up run (1115)

Time:
values 1080, 1120, 1095, 1101, 1104 ms
avg 1100 ms
median 1101 ms
stdev 14.50862 ms
min 1080 ms
max 1120 ms
""", image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
            self._assert_results_are_correct(test, output)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, '')


class TestPageLoadingPerfTest(unittest.TestCase):
    class MockDriver(object):
        def __init__(self, values, test, measurements=None):
            self._values = values
            self._index = 0
            self._test = test
            self._measurements = measurements

        def run_test(self, input, stop_when_done):
            if input.test_name == self._test.force_gc_test:
                return
            value = self._values[self._index]
            self._index += 1
            if isinstance(value, str):
                return DriverOutput('some output', image=None, image_hash=None, audio=None, error=value)
            else:
                return DriverOutput('some output', image=None, image_hash=None, audio=None, test_time=self._values[self._index - 1], measurements=self._measurements)

    def test_run(self):
        port = MockPort()
        test = PageLoadingPerfTest(port, 'some-test', '/path/some-dir/some-test')
        driver = TestPageLoadingPerfTest.MockDriver(range(1, 21), test)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            metrics = test._run_with_driver(driver, None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, '')

        self.assertEqual(len(metrics), 1)
        self.assertEqual(metrics[0].metric(), 'Time')
        self.assertEqual(metrics[0].to_dict(), {'max': 20000, 'avg': 11000.0, 'median': 11000, 'stdev': 5627.314338711378, 'min': 2000, 'unit': 'ms',
            'values': [float(i * 1000) for i in range(2, 21)]})

    def test_run_with_memory_output(self):
        port = MockPort()
        test = PageLoadingPerfTest(port, 'some-test', '/path/some-dir/some-test')
        memory_results = {'Malloc': 10, 'JSHeap': 5}
        self.maxDiff = None
        driver = TestPageLoadingPerfTest.MockDriver(range(1, 21), test, memory_results)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            metrics = test._run_with_driver(driver, None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, '')

        self.assertEqual(len(metrics), 3)
        self.assertEqual(metrics[0].metric(), 'Time')
        self.assertEqual(metrics[0].to_dict(), {'max': 20000, 'avg': 11000.0, 'median': 11000, 'stdev': 5627.314338711378, 'min': 2000, 'unit': 'ms',
            'values': [float(i * 1000) for i in range(2, 21)]})
        self.assertEqual(metrics[1].metric(), 'Malloc')
        self.assertEqual(metrics[1].to_dict(), {'max': 10, 'avg': 10.0, 'median': 10, 'min': 10, 'stdev': 0.0, 'unit': 'bytes',
            'values': [float(10)] * 19})
        self.assertEqual(metrics[2].metric(), 'JSHeap')
        self.assertEqual(metrics[2].to_dict(), {'max': 5, 'avg': 5.0, 'median': 5, 'min': 5, 'stdev': 0.0, 'unit': 'bytes',
            'values': [float(5)] * 19})

    def test_run_with_bad_output(self):
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            port = MockPort()
            test = PageLoadingPerfTest(port, 'some-test', '/path/some-dir/some-test')
            driver = TestPageLoadingPerfTest.MockDriver([1, 2, 3, 4, 5, 6, 7, 'some error', 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20], test)
            self.assertEqual(test._run_with_driver(driver, None), None)
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, 'error: some-test\nsome error\n')


class TestReplayPerfTest(unittest.TestCase):

    class ReplayTestPort(MockPort):
        def __init__(self, custom_run_test=None):

            class ReplayTestDriver(TestDriver):
                def run_test(self, text_input, stop_when_done):
                    return custom_run_test(text_input, stop_when_done) if custom_run_test else None

            self._custom_driver_class = ReplayTestDriver
            super(self.__class__, self).__init__()

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
            if test_input.test_name == test.force_gc_test:
                loaded_pages.append(test_input)
                return
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
        self.assertEqual(loaded_pages[0].test_name, test.force_gc_test)
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
        self.assertEqual(loaded_pages[0].test_name, test.force_gc_test)
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
        test = PerfTestFactory.create_perf_test(MockPort(), 'some-dir/some-test', '/path/some-dir/some-test')
        self.assertEqual(test.__class__, PerfTest)

    def test_inspector_test(self):
        test = PerfTestFactory.create_perf_test(MockPort(), 'inspector/some-test', '/path/inspector/some-test')
        self.assertEqual(test.__class__, ChromiumStylePerfTest)
