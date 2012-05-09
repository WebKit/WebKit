#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2011 Apple Inc. All rights reserved.
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

import codecs
import itertools
import json
import logging
import platform
import Queue
import re
import StringIO
import sys
import thread
import time
import threading
import unittest

from webkitpy.common.system import outputcapture, path
from webkitpy.common.system.crashlogs_unittest import make_mock_crash_report_darwin
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.host_mock import MockHost

from webkitpy.layout_tests import port
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.port import Port
from webkitpy.layout_tests.port.test import TestPort, TestDriver
from webkitpy.test.skip import skip_if
from webkitpy.tool.mocktool import MockOptions


def parse_args(extra_args=None, record_results=False, tests_included=False, new_results=False, print_nothing=True):
    extra_args = extra_args or []
    if print_nothing:
        args = ['--print', 'nothing']
    else:
        args = []
    if not '--platform' in extra_args:
        args.extend(['--platform', 'test'])
    if not record_results:
        args.append('--no-record-results')
    if not new_results:
        args.append('--no-new-test-results')

    if not '--child-processes' in extra_args:
        args.extend(['--child-processes', 1])
    args.extend(extra_args)
    if not tests_included:
        # We use the glob to test that globbing works.
        args.extend(['passes',
                     'http/tests',
                     'websocket/tests',
                     'failures/expected/*'])
    return run_webkit_tests.parse_args(args)


def passing_run(extra_args=None, port_obj=None, record_results=False, tests_included=False, host=None):
    options, parsed_args = parse_args(extra_args, record_results, tests_included)
    if not port_obj:
        host = host or MockHost()
        port_obj = host.port_factory.get(port_name=options.platform, options=options)
    buildbot_output = StringIO.StringIO()
    regular_output = StringIO.StringIO()
    res = run_webkit_tests.run(port_obj, options, parsed_args, buildbot_output=buildbot_output, regular_output=regular_output)
    return res == 0 and not regular_output.getvalue() and not buildbot_output.getvalue()


def logging_run(extra_args=None, port_obj=None, record_results=False, tests_included=False, host=None, new_results=False):
    options, parsed_args = parse_args(extra_args=extra_args,
                                      record_results=record_results,
                                      tests_included=tests_included,
                                      print_nothing=False, new_results=new_results)
    host = host or MockHost()
    if not port_obj:
        port_obj = host.port_factory.get(port_name=options.platform, options=options)

    res, buildbot_output, regular_output = run_and_capture(port_obj, options, parsed_args)
    return (res, buildbot_output, regular_output, host.user)


def run_and_capture(port_obj, options, parsed_args):
    oc = outputcapture.OutputCapture()
    try:
        oc.capture_output()
        buildbot_output = StringIO.StringIO()
        regular_output = StringIO.StringIO()
        res = run_webkit_tests.run(port_obj, options, parsed_args,
                                   buildbot_output=buildbot_output,
                                   regular_output=regular_output)
    finally:
        oc.restore_output()
    return (res, buildbot_output, regular_output)


def get_tests_run(extra_args=None, tests_included=False, flatten_batches=False,
                  host=None, include_reference_html=False):
    extra_args = extra_args or []
    if not tests_included:
        # Not including http tests since they get run out of order (that
        # behavior has its own test, see test_get_test_file_queue)
        extra_args = ['passes', 'failures'] + extra_args
    options, parsed_args = parse_args(extra_args, tests_included=True)

    host = host or MockHost()
    test_batches = []

    class RecordingTestDriver(TestDriver):
        def __init__(self, port, worker_number):
            TestDriver.__init__(self, port, worker_number, pixel_tests=port.get_option('pixel_test'), no_timeout=False)
            self._current_test_batch = None

        def start(self):
            pass

        def stop(self):
            self._current_test_batch = None

        def run_test(self, test_input):
            if self._current_test_batch is None:
                self._current_test_batch = []
                test_batches.append(self._current_test_batch)
            test_name = test_input.test_name
            # In case of reftest, one test calls the driver's run_test() twice.
            # We should not add a reference html used by reftests to tests unless include_reference_html parameter
            # is explicitly given.
            filesystem = self._port.host.filesystem
            dirname, filename = filesystem.split(test_name)
            if include_reference_html or not Port.is_reference_html_file(filesystem, dirname, filename):
                self._current_test_batch.append(test_name)
            return TestDriver.run_test(self, test_input)

    class RecordingTestPort(TestPort):
        def create_driver(self, worker_number):
            return RecordingTestDriver(self, worker_number)

    recording_port = RecordingTestPort(host, options=options)
    run_and_capture(recording_port, options, parsed_args)

    if flatten_batches:
        return list(itertools.chain(*test_batches))

    return test_batches


# Update this magic number if you add an unexpected test to webkitpy.layout_tests.port.test
# FIXME: It's nice to have a routine in port/test.py that returns this number.
unexpected_tests_count = 12


class StreamTestingMixin(object):
    def assertContains(self, stream, string):
        self.assertTrue(string in stream.getvalue())

    def assertContainsLine(self, stream, string):
        self.assertTrue(string in stream.buflist)

    def assertEmpty(self, stream):
        self.assertFalse(stream.getvalue())

    def assertNotEmpty(self, stream):
        self.assertTrue(stream.getvalue())


class LintTest(unittest.TestCase, StreamTestingMixin):
    def test_all_configurations(self):

        class FakePort(object):
            def __init__(self, name, path):
                self.name = name
                self.path = path

            def test_expectations(self):
                return ''

            def path_to_test_expectations_file(self):
                return self.path

            def test_configuration(self):
                return None

            def test_expectations_overrides(self):
                return None

        class FakeFactory(object):
            def __init__(self, host, ports):
                self.host = host
                self.ports = {}
                for port in ports:
                    self.ports[port.name] = port
                    port.host = host
                    port.factory = self

            def get(self, port_name, *args, **kwargs):
                return self.ports[port_name]

            def all_port_names(self):
                return sorted(self.ports.keys())

        class FakeExpectationsParser(object):
            def __init__(self, port, *args, **kwargs):
                port.host.ports_parsed.append(port.name)

        host = MockHost()
        host.ports_parsed = []
        host.port_factory = FakeFactory(host, (FakePort('a', 'path-to-a'),
                                               FakePort('b', 'path-to-b'),
                                               FakePort('b-win', 'path-to-b')))

        self.assertEquals(run_webkit_tests.lint(host.port_factory.ports['a'], MockOptions(platform=None), FakeExpectationsParser), 0)
        self.assertEquals(host.ports_parsed, ['a', 'b'])

        host.ports_parsed = []
        self.assertEquals(run_webkit_tests.lint(host.port_factory.ports['a'], MockOptions(platform='a'), FakeExpectationsParser), 0)
        self.assertEquals(host.ports_parsed, ['a'])

    def test_lint_test_files(self):
        res, out, err, user = logging_run(['--lint-test-files'])
        self.assertEqual(res, 0)
        self.assertEmpty(out)
        self.assertContains(err, 'Lint succeeded')

    def test_lint_test_files__errors(self):
        options, parsed_args = parse_args(['--lint-test-files'])
        host = MockHost()
        port_obj = host.port_factory.get(options.platform, options=options)
        port_obj.test_expectations = lambda: "# syntax error"
        res, out, err = run_and_capture(port_obj, options, parsed_args)

        self.assertEqual(res, -1)
        self.assertEmpty(out)
        self.assertTrue(any(['Lint failed' in msg for msg in err.buflist]))


class MainTest(unittest.TestCase, StreamTestingMixin):
    def setUp(self):
        # A real PlatformInfo object is used here instead of a
        # MockPlatformInfo because we need to actually check for
        # Windows and Mac to skip some tests.
        self._platform = SystemHost().platform

        # FIXME: Remove this when we fix test-webkitpy to work
        # properly on cygwin (bug 63846).
        self.should_test_processes = not self._platform.is_win()

    def test_accelerated_compositing(self):
        # This just tests that we recognize the command line args
        self.assertTrue(passing_run(['--accelerated-video']))
        self.assertTrue(passing_run(['--no-accelerated-video']))

    def test_accelerated_2d_canvas(self):
        # This just tests that we recognize the command line args
        self.assertTrue(passing_run(['--accelerated-2d-canvas']))
        self.assertTrue(passing_run(['--no-accelerated-2d-canvas']))

    def test_all(self):
        res, out, err, user = logging_run([], tests_included=True)
        self.assertEquals(res, unexpected_tests_count)

    def test_basic(self):
        self.assertTrue(passing_run())

    def test_batch_size(self):
        batch_tests_run = get_tests_run(['--batch-size', '2'])
        for batch in batch_tests_run:
            self.assertTrue(len(batch) <= 2, '%s had too many tests' % ', '.join(batch))

    def test_child_processes_2(self):
        if self.should_test_processes:
            _, _, regular_output, _ = logging_run(
                ['--print', 'config', '--child-processes', '2'])
            self.assertTrue(any(['Running 2 ' in line for line in regular_output.buflist]))

    def test_child_processes_min(self):
        if self.should_test_processes:
            _, _, regular_output, _ = logging_run(
                ['--print', 'config', '--child-processes', '2', 'passes'],
                tests_included=True)
            self.assertTrue(any(['Running 1 ' in line for line in regular_output.buflist]))

    def test_dryrun(self):
        batch_tests_run = get_tests_run(['--dry-run'])
        self.assertEqual(batch_tests_run, [])

        batch_tests_run = get_tests_run(['-n'])
        self.assertEqual(batch_tests_run, [])

    def test_exception_raised(self):
        # Exceptions raised by a worker are treated differently depending on
        # whether they are in-process or out. inline exceptions work as normal,
        # which allows us to get the full stack trace and traceback from the
        # worker. The downside to this is that it could be any error, but this
        # is actually useful in testing.
        #
        # Exceptions raised in a separate process are re-packaged into
        # WorkerExceptions, which have a string capture of the stack which can
        # be printed, but don't display properly in the unit test exception handlers.
        self.assertRaises(ValueError, logging_run,
            ['failures/expected/exception.html', '--child-processes', '1'], tests_included=True)

        if self.should_test_processes:
            self.assertRaises(run_webkit_tests.WorkerException, logging_run,
                ['--child-processes', '2', '--force', 'failures/expected/exception.html', 'passes/text.html'], tests_included=True)

    def test_full_results_html(self):
        # FIXME: verify html?
        res, out, err, user = logging_run(['--full-results-html'])
        self.assertEqual(res, 0)

    def test_help_printing(self):
        res, out, err, user = logging_run(['--help-printing'])
        self.assertEqual(res, 0)
        self.assertEmpty(out)
        self.assertNotEmpty(err)

    def test_hung_thread(self):
        res, out, err, user = logging_run(['--run-singly', '--time-out-ms=50',
                                          'failures/expected/hang.html'],
                                          tests_included=True)
        self.assertEqual(res, 0)
        self.assertNotEmpty(out)
        self.assertNotEmpty(err)

    def test_keyboard_interrupt(self):
        # Note that this also tests running a test marked as SKIP if
        # you specify it explicitly.
        self.assertRaises(KeyboardInterrupt, logging_run,
            ['failures/expected/keyboard.html', '--child-processes', '1'],
            tests_included=True)

        if self.should_test_processes:
            self.assertRaises(KeyboardInterrupt, logging_run,
                ['failures/expected/keyboard.html', 'passes/text.html', '--child-processes', '2', '--force'], tests_included=True)

    def test_no_tests_found(self):
        res, out, err, user = logging_run(['resources'], tests_included=True)
        self.assertEqual(res, -1)
        self.assertEmpty(out)
        self.assertContainsLine(err, 'No tests to run.\n')

    def test_no_tests_found_2(self):
        res, out, err, user = logging_run(['foo'], tests_included=True)
        self.assertEqual(res, -1)
        self.assertEmpty(out)
        self.assertContainsLine(err, 'No tests to run.\n')

    def test_randomize_order(self):
        # FIXME: verify order was shuffled
        self.assertTrue(passing_run(['--randomize-order']))

    def test_gc_between_tests(self):
        self.assertTrue(passing_run(['--gc-between-tests']))

    def test_complex_text(self):
        self.assertTrue(passing_run(['--complex-text']))

    def test_threaded(self):
        self.assertTrue(passing_run(['--threaded']))

    def test_repeat_each(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--repeat-each', '2'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(tests_run, ['passes/image.html', 'passes/image.html', 'passes/text.html', 'passes/text.html'])

    def test_skip_pixel_test_if_no_baseline_option(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--skip-pixel-test-if-no-baseline'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(tests_run, ['passes/image.html', 'passes/text.html'])

    def test_ignore_tests(self):
        def assert_ignored(args, tests_expected_to_run):
            tests_to_run = ['failures/expected/image.html', 'passes/image.html']
            tests_run = get_tests_run(args + tests_to_run, tests_included=True, flatten_batches=True)
            self.assertEquals(tests_run, tests_expected_to_run)

        assert_ignored(['-i', 'failures/expected/image.html'], ['passes/image.html'])
        assert_ignored(['-i', 'passes'], ['failures/expected/image.html'])

        # Note here that there is an expectation for failures/expected/image.html already, but
        # it is overriden by the command line arg. This might be counter-intuitive.
        # FIXME: This isn't currently working ...
        # assert_ignored(['-i', 'failures/expected'], ['passes/image.html'])

    def test_iterations(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--iterations', '2'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(tests_run, ['passes/image.html', 'passes/text.html', 'passes/image.html', 'passes/text.html'])

    def test_repeat_each_iterations_num_tests(self):
        # The total number of tests should be: number_of_tests *
        # repeat_each * iterations
        host = MockHost()
        res, out, err, _ = logging_run(['--iterations', '2',
                                        '--repeat-each', '4',
                                        '--print', 'everything',
                                        'passes/text.html', 'failures/expected/text.html'],
                                       tests_included=True, host=host, record_results=True)
        self.assertContainsLine(out, "=> Results: 8/16 tests passed (50.0%)\n")
        self.assertContainsLine(err, "All 16 tests ran as expected.\n")

    def test_run_chunk(self):
        # Test that we actually select the right chunk
        all_tests_run = get_tests_run(flatten_batches=True)
        chunk_tests_run = get_tests_run(['--run-chunk', '1:4'], flatten_batches=True)
        self.assertEquals(all_tests_run[4:8], chunk_tests_run)

        # Test that we wrap around if the number of tests is not evenly divisible by the chunk size
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']
        chunk_tests_run = get_tests_run(['--run-chunk', '1:3'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/text.html', 'passes/error.html', 'passes/image.html'], chunk_tests_run)

    def test_run_force(self):
        # This raises an exception because we run
        # failures/expected/exception.html, which is normally SKIPped.

        # See also the comments in test_exception_raised() about ValueError vs. WorkerException.
        self.assertRaises(ValueError, logging_run, ['--force'])

    def test_run_part(self):
        # Test that we actually select the right part
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']
        tests_run = get_tests_run(['--run-part', '1:2'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/error.html', 'passes/image.html'], tests_run)

        # Test that we wrap around if the number of tests is not evenly divisible by the chunk size
        # (here we end up with 3 parts, each with 2 tests, and we only have 4 tests total, so the
        # last part repeats the first two tests).
        chunk_tests_run = get_tests_run(['--run-part', '3:3'] + tests_to_run, tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/error.html', 'passes/image.html'], chunk_tests_run)

    def test_run_singly(self):
        batch_tests_run = get_tests_run(['--run-singly'])
        for batch in batch_tests_run:
            self.assertEquals(len(batch), 1, '%s had too many tests' % ', '.join(batch))

    def test_skip_failing_tests(self):
        # This tests that we skip both known failing and known flaky tests. Because there are
        # no known flaky tests in the default test_expectations, we add additional expectations.
        host = MockHost()
        host.filesystem.write_text_file('/tmp/overrides.txt', 'BUGX : passes/image.html = IMAGE PASS\n')

        batches = get_tests_run(['--skip-failing-tests', '--additional-expectations', '/tmp/overrides.txt'], host=host)
        has_passes_text = False
        for batch in batches:
            self.assertFalse('failures/expected/text.html' in batch)
            self.assertFalse('passes/image.html' in batch)
            has_passes_text = has_passes_text or ('passes/text.html' in batch)
        self.assertTrue(has_passes_text)

    def test_run_singly_actually_runs_tests(self):
        res, _, _, _ = logging_run(['--run-singly', 'failures/unexpected'])
        self.assertEquals(res, 8)

    def test_single_file(self):
        # FIXME: We should consider replacing more of the get_tests_run()-style tests
        # with tests that read the tests_run* files, like this one.
        host = MockHost()
        tests_run = passing_run(['passes/text.html'], tests_included=True, host=host)
        self.assertEquals(host.filesystem.read_text_file('/tmp/layout-test-results/tests_run0.txt'),
                          'passes/text.html\n')

    def test_single_file_with_prefix(self):
        tests_run = get_tests_run(['LayoutTests/passes/text.html'], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/text.html'], tests_run)

    def test_single_skipped_file(self):
        tests_run = get_tests_run(['failures/expected/keybaord.html'], tests_included=True, flatten_batches=True)
        self.assertEquals([], tests_run)

    def test_stderr_is_saved(self):
        host = MockHost()
        self.assertTrue(passing_run(host=host))
        self.assertEquals(host.filesystem.read_text_file('/tmp/layout-test-results/passes/error-stderr.txt'),
                          'stuff going to stderr')

    def test_test_list(self):
        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, 'passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], tests_included=True, flatten_batches=True, host=host)
        self.assertEquals(['passes/text.html'], tests_run)
        host.filesystem.remove(filename)
        res, out, err, user = logging_run(['--test-list=%s' % filename],
                                          tests_included=True, host=host)
        self.assertEqual(res, -1)
        self.assertNotEmpty(err)

    def test_test_list_with_prefix(self):
        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, 'LayoutTests/passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], tests_included=True, flatten_batches=True, host=host)
        self.assertEquals(['passes/text.html'], tests_run)

    def test_unexpected_failures(self):
        # Run tests including the unexpected failures.
        self._url_opened = None
        res, out, err, user = logging_run(tests_included=True)

        self.assertEqual(res, unexpected_tests_count)
        self.assertNotEmpty(out)
        self.assertNotEmpty(err)
        self.assertEqual(user.opened_urls, [path.abspath_to_uri('/tmp/layout-test-results/results.html')])

    def test_missing_and_unexpected_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        res, out, err, _ = logging_run(['--no-show-results',
            'failures/expected/missing_image.html',
            'failures/unexpected/missing_text.html',
            'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host, record_results=True)
        file_list = host.filesystem.written_files.keys()
        file_list.remove('/tmp/layout-test-results/tests_run0.txt')
        self.assertEquals(res, 1)
        expected_token = '"unexpected":{"text-image-checksum.html":{"expected":"PASS","actual":"TEXT"},"missing_text.html":{"expected":"PASS","is_missing_text":true,"actual":"MISSING"}'
        json_string = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        self.assertTrue(json_string.find(expected_token) != -1)
        self.assertTrue(json_string.find('"num_regressions":1') != -1)
        self.assertTrue(json_string.find('"num_flaky":0') != -1)
        self.assertTrue(json_string.find('"num_missing":1') != -1)

    def test_missing_and_unexpected_results_with_custom_exit_code(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        class CustomExitCodePort(TestPort):
            def exit_code_from_summarized_results(self, unexpected_results):
                return unexpected_results['num_regressions'] + unexpected_results['num_missing']

        host = MockHost()
        options, parsed_args = run_webkit_tests.parse_args(['--pixel-tests', '--no-new-test-results'])
        test_port = CustomExitCodePort(host, options=options)
        res, out, err, _ = logging_run(['--no-show-results',
            'failures/expected/missing_image.html',
            'failures/unexpected/missing_text.html',
            'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host, record_results=True, port_obj=test_port)
        self.assertEquals(res, 2)

    def test_crash_with_stderr(self):
        host = MockHost()
        res, buildbot_output, regular_output, user = logging_run([
                'failures/unexpected/crash-with-stderr.html',
            ],
            tests_included=True,
            record_results=True,
            host=host)
        self.assertTrue(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json').find('{"crash-with-stderr.html":{"expected":"PASS","actual":"CRASH","has_stderr":true}}') != -1)

    def test_no_image_failure_with_image_diff(self):
        host = MockHost()
        res, buildbot_output, regular_output, user = logging_run([
                'failures/unexpected/checksum-with-matching-image.html',
            ],
            tests_included=True,
            record_results=True,
            host=host)
        self.assertTrue(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json').find('"num_regressions":0') != -1)

    def test_crash_log(self):
        # FIXME: Need to rewrite these tests to not be mac-specific, or move them elsewhere.
        # Currently CrashLog uploading only works on Darwin.
        if not self._platform.is_mac():
            return
        mock_crash_report = make_mock_crash_report_darwin('DumpRenderTree', 12345)
        host = MockHost()
        host.filesystem.write_text_file('/Users/mock/Library/Logs/DiagnosticReports/DumpRenderTree_2011-06-13-150719_quadzen.crash', mock_crash_report)
        res, buildbot_output, regular_output, user = logging_run([
                'failures/unexpected/crash-with-stderr.html',
            ],
            tests_included=True,
            record_results=True,
            host=host)
        expected_crash_log = mock_crash_report
        self.assertEquals(host.filesystem.read_text_file('/tmp/layout-test-results/failures/unexpected/crash-with-stderr-crash-log.txt'), expected_crash_log)

    def test_web_process_crash_log(self):
        # FIXME: Need to rewrite these tests to not be mac-specific, or move them elsewhere.
        # Currently CrashLog uploading only works on Darwin.
        if not self._platform.is_mac():
            return
        mock_crash_report = make_mock_crash_report_darwin('WebProcess', 12345)
        host = MockHost()
        host.filesystem.write_text_file('/Users/mock/Library/Logs/DiagnosticReports/WebProcess_2011-06-13-150719_quadzen.crash', mock_crash_report)
        res, buildbot_output, regular_output, user = logging_run([
                'failures/unexpected/web-process-crash-with-stderr.html',
            ],
            tests_included=True,
            record_results=True,
            host=host)
        self.assertEquals(host.filesystem.read_text_file('/tmp/layout-test-results/failures/unexpected/web-process-crash-with-stderr-crash-log.txt'), mock_crash_report)

    def test_exit_after_n_failures_upload(self):
        host = MockHost()
        res, buildbot_output, regular_output, user = logging_run([
                'failures/unexpected/text-image-checksum.html',
                'passes/text.html',
                '--exit-after-n-failures', '1',
            ],
            tests_included=True,
            record_results=True,
            host=host)

        # By returning False, we know that the incremental results were generated and then deleted.
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/incremental_results.json'))

        # This checks that we report only the number of tests that actually failed.
        self.assertEquals(res, 1)

        # This checks that passes/text.html is considered SKIPped.
        self.assertTrue('"skipped":1' in host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))

        # This checks that we told the user we bailed out.
        self.assertTrue('Exiting early after 1 failures. 1 tests run.\n' in regular_output.getvalue())

        # This checks that neither test ran as expected.
        # FIXME: This log message is confusing; tests that were skipped should be called out separately.
        self.assertTrue('0 tests ran as expected, 2 didn\'t:\n' in regular_output.getvalue())

    def test_exit_after_n_failures(self):
        # Unexpected failures should result in tests stopping.
        tests_run = get_tests_run([
                'failures/unexpected/text-image-checksum.html',
                'passes/text.html',
                '--exit-after-n-failures', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/unexpected/text-image-checksum.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run([
                'failures/expected/text.html',
                'passes/text.html',
                '--exit-after-n-failures', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/expected/text.html', 'passes/text.html'], tests_run)

    def test_exit_after_n_crashes(self):
        # Unexpected crashes should result in tests stopping.
        tests_run = get_tests_run([
                'failures/unexpected/crash.html',
                'passes/text.html',
                '--exit-after-n-crashes-or-timeouts', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/unexpected/crash.html'], tests_run)

        # Same with timeouts.
        tests_run = get_tests_run([
                'failures/unexpected/timeout.html',
                'passes/text.html',
                '--exit-after-n-crashes-or-timeouts', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/unexpected/timeout.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run([
                'failures/expected/crash.html',
                'passes/text.html',
                '--exit-after-n-crashes-or-timeouts', '1',
            ],
            tests_included=True,
            flatten_batches=True)
        self.assertEquals(['failures/expected/crash.html', 'passes/text.html'], tests_run)

    def test_results_directory_absolute(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        host = MockHost()
        with host.filesystem.mkdtemp() as tmpdir:
            res, out, err, user = logging_run(['--results-directory=' + str(tmpdir)],
                                              tests_included=True, host=host)
            self.assertEqual(user.opened_urls, [path.abspath_to_uri(host.filesystem.join(tmpdir, 'results.html'))])

    def test_results_directory_default(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        # This is the default location.
        res, out, err, user = logging_run(tests_included=True)
        self.assertEqual(user.opened_urls, [path.abspath_to_uri('/tmp/layout-test-results/results.html')])

    def test_results_directory_relative(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.
        host = MockHost()
        host.filesystem.maybe_make_directory('/tmp/cwd')
        host.filesystem.chdir('/tmp/cwd')
        res, out, err, user = logging_run(['--results-directory=foo'],
                                          tests_included=True, host=host)
        self.assertEqual(user.opened_urls, [path.abspath_to_uri('/tmp/cwd/foo/results.html')])

    def test_retrying_and_flaky_tests(self):
        host = MockHost()
        res, out, err, _ = logging_run(['failures/flaky'], tests_included=True, host=host)
        self.assertEquals(res, 0)
        self.assertTrue('Retrying' in err.getvalue())
        self.assertTrue('Unexpected flakiness' in out.getvalue())
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/flaky/text-actual.txt'))
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/retries/tests_run0.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retries/failures/flaky/text-actual.txt'))

        # Now we test that --clobber-old-results does remove the old entries and the old retries,
        # and that we don't retry again.
        res, out, err, _ = logging_run(['--no-retry-failures', '--clobber-old-results', 'failures/flaky'], tests_included=True, host=host)
        self.assertEquals(res, 1)
        self.assertTrue('Clobbering old results' in err.getvalue())
        self.assertTrue('flaky/text.html' in err.getvalue())
        self.assertTrue('Unexpected text diff' in out.getvalue())
        self.assertFalse('Unexpected flakiness' in out.getvalue())
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/flaky/text-actual.txt'))
        self.assertFalse(host.filesystem.exists('retries'))


    # These next tests test that we run the tests in ascending alphabetical
    # order per directory. HTTP tests are sharded separately from other tests,
    # so we have to test both.
    def assert_run_order(self, child_processes='1'):
        tests_run = get_tests_run(['--child-processes', child_processes, 'passes'],
            tests_included=True, flatten_batches=True)
        self.assertEquals(tests_run, sorted(tests_run))

        tests_run = get_tests_run(['--child-processes', child_processes, 'http/tests/passes'],
            tests_included=True, flatten_batches=True)
        self.assertEquals(tests_run, sorted(tests_run))

    def test_run_order__inline(self):
        self.assert_run_order()

    def test_tolerance(self):
        class ImageDiffTestPort(TestPort):
            def diff_image(self, expected_contents, actual_contents, tolerance=None):
                self.tolerance_used_for_diff_image = self._options.tolerance
                return (True, 1)

        def get_port_for_run(args):
            options, parsed_args = run_webkit_tests.parse_args(args)
            host = MockHost()
            test_port = ImageDiffTestPort(host, options=options)
            res = passing_run(args, port_obj=test_port, tests_included=True)
            self.assertTrue(res)
            return test_port

        base_args = ['--pixel-tests', '--no-new-test-results', 'failures/expected/*']

        # If we pass in an explicit tolerance argument, then that will be used.
        test_port = get_port_for_run(base_args + ['--tolerance', '.1'])
        self.assertEqual(0.1, test_port.tolerance_used_for_diff_image)
        test_port = get_port_for_run(base_args + ['--tolerance', '0'])
        self.assertEqual(0, test_port.tolerance_used_for_diff_image)

        # Otherwise the port's default tolerance behavior (including ignoring it)
        # should be used.
        test_port = get_port_for_run(base_args)
        self.assertEqual(None, test_port.tolerance_used_for_diff_image)

    def test_virtual(self):
        self.assertTrue(passing_run(['passes/text.html', 'passes/args.html',
                                     'virtual/passes/text.html', 'virtual/passes/args.html']))

    def test_reftest_run(self):
        tests_run = get_tests_run(['passes/reftest.html'], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/reftest.html'], tests_run)

    def test_reftest_run_reftests_if_pixel_tests_are_disabled(self):
        tests_run = get_tests_run(['--no-pixel-tests', 'passes/reftest.html'], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/reftest.html'], tests_run)

    def test_reftest_skip_reftests_if_no_ref_tests(self):
        tests_run = get_tests_run(['--no-ref-tests', 'passes/reftest.html'], tests_included=True, flatten_batches=True)
        self.assertEquals([], tests_run)
        tests_run = get_tests_run(['--no-ref-tests', '--no-pixel-tests', 'passes/reftest.html'], tests_included=True, flatten_batches=True)
        self.assertEquals([], tests_run)

    def test_reftest_expected_html_should_be_ignored(self):
        tests_run = get_tests_run(['passes/reftest-expected.html'], tests_included=True, flatten_batches=True)
        self.assertEquals([], tests_run)

    def test_reftest_driver_should_run_expected_html(self):
        tests_run = get_tests_run(['passes/reftest.html'], tests_included=True, flatten_batches=True, include_reference_html=True)
        self.assertEquals(['passes/reftest.html', 'passes/reftest-expected.html'], tests_run)

    def test_reftest_driver_should_run_expected_mismatch_html(self):
        tests_run = get_tests_run(['passes/mismatch.html'], tests_included=True, flatten_batches=True, include_reference_html=True)
        self.assertEquals(['passes/mismatch.html', 'passes/mismatch-expected-mismatch.html'], tests_run)

    def test_reftest_should_not_use_naming_convention_if_not_listed_in_reftestlist(self):
        host = MockHost()
        res, out, err, _ = logging_run(['--no-show-results', 'reftests/foo/'], tests_included=True, host=host, record_results=True)
        json_string = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        self.assertTrue(json_string.find('"unlistedtest.html":{"expected":"PASS","is_missing_text":true,"actual":"MISSING","is_missing_image":true}') != -1)
        self.assertTrue(json_string.find('"num_regressions":4') != -1)
        self.assertTrue(json_string.find('"num_flaky":0') != -1)
        self.assertTrue(json_string.find('"num_missing":1') != -1)

    def test_additional_platform_directory(self):
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/../foo']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo', '--additional-platform-directory', '/tmp/bar']))

        res, buildbot_output, regular_output, user = logging_run(['--additional-platform-directory', 'foo'])
        self.assertContainsLine(regular_output, '--additional-platform-directory=foo is ignored since it is not absolute\n')

    def test_additional_expectations(self):
        host = MockHost()
        host.filesystem.write_text_file('/tmp/overrides.txt', 'BUGX : failures/unexpected/mismatch.html = IMAGE\n')
        self.assertTrue(passing_run(['--additional-expectations', '/tmp/overrides.txt', 'failures/unexpected/mismatch.html'],
                                     tests_included=True, host=host))

    def test_no_http_and_force(self):
        # See test_run_force, using --force raises an exception.
        # FIXME: We would like to check the warnings generated.
        self.assertRaises(ValueError, logging_run, ['--force', '--no-http'])

    @staticmethod
    def has_test_of_type(tests, type):
        return [test for test in tests if type in test]

    def test_no_http_tests(self):
        batch_tests_dryrun = get_tests_run(['LayoutTests/http', 'websocket/'], flatten_batches=True)
        self.assertTrue(MainTest.has_test_of_type(batch_tests_dryrun, 'http'))
        self.assertTrue(MainTest.has_test_of_type(batch_tests_dryrun, 'websocket'))

        batch_tests_run_no_http = get_tests_run(['--no-http', 'LayoutTests/http', 'websocket/'], flatten_batches=True)
        self.assertFalse(MainTest.has_test_of_type(batch_tests_run_no_http, 'http'))
        self.assertFalse(MainTest.has_test_of_type(batch_tests_run_no_http, 'websocket'))

        batch_tests_run_http = get_tests_run(['--http', 'LayoutTests/http', 'websocket/'], flatten_batches=True)
        self.assertTrue(MainTest.has_test_of_type(batch_tests_run_http, 'http'))
        self.assertTrue(MainTest.has_test_of_type(batch_tests_run_http, 'websocket'))


class EndToEndTest(unittest.TestCase):
    def parse_full_results(self, full_results_text):
        json_to_eval = full_results_text.replace("ADD_RESULTS(", "").replace(");", "")
        compressed_results = json.loads(json_to_eval)
        return compressed_results

    def test_end_to_end(self):
        host = MockHost()
        res, out, err, user = logging_run(record_results=True, tests_included=True, host=host)

        self.assertEquals(res, unexpected_tests_count)
        results = self.parse_full_results(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))

        # Check to ensure we're passing back image diff %age correctly.
        self.assertEquals(results['tests']['failures']['expected']['image.html']['image_diff_percent'], 1)

        # Check that we attempted to display the results page in a browser.
        self.assertTrue(user.opened_urls)

    def test_reftest_with_two_notrefs(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        res, out, err, _ = logging_run(['--no-show-results', 'reftests/foo/'], tests_included=True, host=host, record_results=True)
        file_list = host.filesystem.written_files.keys()
        file_list.remove('/tmp/layout-test-results/tests_run0.txt')
        json_string = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        json = self.parse_full_results(json_string)
        self.assertTrue("multiple-match-success.html" not in json["tests"]["reftests"]["foo"])
        self.assertTrue("multiple-mismatch-success.html" not in json["tests"]["reftests"]["foo"])
        self.assertTrue("multiple-both-success.html" not in json["tests"]["reftests"]["foo"])
        self.assertEqual(json["tests"]["reftests"]["foo"]["multiple-match-failure.html"],
            {"expected": "PASS", "ref_file": "reftests/foo/second-mismatching-ref.html", "actual": "IMAGE", "image_diff_percent": 1, 'is_reftest': True})
        self.assertEqual(json["tests"]["reftests"]["foo"]["multiple-mismatch-failure.html"],
            {"expected": "PASS", "ref_file": "reftests/foo/matching-ref.html", "actual": "IMAGE", "is_mismatch_reftest": True})
        self.assertEqual(json["tests"]["reftests"]["foo"]["multiple-both-failure.html"],
            {"expected": "PASS", "ref_file": "reftests/foo/matching-ref.html", "actual": "IMAGE", "is_mismatch_reftest": True})


class RebaselineTest(unittest.TestCase, StreamTestingMixin):
    def assertBaselines(self, file_list, file, extensions, err):
        "assert that the file_list contains the baselines."""
        for ext in extensions:
            baseline = file + "-expected" + ext
            baseline_msg = 'Writing new expected result "%s"\n' % baseline[1:]
            self.assertTrue(any(f.find(baseline) != -1 for f in file_list))
            self.assertContainsLine(err, baseline_msg)

    # FIXME: Add tests to ensure that we're *not* writing baselines when we're not
    # supposed to be.

    def test_reset_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        res, out, err, _ = logging_run(['--pixel-tests',
                        '--reset-results',
                        'passes/image.html',
                        'failures/expected/missing_image.html'],
                        tests_included=True, host=host, new_results=True)
        file_list = host.filesystem.written_files.keys()
        file_list.remove('/tmp/layout-test-results/tests_run0.txt')
        self.assertEquals(res, 0)
        self.assertEmpty(out)
        self.assertEqual(len(file_list), 4)
        self.assertBaselines(file_list, "/passes/image", [".txt", ".png"], err)
        self.assertBaselines(file_list, "/failures/expected/missing_image", [".txt", ".png"], err)

    def test_missing_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        res, out, err, _ = logging_run(['--no-show-results',
                     'failures/unexpected/missing_text.html',
                     'failures/unexpected/missing_image.html',
                     'failures/unexpected/missing_audio.html',
                     'failures/unexpected/missing_render_tree_dump.html'],
                     tests_included=True, host=host, new_results=True)
        file_list = host.filesystem.written_files.keys()
        file_list.remove('/tmp/layout-test-results/tests_run0.txt')
        self.assertEquals(res, 0)
        self.assertNotEmpty(out)
        self.assertEqual(len(file_list), 6)
        self.assertBaselines(file_list, "/failures/unexpected/missing_text", [".txt"], err)
        self.assertBaselines(file_list, "/platform/test-mac-leopard/failures/unexpected/missing_image", [".png"], err)
        self.assertBaselines(file_list, "/platform/test-mac-leopard/failures/unexpected/missing_render_tree_dump", [".txt"], err)

    def test_new_baseline(self):
        # Test that we update the platform expectations. If the expectation
        # is mssing, then create a new expectation in the platform dir.
        host = MockHost()
        res, out, err, _ = logging_run(['--pixel-tests',
                        '--new-baseline',
                        'passes/image.html',
                        'failures/expected/missing_image.html'],
                    tests_included=True, host=host, new_results=True)
        file_list = host.filesystem.written_files.keys()
        file_list.remove('/tmp/layout-test-results/tests_run0.txt')
        self.assertEquals(res, 0)
        self.assertEmpty(out)
        self.assertEqual(len(file_list), 4)
        self.assertBaselines(file_list,
            "/platform/test-mac-leopard/passes/image", [".txt", ".png"], err)
        self.assertBaselines(file_list,
            "/platform/test-mac-leopard/failures/expected/missing_image", [".txt", ".png"], err)


if __name__ == '__main__':
    unittest.main()
