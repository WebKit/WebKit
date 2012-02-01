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

"""Unit tests for run_perf_tests."""

import StringIO
import json
import unittest

from webkitpy.common import array_stream
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.port.driver import DriverInput, DriverOutput
from webkitpy.layout_tests.port.test import TestPort
from webkitpy.layout_tests.views import printing
from webkitpy.performance_tests.perftestsrunner import PerfTestsRunner


class MainTest(unittest.TestCase):
    class TestDriver:
        def run_test(self, driver_input):
            text = ''
            timeout = False
            crash = False
            if driver_input.test_name.endswith('pass.html'):
                text = 'RESULT group_name: test_name= 42 ms'
            elif driver_input.test_name.endswith('timeout.html'):
                timeout = True
            elif driver_input.test_name.endswith('failed.html'):
                text = None
            elif driver_input.test_name.endswith('tonguey.html'):
                text = 'we are not expecting an output from perf tests but RESULT blablabla'
            elif driver_input.test_name.endswith('crash.html'):
                crash = True
            elif driver_input.test_name.endswith('event-target-wrapper.html'):
                text = """Running 20 times
Ignoring warm-up run (1502)
1504
1505
1510
1504
1507
1509
1510
1487
1488
1472
1472
1488
1473
1472
1475
1487
1486
1486
1475
1471

avg 1489.05
median 1487
stdev 14.46
min 1471
max 1510
"""
            elif driver_input.test_name.endswith('some-parser.html'):
                text = """Running 20 times
Ignoring warm-up run (1115)

avg 1100
median 1101
stdev 11
min 1080
max 1120
"""
            return DriverOutput(text, '', '', '', crash=crash, timeout=timeout)

        def stop(self):
            """do nothing"""

    def create_runner(self, buildbot_output=None, args=[], regular_output=None, driver_class=TestDriver):
        buildbot_output = buildbot_output or array_stream.ArrayStream()
        regular_output = regular_output or array_stream.ArrayStream()

        options, parsed_args = PerfTestsRunner._parse_args(args)
        test_port = TestPort(host=MockHost(), options=options)
        test_port.create_driver = lambda worker_number=None, no_timeout=False: driver_class()

        runner = PerfTestsRunner(regular_output, buildbot_output, args=args, port=test_port)
        runner._host.filesystem.maybe_make_directory(runner._base_path, 'inspector')
        runner._host.filesystem.maybe_make_directory(runner._base_path, 'Bindings')
        runner._host.filesystem.maybe_make_directory(runner._base_path, 'Parser')
        return runner

    def run_test(self, test_name):
        runner = self.create_runner()
        driver = MainTest.TestDriver()
        return runner._run_single_test(test_name, driver, is_chromium_style=True)

    def test_run_passing_test(self):
        self.assertTrue(self.run_test('pass.html'))

    def test_run_silent_test(self):
        self.assertFalse(self.run_test('silent.html'))

    def test_run_failed_test(self):
        self.assertFalse(self.run_test('failed.html'))

    def test_run_tonguey_test(self):
        self.assertFalse(self.run_test('tonguey.html'))

    def test_run_timeout_test(self):
        self.assertFalse(self.run_test('timeout.html'))

    def test_run_crash_test(self):
        self.assertFalse(self.run_test('crash.html'))

    def test_run_test_set(self):
        buildbot_output = array_stream.ArrayStream()
        runner = self.create_runner(buildbot_output)
        dirname = runner._base_path + '/inspector/'
        tests = [dirname + 'pass.html', dirname + 'silent.html', dirname + 'failed.html',
            dirname + 'tonguey.html', dirname + 'timeout.html', dirname + 'crash.html']
        unexpected_result_count = runner._run_tests_set(tests, runner._port)
        self.assertEqual(unexpected_result_count, len(tests) - 1)
        self.assertEqual(len(buildbot_output.get()), 1)
        self.assertEqual(buildbot_output.get()[0], 'RESULT group_name: test_name= 42 ms\n')

    def test_run_test_set_kills_drt_per_run(self):

        class TestDriverWithStopCount(MainTest.TestDriver):
            stop_count = 0

            def __init__(self):
                TestDriverWithStopCount.sotp_count = 0

            def stop(self):
                TestDriverWithStopCount.stop_count += 1

        buildbot_output = array_stream.ArrayStream()
        runner = self.create_runner(buildbot_output, driver_class=TestDriverWithStopCount)

        dirname = runner._base_path + '/inspector/'
        tests = [dirname + 'pass.html', dirname + 'silent.html', dirname + 'failed.html',
            dirname + 'tonguey.html', dirname + 'timeout.html', dirname + 'crash.html']

        unexpected_result_count = runner._run_tests_set(tests, runner._port)
        self.assertEqual(TestDriverWithStopCount.stop_count, 6)

    def test_run_test_set_for_parser_tests(self):
        buildbot_output = array_stream.ArrayStream()
        runner = self.create_runner(buildbot_output)
        tests = [runner._base_path + '/Bindings/event-target-wrapper.html', runner._base_path + '/Parser/some-parser.html']
        unexpected_result_count = runner._run_tests_set(tests, runner._port)
        self.assertEqual(unexpected_result_count, 0)
        self.assertEqual(buildbot_output.get()[0], 'RESULT Bindings: event-target-wrapper= 1489.05 ms\n')
        self.assertEqual(buildbot_output.get()[1], 'median= 1487.0 ms, stdev= 14.46 ms, min= 1471.0 ms, max= 1510.0 ms\n')
        self.assertEqual(buildbot_output.get()[2], 'RESULT Parser: some-parser= 1100.0 ms\n')
        self.assertEqual(buildbot_output.get()[3], 'median= 1101.0 ms, stdev= 11.0 ms, min= 1080.0 ms, max= 1120.0 ms\n')

    def test_run_test_set_with_json_output(self):
        buildbot_output = array_stream.ArrayStream()
        runner = self.create_runner(buildbot_output, args=['--output-json-path=/mock-checkout/output.json'])
        runner._host.filesystem.files[runner._base_path + '/inspector/pass.html'] = True
        runner._host.filesystem.files[runner._base_path + '/Bindings/event-target-wrapper.html'] = True
        runner._timestamp = 123456789
        self.assertEqual(runner.run(), 0)
        self.assertEqual(len(buildbot_output.get()), 3)
        self.assertEqual(buildbot_output.get()[0], 'RESULT Bindings: event-target-wrapper= 1489.05 ms\n')
        self.assertEqual(buildbot_output.get()[1], 'median= 1487.0 ms, stdev= 14.46 ms, min= 1471.0 ms, max= 1510.0 ms\n')
        self.assertEqual(buildbot_output.get()[2], 'RESULT group_name: test_name= 42 ms\n')

        self.assertEqual(json.loads(runner._host.filesystem.files['/mock-checkout/output.json']), {
            "timestamp": 123456789, "results":
            {"Bindings/event-target-wrapper": {"max": 1510, "avg": 1489.05, "median": 1487, "min": 1471, "stdev": 14.46},
            "group_name:test_name": 42},
            "revision": 1234})

    def test_run_test_set_with_json_source(self):
        buildbot_output = array_stream.ArrayStream()
        runner = self.create_runner(buildbot_output, args=['--output-json-path=/mock-checkout/output.json',
            '--source-json-path=/mock-checkout/source.json'])
        runner._host.filesystem.files['/mock-checkout/source.json'] = '{"key": "value"}'
        runner._host.filesystem.files[runner._base_path + '/inspector/pass.html'] = True
        runner._host.filesystem.files[runner._base_path + '/Bindings/event-target-wrapper.html'] = True
        runner._timestamp = 123456789
        self.assertEqual(runner.run(), 0)
        self.assertEqual(len(buildbot_output.get()), 3)
        self.assertEqual(buildbot_output.get()[0], 'RESULT Bindings: event-target-wrapper= 1489.05 ms\n')
        self.assertEqual(buildbot_output.get()[1], 'median= 1487.0 ms, stdev= 14.46 ms, min= 1471.0 ms, max= 1510.0 ms\n')
        self.assertEqual(buildbot_output.get()[2], 'RESULT group_name: test_name= 42 ms\n')

        self.assertEqual(json.loads(runner._host.filesystem.files['/mock-checkout/output.json']), {
            "timestamp": 123456789, "results":
            {"Bindings/event-target-wrapper": {"max": 1510, "avg": 1489.05, "median": 1487, "min": 1471, "stdev": 14.46},
            "group_name:test_name": 42},
            "revision": 1234,
            "key": "value"})

    def test_run_with_upload_json(self):
        runner = self.create_runner(args=['--output-json-path=/mock-checkout/output.json',
            '--test-results-server', 'some.host', '--platform', 'platform1', '--builder-name', 'builder1', '--build-number', '123'])
        upload_json_is_called = [False]
        upload_json_returns_true = True

        def mock_upload_json(hostname, json_path):
            self.assertEqual(hostname, 'some.host')
            self.assertEqual(json_path, '/mock-checkout/output.json')
            upload_json_is_called[0] = True
            return upload_json_returns_true

        runner._upload_json = mock_upload_json
        runner._host.filesystem.files['/mock-checkout/source.json'] = '{"key": "value"}'
        runner._host.filesystem.files[runner._base_path + '/inspector/pass.html'] = True
        runner._host.filesystem.files[runner._base_path + '/Bindings/event-target-wrapper.html'] = True
        runner._timestamp = 123456789
        self.assertEqual(runner.run(), 0)
        self.assertEqual(upload_json_is_called[0], True)
        generated_json = json.loads(runner._host.filesystem.files['/mock-checkout/output.json'])
        self.assertEqual(generated_json['platform'], 'platform1')
        self.assertEqual(generated_json['builder-name'], 'builder1')
        self.assertEqual(generated_json['build-number'], 123)
        upload_json_returns_true = False
        self.assertEqual(runner.run(), -3)

    def test_upload_json(self):
        regular_output = array_stream.ArrayStream()
        runner = self.create_runner(regular_output=regular_output)
        runner._host.filesystem.files['/mock-checkout/some.json'] = 'some content'

        called = []
        upload_single_text_file_throws = False
        upload_single_text_file_return_value = StringIO.StringIO('OK')

        class MockFileUploader:
            def __init__(mock, url, timeout):
                self.assertEqual(url, 'https://some.host/api/test/report')
                self.assertTrue(isinstance(timeout, int) and timeout)
                called.append('FileUploader')

            def upload_single_text_file(mock, filesystem, content_type, filename):
                self.assertEqual(filesystem, runner._host.filesystem)
                self.assertEqual(content_type, 'application/json')
                self.assertEqual(filename, 'some.json')
                called.append('upload_single_text_file')
                if upload_single_text_file_throws:
                    raise "Some exception"
                return upload_single_text_file_return_value

        runner._upload_json('some.host', 'some.json', MockFileUploader)
        self.assertEqual(called, ['FileUploader', 'upload_single_text_file'])

        output = OutputCapture()
        output.capture_output()
        upload_single_text_file_return_value = StringIO.StringIO('Some error')
        runner._upload_json('some.host', 'some.json', MockFileUploader)
        _, _, logs = output.restore_output()
        self.assertEqual(logs, 'Uploaded JSON but got a bad response:\nSome error\n')

        # Throwing an exception upload_single_text_file shouldn't blow up _upload_json
        called = []
        upload_single_text_file_throws = True
        runner._upload_json('some.host', 'some.json', MockFileUploader)
        self.assertEqual(called, ['FileUploader', 'upload_single_text_file'])

    def test_collect_tests(self):
        runner = self.create_runner()
        filename = runner._host.filesystem.join(runner._base_path, 'inspector', 'a_file.html')
        runner._host.filesystem.files[filename] = 'a content'
        tests = runner._collect_tests()
        self.assertEqual(len(tests), 1)

    def test_collect_tests_with_skipped_list(self):
        runner = self.create_runner()

        def add_file(dirname, filename, content=True):
            dirname = runner._host.filesystem.join(runner._base_path, dirname) if dirname else runner._base_path
            runner._host.filesystem.maybe_make_directory(dirname)
            runner._host.filesystem.files[runner._host.filesystem.join(dirname, filename)] = content

        add_file('inspector', 'test1.html')
        add_file('inspector', 'unsupported_test1.html')
        add_file('inspector', 'test2.html')
        add_file('inspector/resources', 'resource_file.html')
        add_file('unsupported', 'unsupported_test2.html')
        runner._port.skipped_perf_tests = lambda: ['inspector/unsupported_test1.html', 'unsupported']
        tests = [runner._port.relative_perf_test_filename(test) for test in runner._collect_tests()]
        self.assertEqual(sorted(tests), ['inspector/test1.html', 'inspector/test2.html'])

    def test_parse_args(self):
        runner = self.create_runner()
        options, args = PerfTestsRunner._parse_args([
                '--verbose',
                '--build-directory=folder42',
                '--platform=platform42',
                '--builder-name', 'webkit-mac-1',
                '--build-number=56',
                '--time-out-ms=42',
                '--output-json-path=a/output.json',
                '--source-json-path=a/source.json',
                '--test-results-server=somehost',
                '--debug', 'an_arg'])
        self.assertEqual(options.build, True)
        self.assertEqual(options.verbose, True)
        self.assertEqual(options.help_printing, None)
        self.assertEqual(options.build_directory, 'folder42')
        self.assertEqual(options.platform, 'platform42')
        self.assertEqual(options.builder_name, 'webkit-mac-1')
        self.assertEqual(options.build_number, '56')
        self.assertEqual(options.time_out_ms, '42')
        self.assertEqual(options.configuration, 'Debug')
        self.assertEqual(options.print_options, None)
        self.assertEqual(options.output_json_path, 'a/output.json')
        self.assertEqual(options.source_json_path, 'a/source.json')
        self.assertEqual(options.test_results_server, 'somehost')


if __name__ == '__main__':
    unittest.main()
