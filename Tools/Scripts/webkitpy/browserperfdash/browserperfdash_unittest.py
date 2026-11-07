# Copyright (C) 2018 Igalia S.L.
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


import json
import logging
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.browser_driver.browser_driver import BrowserDriver
from webkitpy.benchmark_runner.browser_driver.browser_driver_factory import (
    BrowserDriverFactory,
)
from webkitpy.benchmark_runner.run_benchmark import (
    benchmark_runner_subclasses,
)
from webkitpy.benchmark_runner.webserver_benchmark_runner import WebServerBenchmarkRunner
from webkitpy.browserperfdash import browserperfdash_runner


_log = logging.getLogger(__name__)


class FakeBrowserDriver(BrowserDriver):
    browser_name = None
    process_search_list = []
    platform = "fake"

    def __init__(self, browser_args):
        super(FakeBrowserDriver, self).__init__(browser_args)
        self.process_name = "fake/process"

    def prepare_env(self, config):
        pass

    def prepare_initial_env(self, config):
        pass

    def restore_env(self):
        pass

    def restore_env_after_all_testing(self):
        pass

    def close_browsers(self):
        pass

    def launch_url(self, url, options, browser_build_path, browser_path):
        pass

    def launch_webdriver(self, url, driver):
        pass


BrowserDriverFactory.add_browser_driver("fake", None, FakeBrowserDriver)


class FakeBenchmarkRunner(BenchmarkRunner):
    name = 'fake'

    def __init__(self, plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path):
        super(FakeBenchmarkRunner, self).__init__(plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path)

    def execute(self):
        return True


# This tests for some features of benchmark_runner that browserperfdash_runner depends on
class BrowserPerfDashRunnerTest(unittest.TestCase):
    def test_list_plans_at_least_five(self):
        plan_list = BenchmarkRunner.available_plans()
        self.assertTrue(len(plan_list) > 4)

    def test_benchmark_runner_subclasses_at_least_two(self):
        self.assertTrue(len(benchmark_runner_subclasses) > 1)

    def test_can_construct_runner_object_minimum_parameters(self):
        # This tests that constructing the benchmark_runner object specifying the minimum required paramaters is ok.
        plan_list = BenchmarkRunner.available_plans()
        build_dir = os.path.abspath(os.curdir)
        runner = FakeBenchmarkRunner(
            plan_list[0], False, None, None, build_dir, "/tmp/testOutput.txt", "fake", None, None
        )
        self.assertTrue(runner.execute())


class RecordingBenchmarkRunner(object):
    # Records the diagnose directory each benchmark run gets, and writes a minimal result like run-benchmark does.
    diagnose_dirs = []

    def __init__(self, plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests=None, diagnose_dir=None, **kwargs):
        self._output_file = output_file
        self._subtests = subtests
        RecordingBenchmarkRunner.diagnose_dirs.append(diagnose_dir)

    def execute(self):
        result = {}
        if self._subtests:
            subtest_name = self._subtests[0].split('/')[-1]
            result = {'Benchmark': {'metrics': {'Score': ['Geometric']}, 'tests': {subtest_name: {'metrics': {'Score': {'current': [1]}}}}}}
        with open(self._output_file, 'w') as f:
            json.dump(result, f)


class BrowserPerfDashRunnerDiagnoseDirTest(unittest.TestCase):
    def setUp(self):
        self._temp_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self._temp_dir)
        self._diagnose_dir = os.path.join(self._temp_dir, 'diagnose')
        config_file = os.path.join(self._temp_dir, 'config.txt')
        with open(config_file, 'w') as f:
            f.write('[settings]\nplan_list = speedometer3\n\n[settings:other]\nplan_list = speedometer3\n\n')
            f.write('[test-server]\nbot_id = test\nbot_password = test\npost_url = http://127.0.0.1:9/\n')
        RecordingBenchmarkRunner.diagnose_dirs = []
        patches = [
            mock.patch.object(sys, 'argv', ['browserperfdash-benchmark', '--config-file', config_file, '--plans-from-config', '--timestamp', '1', '--browser-version', '1']),
            mock.patch.object(browserperfdash_runner, 'default_diagnose_dir', return_value=self._diagnose_dir),
            mock.patch.dict(browserperfdash_runner.benchmark_runner_subclasses, {WebServerBenchmarkRunner.name: RecordingBenchmarkRunner}),
            mock.patch.object(BenchmarkRunner, 'available_subtests', return_value={'JetStream2': ['Air', 'Basic']}),
        ]
        for patch in patches:
            patch.start()
            self.addCleanup(patch.stop)
        self._runner = browserperfdash_runner.BrowserPerfDashRunner(browserperfdash_runner.parse_args())

    def _run_plans(self, plans, settings_name='default'):
        with mock.patch.object(BrowserDriverFactory, 'create'), \
                mock.patch.object(browserperfdash_runner.BrowserPerfDashRunner, '_upload_result', return_value=True), \
                mock.patch.object(browserperfdash_runner.BrowserPerfDashRunner, '_get_plan_version_hash', return_value='hash'), \
                mock.patch.object(browserperfdash_runner.BrowserPerfDashRunner, '_get_test_data_json_string', return_value='{}'):
            return self._runner._run_and_upload_plans_for_browser(plans, [], settings_name)

    def test_diagnose_dir_per_plan(self):
        results = self._run_plans(['speedometer3'])
        self.assertEqual(results['worked'], ['speedometer3'])
        self.assertEqual(RecordingBenchmarkRunner.diagnose_dirs, [os.path.join(self._diagnose_dir, 'default', 'speedometer3')])

    def test_diagnose_dir_per_subtest_on_split_subtests_plans(self):
        results = self._run_plans(['jetstream2-split-subtests'])
        self.assertEqual(results['worked'], ['jetstream2-split-subtests'])
        plan_dir = os.path.join(self._diagnose_dir, 'default', 'jetstream2-split-subtests')
        self.assertEqual(RecordingBenchmarkRunner.diagnose_dirs, [os.path.join(plan_dir, 'JetStream2_Air'), os.path.join(plan_dir, 'JetStream2_Basic')])

    def test_diagnose_dir_per_browser_settings(self):
        self._run_plans(['speedometer3'])
        self._run_plans(['speedometer3'], 'other')
        self.assertEqual(RecordingBenchmarkRunner.diagnose_dirs, [os.path.join(self._diagnose_dir, 'default', 'speedometer3'),
                                                                  os.path.join(self._diagnose_dir, 'other', 'speedometer3')])
