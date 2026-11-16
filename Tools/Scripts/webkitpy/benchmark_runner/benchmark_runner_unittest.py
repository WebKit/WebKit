# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import collections
import io
import os
import unittest
from contextlib import redirect_stdout

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner


class MockRunner:
    def __init__(self, plan):
        self._plan = plan


class ConstructSubtestURLTest(unittest.TestCase):

    def _load_plan(self, name):
        _, plan = BenchmarkRunner._load_plan_data(name)
        return plan

    def _construct(self, plan, subtests):
        runner = MockRunner(plan)
        return BenchmarkRunner._construct_subtest_url(runner, subtests)

    # Edge cases

    def test_no_subtests_returns_empty(self):
        plan = self._load_plan('speedometer3.0')
        self.assertEqual(self._construct(plan, None), '')

    def test_empty_subtests_returns_empty(self):
        plan = self._load_plan('speedometer3.0')
        self.assertEqual(self._construct(plan, {}), '')

    def test_no_format_in_plan_returns_empty(self):
        plan = {'timeout': 600}
        subtests = collections.OrderedDict([('MotionMark', ['CanvasArcs'])])
        self.assertEqual(self._construct(plan, subtests), '')

    # Speedometer-style: comma-separated suite names

    def test_speedometer_single_subtest(self):
        plan = self._load_plan('speedometer3.0')
        subtests = collections.OrderedDict([('TodoMVC-React', [''])])
        self.assertEqual(self._construct(plan, subtests), 'TodoMVC-React,')

    def test_speedometer_multiple_subtests(self):
        plan = self._load_plan('speedometer3.0')
        subtests = collections.OrderedDict([('TodoMVC-React', ['']), ('TodoMVC-Vue', [''])])
        self.assertEqual(self._construct(plan, subtests), 'TodoMVC-React,TodoMVC-Vue,')

    # MotionMark-style: URL parameters with suite-name and test-name

    def test_motionmark_single_subtest(self):
        plan = self._load_plan('motionmark1.3.1')
        subtests = collections.OrderedDict([('MotionMark', ['CanvasArcs'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&suite-name=MotionMark&test-name=CanvasArcs')

    def test_motionmark_multiple_subtests(self):
        plan = self._load_plan('motionmark1.3.1')
        subtests = collections.OrderedDict([('MotionMark', ['CanvasArcs', 'Leaves'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&suite-name=MotionMark&test-name=CanvasArcs&suite-name=MotionMark&test-name=Leaves')

    # JetStream-style: leading & with test parameter

    def test_jetstream_single_subtest(self):
        plan = self._load_plan('jetstream3')
        subtests = collections.OrderedDict([('default', ['HashSet-String'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&test=HashSet-String')

    def test_jetstream_multiple_subtests(self):
        plan = self._load_plan('jetstream3')
        subtests = collections.OrderedDict([('default', ['HashSet-String', 'bomb-workers'])])
        self.assertEqual(
            self._construct(plan, subtests),
            '&test=HashSet-String&test=bomb-workers')


def iteration_result_with_score(score):
    return {'Speedometer': {
        'metrics': {'Score': ['Geometric']},
        'tests': {
            'SubTest1': {'metrics': {'Score': {'current': [score]}}},
            'SubTest2': {'metrics': {'Score': {'current': [score * 4]}}}}}}


class FormatIterationResultsTest(unittest.TestCase):

    def test_only_top_level_metrics_are_formatted(self):
        self.assertEqual(
            BenchmarkRunner._format_iteration_results(iteration_result_with_score(10)),
            'Speedometer:Score:Geometric: 20.0pt stdev=0.0%\n')

    def test_scale_unit_is_honored(self):
        self.assertEqual(
            BenchmarkRunner._format_iteration_results(iteration_result_with_score(10), scale_unit=False),
            'Speedometer:Score:Geometric: 20.000pt stdev=0.0%\n')

    def test_debug_output_is_ignored(self):
        result = iteration_result_with_score(10)
        result['debugOutput'] = ['some debug output']
        self.assertEqual(
            BenchmarkRunner._format_iteration_results(result),
            'Speedometer:Score:Geometric: 20.0pt stdev=0.0%\n')

    def test_grouped_values_within_one_iteration(self):
        result = {'Speedometer': {
            'metrics': {'Time': ['Total']},
            'tests': {
                'SubTest1': {'metrics': {'Time': {'current': [[1, 2]]}}},
                'SubTest2': {'metrics': {'Time': {'current': [[5, 6]]}}}}}}
        self.assertEqual(
            BenchmarkRunner._format_iteration_results(result, scale_unit=False),
            'Speedometer:Time:Total: 7.000ms stdev=20.2%\n')
        self.assertEqual(
            BenchmarkRunner._format_iteration_results(result, scale_unit=False, show_iteration_raw_values=True),
            'Speedometer:Time:Total: 7.000ms stdev=20.2% raw=[6.000, 8.000]\n')


class MockBrowserDriver:

    def prepare_initial_env(self, config):
        pass

    def prepare_env(self, config):
        pass

    def restore_env(self):
        pass

    def restore_env_after_all_testing(self):
        pass


class FakeBenchmarkRunner(BenchmarkRunner):
    def __init__(self, iteration_results, scale_unit=True, show_iteration_raw_values=False):
        self._iteration_results = iteration_results
        self._plan = {'entry_point': 'index.html', 'options': {}}
        self._plan_name = 'fake'
        self._subtests = None
        self._browser_driver = MockBrowserDriver()
        self._config = {}
        self._generate_pgo_profiles = False
        self._diagnose_dir = None
        self._output_file = os.devnull
        self._scale_unit = scale_unit
        self._show_iteration_raw_values = show_iteration_raw_values

    def _run_one_test(self, web_root, test_file, iteration):
        return self._iteration_results[iteration - 1]


class ShowIterationResultsTest(unittest.TestCase):

    def test_logs_top_level_metrics(self):
        runner = FakeBenchmarkRunner([])
        with self.assertLogs('webkitpy.benchmark_runner.benchmark_runner', level='INFO') as logs:
            runner._show_iteration_results(2, iteration_result_with_score(10))
        self.assertEqual(
            [record.getMessage() for record in logs.records],
            ['Top level results of the iteration 2:\nSpeedometer:Score:Geometric: 20.0pt stdev=0.0%\n'])

    def test_honors_runner_formatting_options(self):
        runner = FakeBenchmarkRunner([], scale_unit=False)
        with self.assertLogs('webkitpy.benchmark_runner.benchmark_runner', level='INFO') as logs:
            runner._show_iteration_results(1, iteration_result_with_score(10))
        self.assertEqual(
            [record.getMessage() for record in logs.records],
            ['Top level results of the iteration 1:\nSpeedometer:Score:Geometric: 20.000pt stdev=0.0%\n'])

    def test_honors_show_iteration_raw_values(self):
        runner = FakeBenchmarkRunner([], show_iteration_raw_values=True)
        with self.assertLogs('webkitpy.benchmark_runner.benchmark_runner', level='INFO') as logs:
            runner._show_iteration_results(1, iteration_result_with_score(10))
        self.assertEqual(
            [record.getMessage() for record in logs.records],
            ['Top level results of the iteration 1:\nSpeedometer:Score:Geometric: 20.0pt stdev=0.0% raw=[20.0]\n'])

    def test_malformed_results_are_reported_but_not_raised(self):
        runner = FakeBenchmarkRunner([])
        with self.assertLogs('webkitpy.benchmark_runner.benchmark_runner', level='WARNING') as logs:
            runner._show_iteration_results(3, {'Speedometer': {}})
        self.assertEqual(
            [record.getMessage() for record in logs.records],
            ['Cannot format the results of the iteration 3: "Speedometer" does not contain metrics or tests'])


class RunBenchmarkIterationLoggingTest(unittest.TestCase):

    def test_top_level_metrics_are_logged_after_each_iteration(self):
        runner = FakeBenchmarkRunner([iteration_result_with_score(10), iteration_result_with_score(20)])
        with self.assertLogs('webkitpy.benchmark_runner.benchmark_runner', level='INFO') as logs:
            with redirect_stdout(io.StringIO()):
                runner._run_benchmark(2, '/fake/web/root')
        self.assertEqual([record.getMessage() for record in logs.records], [
            'Start the iteration 1 of 2 for current benchmark',
            'End the iteration 1 of 2 for current benchmark',
            'Top level results of the iteration 1:\nSpeedometer:Score:Geometric: 20.0pt stdev=0.0%\n',
            'Start the iteration 2 of 2 for current benchmark',
            'End the iteration 2 of 2 for current benchmark',
            'Top level results of the iteration 2:\nSpeedometer:Score:Geometric: 40.0pt stdev=0.0%\n',
            'Dumping the results to file {}'.format(os.devnull)])
