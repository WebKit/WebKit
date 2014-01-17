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
import unittest2 as unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.port.driver import DriverOutput
from webkitpy.port.test import TestDriver
from webkitpy.port.test import TestPort
from webkitpy.performance_tests.perftest import PerfTest
from webkitpy.performance_tests.perftest import PerfTestMetric
from webkitpy.performance_tests.perftest import PerfTestFactory
from webkitpy.performance_tests.perftest import SingleProcessPerfTest


class MockPort(TestPort):
    def __init__(self, custom_run_test=None):
        super(MockPort, self).__init__(host=MockHost(), custom_run_test=custom_run_test)


class TestPerfTestMetric(unittest.TestCase):
    def test_init_set_missing_unit(self):
        self.assertEqual(PerfTestMetric(['some', 'test'], 'some/test.html', 'Time', iterations=[1, 2, 3, 4, 5]).unit(), 'ms')
        self.assertEqual(PerfTestMetric(['some', 'test'], 'some/test.html', 'Malloc', iterations=[1, 2, 3, 4, 5]).unit(), 'bytes')
        self.assertEqual(PerfTestMetric(['some', 'test'], 'some/test.html', 'JSHeap', iterations=[1, 2, 3, 4, 5]).unit(), 'bytes')

    def test_init_set_time_metric(self):
        self.assertEqual(PerfTestMetric(['some', 'test'], 'some/test.html', 'Time', 'ms').name(), 'Time')
        self.assertEqual(PerfTestMetric(['some', 'test'], 'some/test.html', 'Time', 'fps').name(), 'FrameRate')
        self.assertEqual(PerfTestMetric(['some', 'test'], 'some/test.html', 'Time', 'runs/s').name(), 'Runs')

    def test_has_values(self):
        self.assertFalse(PerfTestMetric(['some', 'test'], 'some/test.html', 'Time').has_values())
        self.assertTrue(PerfTestMetric(['some', 'test'], 'some/test.html', 'Time', iterations=[1]).has_values())

    def test_append(self):
        metric = PerfTestMetric(['some', 'test'], 'some/test.html', 'Time')
        metric2 = PerfTestMetric(['some', 'test'], 'some/test.html', 'Time')
        self.assertFalse(metric.has_values())
        self.assertFalse(metric2.has_values())

        metric.append_group([1])
        self.assertTrue(metric.has_values())
        self.assertFalse(metric2.has_values())
        self.assertEqual(metric.grouped_iteration_values(), [[1]])
        self.assertEqual(metric.flattened_iteration_values(), [1])

        metric.append_group([2])
        self.assertEqual(metric.grouped_iteration_values(), [[1], [2]])
        self.assertEqual(metric.flattened_iteration_values(), [1, 2])

        metric2.append_group([3])
        self.assertTrue(metric2.has_values())
        self.assertEqual(metric.flattened_iteration_values(), [1, 2])
        self.assertEqual(metric2.flattened_iteration_values(), [3])

        metric.append_group([4, 5])
        self.assertEqual(metric.grouped_iteration_values(), [[1], [2], [4, 5]])
        self.assertEqual(metric.flattened_iteration_values(), [1, 2, 4, 5])


class TestPerfTest(unittest.TestCase):
    def _assert_results_are_correct(self, test, output):
        test.run_single = lambda driver, path, time_out_ms: output
        self.assertTrue(test.run(10))
        subtests = test._metrics
        self.assertEqual(map(lambda test: test['name'], subtests), [None])
        metrics = subtests[0]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), metrics), ['Time'])
        self.assertEqual(metrics[0].flattened_iteration_values(), [1080, 1120, 1095, 1101, 1104] * 4)

    def test_parse_output(self):
        output = DriverOutput("""
:Time -> [1080, 1120, 1095, 1101, 1104] ms
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
        self.assertEqual(actual_logs, """RESULT some-test: Time= 1100.0 ms
median= 1101.0 ms, stdev= 13.3140211016 ms, min= 1080.0 ms, max= 1120.0 ms
""")

    def _assert_failed_on_line(self, output_text, expected_log):
        output = DriverOutput(output_text, image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
            test.run_single = lambda driver, path, time_out_ms: output
            self.assertFalse(test._run_with_driver(None, None))
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()
        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, expected_log)

    def test_parse_output_with_running_five_times(self):
        self._assert_failed_on_line("""
Running 5 times
:Time -> [1080, 1120, 1095, 1101, 1104] ms
""", 'ERROR: Running 5 times\n')

    def test_parse_output_with_detailed_info(self):
        self._assert_failed_on_line("""
    1: 1080 ms
:Time -> [1080, 1120, 1095, 1101, 1104] ms
""", 'ERROR:     1: 1080 ms\n')

    def test_parse_output_with_statistics(self):
        self._assert_failed_on_line("""
:Time -> [1080, 1120, 1095, 1101, 1104] ms
    mean: 105 ms
""", 'ERROR:     mean: 105 ms\n')

    def test_parse_output_with_description(self):
        output = DriverOutput("""
Description: this is a test description.

:Time -> [1080, 1120, 1095, 1101, 1104] ms
""", image=None, image_hash=None, audio=None)
        test = PerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
        self._assert_results_are_correct(test, output)
        self.assertEqual(test.description(), 'this is a test description.')

    def test_parse_output_with_subtests(self):
        output = DriverOutput("""
Description: this is a test description.
some test:Time -> [1, 2, 3, 4, 5] ms
some other test = else:Time -> [6, 7, 8, 9, 10] ms
some other test = else:Malloc -> [11, 12, 13, 14, 15] bytes
Array Construction, []:Time -> [11, 12, 13, 14, 15] ms
Concat String:Time -> [15163, 15304, 15386, 15608, 15622] ms
jQuery - addClass:Time -> [2785, 2815, 2826, 2841, 2861] ms
Dojo - div:only-child:Time -> [7825, 7910, 7950, 7958, 7970] ms
Dojo - div:nth-child(2n+1):Time -> [3620, 3623, 3633, 3641, 3658] ms
Dojo - div > div:Time -> [10158, 10172, 10180, 10183, 10231] ms
Dojo - div ~ div:Time -> [6673, 6675, 6714, 6848, 6902] ms

:Time -> [1080, 1120, 1095, 1101, 1104] ms
""", image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(MockPort(), 'some-dir/some-test', '/path/some-dir/some-test')
            test.run_single = lambda driver, path, time_out_ms: output
            self.assertTrue(test.run(10))
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        subtests = test._metrics
        self.assertEqual(map(lambda test: test['name'], subtests), ['some test', 'some other test = else',
            'Array Construction, []', 'Concat String', 'jQuery - addClass', 'Dojo - div:only-child',
            'Dojo - div:nth-child(2n+1)', 'Dojo - div > div', 'Dojo - div ~ div', None])

        some_test_metrics = subtests[0]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), some_test_metrics), ['Time'])
        self.assertEqual(some_test_metrics[0].path(), ['some-dir', 'some-test', 'some test'])
        self.assertEqual(some_test_metrics[0].flattened_iteration_values(), [1, 2, 3, 4, 5] * 4)

        some_other_test_metrics = subtests[1]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), some_other_test_metrics), ['Time', 'Malloc'])
        self.assertEqual(some_other_test_metrics[0].path(), ['some-dir', 'some-test', 'some other test = else'])
        self.assertEqual(some_other_test_metrics[0].flattened_iteration_values(), [6, 7, 8, 9, 10] * 4)
        self.assertEqual(some_other_test_metrics[1].path(), ['some-dir', 'some-test', 'some other test = else'])
        self.assertEqual(some_other_test_metrics[1].flattened_iteration_values(), [11, 12, 13, 14, 15] * 4)

        main_metrics = subtests[len(subtests) - 1]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), main_metrics), ['Time'])
        self.assertEqual(main_metrics[0].path(), ['some-dir', 'some-test'])
        self.assertEqual(main_metrics[0].flattened_iteration_values(), [1080, 1120, 1095, 1101, 1104] * 4)

        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, """DESCRIPTION: this is a test description.
RESULT some-dir: some-test: Time= 1100.0 ms
median= 1101.0 ms, stdev= 13.3140211016 ms, min= 1080.0 ms, max= 1120.0 ms
""")

    def test_parse_output_with_subtests_and_total(self):
        output = DriverOutput("""
:Time:Total -> [2324, 2328, 2345, 2314, 2312] ms
EmberJS-TodoMVC:Time:Total -> [1462, 1473, 1490, 1465, 1458] ms
EmberJS-TodoMVC/a:Time -> [1, 2, 3, 4, 5] ms
BackboneJS-TodoMVC:Time -> [862, 855, 855, 849, 854] ms
""", image=None, image_hash=None, audio=None)
        output_capture = OutputCapture()
        output_capture.capture_output()
        try:
            test = PerfTest(MockPort(), 'some-dir/some-test', '/path/some-dir/some-test')
            test.run_single = lambda driver, path, time_out_ms: output
            self.assertTrue(test.run(10))
        finally:
            actual_stdout, actual_stderr, actual_logs = output_capture.restore_output()

        subtests = test._metrics
        self.assertEqual(map(lambda test: test['name'], subtests), [None, 'EmberJS-TodoMVC', 'EmberJS-TodoMVC/a', 'BackboneJS-TodoMVC'])

        main_metrics = subtests[0]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), main_metrics), ['Time'])
        self.assertEqual(main_metrics[0].aggregator(), 'Total')
        self.assertEqual(main_metrics[0].path(), ['some-dir', 'some-test'])
        self.assertEqual(main_metrics[0].flattened_iteration_values(), [2324, 2328, 2345, 2314, 2312] * 4)

        some_test_metrics = subtests[1]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), some_test_metrics), ['Time'])
        self.assertEqual(some_test_metrics[0].aggregator(), 'Total')
        self.assertEqual(some_test_metrics[0].path(), ['some-dir', 'some-test', 'EmberJS-TodoMVC'])
        self.assertEqual(some_test_metrics[0].flattened_iteration_values(), [1462, 1473, 1490, 1465, 1458] * 4)

        some_test_metrics = subtests[2]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), some_test_metrics), ['Time'])
        self.assertEqual(some_test_metrics[0].aggregator(), None)
        self.assertEqual(some_test_metrics[0].path(), ['some-dir', 'some-test', 'EmberJS-TodoMVC', 'a'])
        self.assertEqual(some_test_metrics[0].flattened_iteration_values(), [1, 2, 3, 4, 5] * 4)

        some_test_metrics = subtests[3]['metrics']
        self.assertEqual(map(lambda metric: metric.name(), some_test_metrics), ['Time'])
        self.assertEqual(some_test_metrics[0].aggregator(), None)
        self.assertEqual(some_test_metrics[0].path(), ['some-dir', 'some-test', 'BackboneJS-TodoMVC'])
        self.assertEqual(some_test_metrics[0].flattened_iteration_values(), [862, 855, 855, 849, 854] * 4)

        self.assertEqual(actual_stdout, '')
        self.assertEqual(actual_stderr, '')
        self.assertEqual(actual_logs, """RESULT some-dir: some-test: Time= 2324.6 ms
median= 2324.0 ms, stdev= 12.1326007105 ms, min= 2312.0 ms, max= 2345.0 ms
""")


class TestSingleProcessPerfTest(unittest.TestCase):
    def test_use_only_one_process(self):
        called = [0]

        def run_single(driver, path, time_out_ms):
            called[0] += 1
            return DriverOutput("""
Description: this is a test description.
:Time -> [1080, 1120, 1095, 1101, 1104] ms
""", image=None, image_hash=None, audio=None)

        test = SingleProcessPerfTest(MockPort(), 'some-test', '/path/some-dir/some-test')
        test.run_single = run_single
        self.assertTrue(test.run(0))
        self.assertEqual(called[0], 1)


class TestPerfTestFactory(unittest.TestCase):
    def test_regular_test(self):
        test = PerfTestFactory.create_perf_test(MockPort(), 'some-dir/some-test', '/path/some-dir/some-test')
        self.assertEqual(test.__class__, PerfTest)
