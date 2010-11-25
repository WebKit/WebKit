#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

"""Unit tests for run_webkit_tests."""

import codecs
import itertools
import logging
import os
import Queue
import shutil
import sys
import tempfile
import thread
import time
import threading
import unittest

from webkitpy.common import array_stream
from webkitpy.common.system import outputcapture
from webkitpy.common.system import user
from webkitpy.layout_tests import port
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.layout_package import dump_render_tree_thread
from webkitpy.layout_tests.port.test import TestPort, TestDriver
from webkitpy.python24.versioning import compare_version
from webkitpy.test.skip import skip_if

from webkitpy.thirdparty.mock import Mock


class MockUser():
    def __init__(self):
        self.url = None

    def open_url(self, url):
        self.url = url


def passing_run(extra_args=None, port_obj=None, record_results=False,
                tests_included=False):
    extra_args = extra_args or []
    args = ['--print', 'nothing']
    if not '--platform' in extra_args:
        args.extend(['--platform', 'test'])
    if not record_results:
        args.append('--no-record-results')
    if not '--child-processes' in extra_args:
        args.extend(['--worker-model', 'inline'])
    args.extend(extra_args)
    if not tests_included:
        # We use the glob to test that globbing works.
        args.extend(['passes',
                     'http/tests',
                     'websocket/tests',
                     'failures/expected/*'])
    options, parsed_args = run_webkit_tests.parse_args(args)
    if not port_obj:
        port_obj = port.get(port_name=options.platform, options=options,
                            user=MockUser())
    res = run_webkit_tests.run(port_obj, options, parsed_args)
    return res == 0


def logging_run(extra_args=None, port_obj=None, tests_included=False):
    extra_args = extra_args or []
    args = ['--no-record-results']
    if not '--platform' in extra_args:
        args.extend(['--platform', 'test'])
    if not '--child-processes' in extra_args:
        args.extend(['--worker-model', 'inline'])
    args.extend(extra_args)
    if not tests_included:
        args.extend(['passes',
                     'http/tests',
                     'websocket/tests',
                     'failures/expected/*'])

    oc = outputcapture.OutputCapture()
    try:
        oc.capture_output()
        options, parsed_args = run_webkit_tests.parse_args(args)
        user = MockUser()
        if not port_obj:
            port_obj = port.get(port_name=options.platform, options=options,
                                user=user)
        buildbot_output = array_stream.ArrayStream()
        regular_output = array_stream.ArrayStream()
        res = run_webkit_tests.run(port_obj, options, parsed_args,
                                   buildbot_output=buildbot_output,
                                   regular_output=regular_output)
    finally:
        oc.restore_output()
    return (res, buildbot_output, regular_output, user)


def get_tests_run(extra_args=None, tests_included=False, flatten_batches=False):
    extra_args = extra_args or []
    args = [
        '--print', 'nothing',
        '--platform', 'test',
        '--no-record-results',
        '--worker-model', 'inline']
    args.extend(extra_args)
    if not tests_included:
        # Not including http tests since they get run out of order (that
        # behavior has its own test, see test_get_test_file_queue)
        args.extend(['passes', 'failures'])
    options, parsed_args = run_webkit_tests.parse_args(args)
    user = MockUser()

    test_batches = []

    class RecordingTestDriver(TestDriver):
        def __init__(self, port, worker_number):
            TestDriver.__init__(self, port, worker_number)
            self._current_test_batch = None

        def poll(self):
            # So that we don't create a new driver for every test
            return None

        def stop(self):
            self._current_test_batch = None

        def run_test(self, test_input):
            if self._current_test_batch is None:
                self._current_test_batch = []
                test_batches.append(self._current_test_batch)
            test_name = self._port.relative_test_filename(test_input.filename)
            self._current_test_batch.append(test_name)
            return TestDriver.run_test(self, test_input)

    class RecordingTestPort(TestPort):
        def create_driver(self, worker_number):
            return RecordingTestDriver(self, worker_number)

    recording_port = RecordingTestPort(options=options, user=user)
    logging_run(extra_args=args, port_obj=recording_port, tests_included=True)

    if flatten_batches:
        return list(itertools.chain(*test_batches))

    return test_batches

class MainTest(unittest.TestCase):
    def test_accelerated_compositing(self):
        # This just tests that we recognize the command line args
        self.assertTrue(passing_run(['--accelerated-compositing']))
        self.assertTrue(passing_run(['--no-accelerated-compositing']))

    def test_accelerated_2d_canvas(self):
        # This just tests that we recognize the command line args
        self.assertTrue(passing_run(['--accelerated-2d-canvas']))
        self.assertTrue(passing_run(['--no-accelerated-2d-canvas']))

    def test_basic(self):
        self.assertTrue(passing_run())

    def test_batch_size(self):
        batch_tests_run = get_tests_run(['--batch-size', '2'])
        for batch in batch_tests_run:
            self.assertTrue(len(batch) <= 2, '%s had too many tests' % ', '.join(batch))

    def test_child_process_1(self):
        (res, buildbot_output, regular_output, user) = logging_run(
             ['--print', 'config', '--child-processes', '1'])
        self.assertTrue('Running one DumpRenderTree\n'
                        in regular_output.get())

    def test_child_processes_2(self):
        (res, buildbot_output, regular_output, user) = logging_run(
             ['--print', 'config', '--child-processes', '2'])
        self.assertTrue('Running 2 DumpRenderTrees in parallel\n'
                        in regular_output.get())

    def test_exception_raised(self):
        self.assertRaises(ValueError, logging_run,
            ['failures/expected/exception.html'], tests_included=True)

    def test_full_results_html(self):
        # FIXME: verify html?
        self.assertTrue(passing_run(['--full-results-html']))

    def test_help_printing(self):
        res, out, err, user = logging_run(['--help-printing'])
        self.assertEqual(res, 0)
        self.assertTrue(out.empty())
        self.assertFalse(err.empty())

    def test_hung_thread(self):
        res, out, err, user = logging_run(['--run-singly', '--time-out-ms=50',
                                          'failures/expected/hang.html'],
                                          tests_included=True)
        self.assertEqual(res, 0)
        self.assertFalse(out.empty())
        self.assertFalse(err.empty())

    def test_keyboard_interrupt(self):
        # Note that this also tests running a test marked as SKIP if
        # you specify it explicitly.
        self.assertRaises(KeyboardInterrupt, logging_run,
            ['failures/expected/keyboard.html'], tests_included=True)

    def test_last_results(self):
        passing_run(['--clobber-old-results'], record_results=True)
        (res, buildbot_output, regular_output, user) = logging_run(
            ['--print-last-failures'])
        self.assertEqual(regular_output.get(), ['\n\n'])
        self.assertEqual(buildbot_output.get(), [])

    def test_lint_test_files(self):
        # FIXME:  add errors?
        res, out, err, user = logging_run(['--lint-test-files'],
                                          tests_included=True)
        self.assertEqual(res, 0)
        self.assertTrue(out.empty())
        self.assertTrue(any(['lint succeeded' in msg for msg in err.get()]))

    def test_no_tests_found(self):
        res, out, err, user = logging_run(['resources'], tests_included=True)
        self.assertEqual(res, -1)
        self.assertTrue(out.empty())
        self.assertTrue('No tests to run.\n' in err.get())

    def test_no_tests_found_2(self):
        res, out, err, user = logging_run(['foo'], tests_included=True)
        self.assertEqual(res, -1)
        self.assertTrue(out.empty())
        self.assertTrue('No tests to run.\n' in err.get())

    def test_randomize_order(self):
        # FIXME: verify order was shuffled
        self.assertTrue(passing_run(['--randomize-order']))

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

    def test_single_file(self):
        tests_run = get_tests_run(['passes/text.html'], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/text.html'], tests_run)

    def test_test_list(self):
        filename = tempfile.mktemp()
        tmpfile = file(filename, mode='w+')
        tmpfile.write('passes/text.html')
        tmpfile.close()
        tests_run = get_tests_run(['--test-list=%s' % filename], tests_included=True, flatten_batches=True)
        self.assertEquals(['passes/text.html'], tests_run)
        os.remove(filename)
        res, out, err, user = logging_run(['--test-list=%s' % filename],
                                          tests_included=True)
        self.assertEqual(res, -1)
        self.assertFalse(err.empty())

    def test_unexpected_failures(self):
        # Run tests including the unexpected failures.
        self._url_opened = None
        res, out, err, user = logging_run(tests_included=True)
        self.assertEqual(res, 1)
        self.assertFalse(out.empty())
        self.assertFalse(err.empty())
        self.assertEqual(user.url, '/tmp/layout-test-results/results.html')

    def test_results_directory_absolute(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        tmpdir = tempfile.mkdtemp()
        res, out, err, user = logging_run(['--results-directory=' + tmpdir],
                                          tests_included=True)
        self.assertEqual(user.url, os.path.join(tmpdir, 'results.html'))
        shutil.rmtree(tmpdir, ignore_errors=True)

    def test_results_directory_default(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        # This is the default location.
        res, out, err, user = logging_run(tests_included=True)
        self.assertEqual(user.url, '/tmp/layout-test-results/results.html')

    def test_results_directory_relative(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        res, out, err, user = logging_run(['--results-directory=foo'],
                                          tests_included=True)
        self.assertEqual(user.url, '/tmp/foo/results.html')

    def test_tolerance(self):
        class ImageDiffTestPort(TestPort):
            def diff_image(self, expected_contents, actual_contents,
                   diff_filename=None):
                self.tolerance_used_for_diff_image = self._options.tolerance
                return True

        def get_port_for_run(args):
            options, parsed_args = run_webkit_tests.parse_args(args)
            test_port = ImageDiffTestPort(options=options, user=MockUser())
            passing_run(args, port_obj=test_port, tests_included=True)
            return test_port

        base_args = ['--pixel-tests', 'failures/expected/*']

        # If we pass in an explicit tolerance argument, then that will be used.
        test_port = get_port_for_run(base_args + ['--tolerance', '.1'])
        self.assertEqual(0.1, test_port.tolerance_used_for_diff_image)
        test_port = get_port_for_run(base_args + ['--tolerance', '0'])
        self.assertEqual(0, test_port.tolerance_used_for_diff_image)

        # Otherwise the port's default tolerance behavior (including ignoring it)
        # should be used.
        test_port = get_port_for_run(base_args)
        self.assertEqual(None, test_port.tolerance_used_for_diff_image)

    def test_worker_model__inline(self):
        self.assertTrue(passing_run(['--worker-model', 'inline']))

    def test_worker_model__threads(self):
        self.assertTrue(passing_run(['--worker-model', 'threads']))

    def test_worker_model__processes(self):
        self.assertRaises(ValueError, logging_run,
                          ['--worker-model', 'processes'])

    def test_worker_model__unknown(self):
        self.assertRaises(ValueError, logging_run,
                          ['--worker-model', 'unknown'])

MainTest = skip_if(MainTest, sys.platform == 'cygwin' and compare_version(sys, '2.6')[0] < 0, 'new-run-webkit-tests tests hang on Cygwin Python 2.5.2')



def _mocked_open(original_open, file_list):
    def _wrapper(name, mode, encoding):
        if name.find("-expected.") != -1 and mode.find("w") != -1:
            # we don't want to actually write new baselines, so stub these out
            name.replace('\\', '/')
            file_list.append(name)
            return original_open(os.devnull, mode, encoding)
        return original_open(name, mode, encoding)
    return _wrapper


class RebaselineTest(unittest.TestCase):
    def assertBaselines(self, file_list, file):
        "assert that the file_list contains the baselines."""
        for ext in [".txt", ".png", ".checksum"]:
            baseline = file + "-expected" + ext
            self.assertTrue(any(f.find(baseline) != -1 for f in file_list))

    # FIXME: Add tests to ensure that we're *not* writing baselines when we're not
    # supposed to be.

    def disabled_test_reset_results(self):
        # FIXME: This test is disabled until we can rewrite it to use a
        # mock filesystem.
        #
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        file_list = []
        passing_run(['--pixel-tests',
                        '--reset-results',
                        'passes/image.html',
                        'failures/expected/missing_image.html'],
                        tests_included=True)
        self.assertEqual(len(file_list), 6)
        self.assertBaselines(file_list,
            "data/passes/image")
        self.assertBaselines(file_list,
            "data/failures/expected/missing_image")

    def disabled_test_new_baseline(self):
        # FIXME: This test is disabled until we can rewrite it to use a
        # mock filesystem.
        #
        # Test that we update the platform expectations. If the expectation
        # is mssing, then create a new expectation in the platform dir.
        file_list = []
        original_open = codecs.open
        try:
            # Test that we update the platform expectations. If the expectation
            # is mssing, then create a new expectation in the platform dir.
            file_list = []
            codecs.open = _mocked_open(original_open, file_list)
            passing_run(['--pixel-tests',
                         '--new-baseline',
                         'passes/image.html',
                         'failures/expected/missing_image.html'],
                        tests_included=True)
            self.assertEqual(len(file_list), 6)
            self.assertBaselines(file_list,
                "data/platform/test/passes/image")
            self.assertBaselines(file_list,
                "data/platform/test/failures/expected/missing_image")
        finally:
            codecs.open = original_open


class TestRunnerWrapper(run_webkit_tests.TestRunner):
    def _get_test_input_for_file(self, test_file):
        return test_file


class TestRunnerTest(unittest.TestCase):
    def test_results_html(self):
        mock_port = Mock()
        mock_port.relative_test_filename = lambda name: name
        mock_port.filename_to_uri = lambda name: name

        runner = run_webkit_tests.TestRunner(port=mock_port, options=Mock(),
            printer=Mock(), message_broker=Mock())
        expected_html = u"""<html>
  <head>
    <title>Layout Test Results (time)</title>
  </head>
  <body>
    <h2>Title (time)</h2>
        <p><a href='test_path'>test_path</a><br />
</p>
</body></html>
"""
        html = runner._results_html(["test_path"], {}, "Title", override_time="time")
        self.assertEqual(html, expected_html)

    def test_shard_tests(self):
        # Test that _shard_tests in run_webkit_tests.TestRunner really
        # put the http tests first in the queue.
        runner = TestRunnerWrapper(port=Mock(), options=Mock(),
            printer=Mock(), message_broker=Mock())

        test_list = [
          "LayoutTests/websocket/tests/unicode.htm",
          "LayoutTests/animations/keyframes.html",
          "LayoutTests/http/tests/security/view-source-no-refresh.html",
          "LayoutTests/websocket/tests/websocket-protocol-ignored.html",
          "LayoutTests/fast/css/display-none-inline-style-change-crash.html",
          "LayoutTests/http/tests/xmlhttprequest/supported-xml-content-types.html",
          "LayoutTests/dom/html/level2/html/HTMLAnchorElement03.html",
          "LayoutTests/ietestcenter/Javascript/11.1.5_4-4-c-1.html",
          "LayoutTests/dom/html/level2/html/HTMLAnchorElement06.html",
        ]

        expected_tests_to_http_lock = set([
          'LayoutTests/websocket/tests/unicode.htm',
          'LayoutTests/http/tests/security/view-source-no-refresh.html',
          'LayoutTests/websocket/tests/websocket-protocol-ignored.html',
          'LayoutTests/http/tests/xmlhttprequest/supported-xml-content-types.html',
        ])

        # FIXME: Ideally the HTTP tests don't have to all be in one shard.
        single_thread_results = runner._shard_tests(test_list, False)
        multi_thread_results = runner._shard_tests(test_list, True)

        self.assertEqual("tests_to_http_lock", single_thread_results[0][0])
        self.assertEqual(expected_tests_to_http_lock, set(single_thread_results[0][1]))
        self.assertEqual("tests_to_http_lock", multi_thread_results[0][0])
        self.assertEqual(expected_tests_to_http_lock, set(multi_thread_results[0][1]))


class DryrunTest(unittest.TestCase):
    # FIXME: it's hard to know which platforms are safe to test; the
    # chromium platforms require a chromium checkout, and the mac platform
    # requires fcntl, so it can't be tested on win32, etc. There is
    # probably a better way of handling this.
    def test_darwin(self):
        if sys.platform != "darwin":
            return

        self.assertTrue(passing_run(['--platform', 'test']))
        self.assertTrue(passing_run(['--platform', 'dryrun',
                                     'fast/html']))
        self.assertTrue(passing_run(['--platform', 'dryrun-mac',
                                     'fast/html']))

    def test_test(self):
        self.assertTrue(passing_run(['--platform', 'dryrun-test',
                                           '--pixel-tests']))


if __name__ == '__main__':
    unittest.main()
