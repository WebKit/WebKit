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

"""Unit tests for run_perf_tests."""

import StringIO
import json
import re
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.port.test import TestPort
from webkitpy.performance_tests.perftestsrunner import PerfTestsRunner


class MainTest(unittest.TestCase):
    def create_runner(self, args=[]):
        options, parsed_args = PerfTestsRunner._parse_args(args)
        test_port = TestPort(host=MockHost(), options=options)
        runner = PerfTestsRunner(args=args, port=test_port)
        runner._host.filesystem.maybe_make_directory(runner._base_path, 'inspector')
        runner._host.filesystem.maybe_make_directory(runner._base_path, 'Bindings')
        runner._host.filesystem.maybe_make_directory(runner._base_path, 'Parser')
        return runner, test_port

    def _add_file(self, runner, dirname, filename, content=True):
        dirname = runner._host.filesystem.join(runner._base_path, dirname) if dirname else runner._base_path
        runner._host.filesystem.maybe_make_directory(dirname)
        runner._host.filesystem.files[runner._host.filesystem.join(dirname, filename)] = content

    def test_collect_tests(self):
        runner, port = self.create_runner()
        self._add_file(runner, 'inspector', 'a_file.html', 'a content')
        tests = runner._collect_tests()
        self.assertEqual(len(tests), 1)

    def _collect_tests_and_sort_test_name(self, runner):
        return sorted([test.test_name() for test in runner._collect_tests()])

    def test_collect_tests_with_multile_files(self):
        runner, port = self.create_runner(args=['PerformanceTests/test1.html', 'test2.html'])

        def add_file(filename):
            port.host.filesystem.files[runner._host.filesystem.join(runner._base_path, filename)] = 'some content'

        add_file('test1.html')
        add_file('test2.html')
        add_file('test3.html')
        port.host.filesystem.chdir(runner._port.perf_tests_dir()[:runner._port.perf_tests_dir().rfind(runner._host.filesystem.sep)])
        self.assertEqual(self._collect_tests_and_sort_test_name(runner), ['test1.html', 'test2.html'])

    def test_collect_tests_with_skipped_list(self):
        runner, port = self.create_runner()

        self._add_file(runner, 'inspector', 'test1.html')
        self._add_file(runner, 'inspector', 'unsupported_test1.html')
        self._add_file(runner, 'inspector', 'test2.html')
        self._add_file(runner, 'inspector/resources', 'resource_file.html')
        self._add_file(runner, 'unsupported', 'unsupported_test2.html')
        port.skipped_perf_tests = lambda: ['inspector/unsupported_test1.html', 'unsupported']
        self.assertEqual(self._collect_tests_and_sort_test_name(runner), ['inspector/test1.html', 'inspector/test2.html'])

    def test_collect_tests_with_skipped_list_and_files(self):
        runner, port = self.create_runner(args=['Suite/Test1.html', 'Suite/SkippedTest1.html', 'SkippedSuite/Test1.html'])

        self._add_file(runner, 'SkippedSuite', 'Test1.html')
        self._add_file(runner, 'SkippedSuite', 'Test2.html')
        self._add_file(runner, 'Suite', 'Test1.html')
        self._add_file(runner, 'Suite', 'Test2.html')
        self._add_file(runner, 'Suite', 'SkippedTest1.html')
        self._add_file(runner, 'Suite', 'SkippedTest2.html')
        port.skipped_perf_tests = lambda: ['Suite/SkippedTest1.html', 'Suite/SkippedTest1.html', 'SkippedSuite']
        self.assertEqual(self._collect_tests_and_sort_test_name(runner),
            ['SkippedSuite/Test1.html', 'Suite/SkippedTest1.html', 'Suite/Test1.html'])

    def test_collect_tests_with_ignored_skipped_list(self):
        runner, port = self.create_runner(args=['--force'])

        self._add_file(runner, 'inspector', 'test1.html')
        self._add_file(runner, 'inspector', 'unsupported_test1.html')
        self._add_file(runner, 'inspector', 'test2.html')
        self._add_file(runner, 'inspector/resources', 'resource_file.html')
        self._add_file(runner, 'unsupported', 'unsupported_test2.html')
        port.skipped_perf_tests = lambda: ['inspector/unsupported_test1.html', 'unsupported']
        self.assertEqual(self._collect_tests_and_sort_test_name(runner), ['inspector/test1.html', 'inspector/test2.html', 'inspector/unsupported_test1.html', 'unsupported/unsupported_test2.html'])

    def test_collect_tests_should_ignore_replay_tests_by_default(self):
        runner, port = self.create_runner()
        self._add_file(runner, 'Replay', 'www.webkit.org.replay')
        self.assertEqual(runner._collect_tests(), [])

    def test_collect_tests_with_replay_tests(self):
        runner, port = self.create_runner(args=['--replay'])
        self._add_file(runner, 'Replay', 'www.webkit.org.replay')
        tests = runner._collect_tests()
        self.assertEqual(len(tests), 1)
        self.assertEqual(tests[0].__class__.__name__, 'ReplayPerfTest')

    def test_parse_args(self):
        runner, port = self.create_runner()
        options, args = PerfTestsRunner._parse_args([
                '--build-directory=folder42',
                '--platform=platform42',
                '--builder-name', 'webkit-mac-1',
                '--build-number=56',
                '--time-out-ms=42',
                '--no-show-results',
                '--reset-results',
                '--output-json-path=a/output.json',
                '--slave-config-json-path=a/source.json',
                '--test-results-server=somehost',
                '--debug'])
        self.assertEqual(options.build, True)
        self.assertEqual(options.build_directory, 'folder42')
        self.assertEqual(options.platform, 'platform42')
        self.assertEqual(options.builder_name, 'webkit-mac-1')
        self.assertEqual(options.build_number, '56')
        self.assertEqual(options.time_out_ms, '42')
        self.assertEqual(options.configuration, 'Debug')
        self.assertEqual(options.show_results, False)
        self.assertEqual(options.reset_results, True)
        self.assertEqual(options.output_json_path, 'a/output.json')
        self.assertEqual(options.slave_config_json_path, 'a/source.json')
        self.assertEqual(options.test_results_server, 'somehost')
