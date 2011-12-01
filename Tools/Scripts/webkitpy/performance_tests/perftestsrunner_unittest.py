#!/usr/bin/python
# Copyright (C) 2011 Google Inc. All rights reserved.
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

"""Unit tests for run_perf_tests."""

import unittest

from webkitpy.common import array_stream
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.layout_tests.port.driver import DriverInput, DriverOutput
from webkitpy.layout_tests.views import printing
from webkitpy.performance_tests.perftestsrunner import PerfTestsRunner


class MainTest(unittest.TestCase):
    class TestPort:
        def create_driver(self, worker_number=None):
            return MainTest.TestDriver()

    class TestDriver:
        def run_test(self, driver_input):
            text = ''
            timeout = False
            crash = False
            if driver_input.test_name == 'pass.html':
                text = 'RESULT group_name: test_name= 42 ms'
            elif driver_input.test_name == 'timeout.html':
                timeout = True
            elif driver_input.test_name == 'failed.html':
                text = None
            elif driver_input.test_name == 'tonguey.html':
                text = 'we are not expecting an output from perf tests but RESULT blablabla'
            elif driver_input.test_name == 'crash.html':
                crash = True
            return DriverOutput(text, '', '', '', crash=crash, timeout=timeout)

        def stop(self):
            """do nothing"""

    def create_runner(self, buildbot_output=None):
        buildbot_output = buildbot_output or array_stream.ArrayStream()
        regular_output = array_stream.ArrayStream()
        return PerfTestsRunner('', regular_output, buildbot_output, args=[])

    def run_test(self, test_name):
        runner = self.create_runner()
        driver = MainTest.TestDriver()
        return runner._run_single_test(test_name, driver)

    def test_run_passing_test(self):
        test_failed, driver_need_restart = self.run_test('pass.html')
        self.assertFalse(test_failed)
        self.assertFalse(driver_need_restart)

    def test_run_silent_test(self):
        test_failed, driver_need_restart = self.run_test('silent.html')
        self.assertTrue(test_failed)
        self.assertFalse(driver_need_restart)

    def test_run_failed_test(self):
        test_failed, driver_need_restart = self.run_test('failed.html')
        self.assertTrue(test_failed)
        self.assertFalse(driver_need_restart)

    def test_run_tonguey_test(self):
        test_failed, driver_need_restart = self.run_test('tonguey.html')
        self.assertTrue(test_failed)
        self.assertFalse(driver_need_restart)

    def test_run_timeout_test(self):
        test_failed, driver_need_restart = self.run_test('timeout.html')
        self.assertTrue(test_failed)
        self.assertTrue(driver_need_restart)

    def test_run_crash_test(self):
        test_failed, driver_need_restart = self.run_test('crash.html')
        self.assertTrue(test_failed)
        self.assertTrue(driver_need_restart)

    def test_run_test_set(self):
        buildbot_output = array_stream.ArrayStream()
        runner = self.create_runner(buildbot_output)
        port = MainTest.TestPort()
        tests = ['pass.html', 'silent.html', 'failed.html', 'tonguey.html', 'timeout.html', 'crash.html']
        unexpected_result_count = runner._run_tests_set(tests, port)
        self.assertEqual(unexpected_result_count, len(tests) - 1)
        self.assertEqual(len(buildbot_output.get()), 1)
        self.assertEqual(buildbot_output.get()[0], 'RESULT group_name: test_name= 42 ms\n')

    def test_collect_tests(self):
        runner = self.create_runner()
        webkit_base = '/test.checkout'
        filesystem = MockFileSystem()
        filename = filesystem.join(webkit_base, 'PerformanceTests', 'a_file.html')
        filesystem.files[filename] = 'a content'
        tests = runner._collect_tests(webkit_base, filesystem)
        self.assertEqual(len(tests), 1)

    def test_parse_args(self):
        runner = self.create_runner()
        options, args = runner._parse_args([
                '--verbose',
                '--build-directory=folder42',
                '--platform=platform42',
                '--time-out-ms=42',
                '--debug', 'an_arg'])
        self.assertEqual(options.verbose, True)
        self.assertEqual(options.help_printing, None)
        self.assertEqual(options.build_directory, 'folder42')
        self.assertEqual(options.platform, 'platform42')
        self.assertEqual(options.time_out_ms, '42')
        self.assertEqual(options.configuration, 'Debug')
        self.assertEqual(options.print_options, None)


if __name__ == '__main__':
    unittest.main()
