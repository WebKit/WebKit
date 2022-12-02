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


import logging
import unittest
import os

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.browser_driver.browser_driver import BrowserDriver
from webkitpy.benchmark_runner.browser_driver.browser_driver_factory import (
    BrowserDriverFactory,
)
from webkitpy.benchmark_runner.run_benchmark import (
    benchmark_runner_subclasses,
)


_log = logging.getLogger(__name__)


class FakeBrowserDriver(BrowserDriver):
    browser_name = None
    process_search_list = []
    platform = "fake"

    def __init__(self):
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
