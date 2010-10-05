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

from webkitpy.thirdparty.mock import Mock


class MockUser():
    def __init__(self):
        self.url = None

    def open_url(self, url):
        self.url = url


def passing_run(args=[], port_obj=None, record_results=False,
                tests_included=False):
    new_args = ['--print', 'nothing']
    if not '--platform' in args:
        new_args.extend(['--platform', 'test'])
    if not record_results:
        new_args.append('--no-record-results')
    new_args.extend(args)
    if not tests_included:
        # We use the glob to test that globbing works.
        new_args.extend(['passes',
                         'http/tests',
                         'http/tests/websocket/tests',
                         'failures/expected/*'])
    options, parsed_args = run_webkit_tests.parse_args(new_args)
    if port_obj is None:
        port_obj = port.get(port_name=options.platform, options=options,
                            user=MockUser())
    res = run_webkit_tests.run(port_obj, options, parsed_args)
    return res == 0


def logging_run(args=[], tests_included=False):
    new_args = ['--no-record-results']
    if not '--platform' in args:
        new_args.extend(['--platform', 'test'])
    new_args.extend(args)
    if not tests_included:
        new_args.extend(['passes',
                         'http/tests',
                         'http/tests/websocket/tests',
                         'failures/expected/*'])
    options, parsed_args = run_webkit_tests.parse_args(new_args)
    user = MockUser()
    port_obj = port.get(port_name=options.platform, options=options, user=user)
    buildbot_output = array_stream.ArrayStream()
    regular_output = array_stream.ArrayStream()
    res = run_webkit_tests.run(port_obj, options, parsed_args,
                               buildbot_output=buildbot_output,
                               regular_output=regular_output)
    return (res, buildbot_output, regular_output, user)


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
        # FIXME: verify # of tests run
        self.assertTrue(passing_run(['--batch-size', '2']))

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
        self.assertRaises(KeyboardInterrupt, passing_run,
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
        # FIXME: verify # of tests run
        self.assertTrue(passing_run(['--run-chunk', '1:4']))

    def test_run_force(self):
        # This raises an exception because we run
        # failures/expected/exception.html, which is normally SKIPped.
        self.assertRaises(ValueError, logging_run, ['--force'])

    def test_run_part(self):
        # FIXME: verify # of tests run
        self.assertTrue(passing_run(['--run-part', '1:2']))

    def test_run_singly(self):
        self.assertTrue(passing_run(['--run-singly']))

    def test_single_file(self):
        # FIXME: verify # of tests run
        self.assertTrue(passing_run(['passes/text.html'], tests_included=True))

    def test_test_list(self):
        filename = tempfile.mktemp()
        tmpfile = file(filename, mode='w+')
        tmpfile.write('passes/text.html')
        tmpfile.close()
        self.assertTrue(passing_run(['--test-list=%s' % filename],
                                    tests_included=True))
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



def _mocked_open(original_open, file_list):
    def _wrapper(name, mode, encoding):
        if name.find("-expected.") != -1 and mode == "w":
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

    def test_reset_results(self):
        file_list = []
        original_open = codecs.open
        try:
            # Test that we update expectations in place. If the expectation
            # is missing, update the expected generic location.
            file_list = []
            codecs.open = _mocked_open(original_open, file_list)
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
        finally:
            codecs.open = original_open

    def test_new_baseline(self):
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
    def _get_test_info_for_file(self, test_file):
        return test_file


class TestRunnerTest(unittest.TestCase):
    def test_results_html(self):
        mock_port = Mock()
        mock_port.relative_test_filename = lambda name: name
        mock_port.filename_to_uri = lambda name: name

        runner = run_webkit_tests.TestRunner(port=mock_port, options=Mock(), printer=Mock())
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

    def queue_to_list(self, queue):
        queue_list = []
        while(True):
            try:
                queue_list.append(queue.get_nowait())
            except Queue.Empty:
                break
        return queue_list

    def test_get_test_file_queue(self):
        # Test that _get_test_file_queue in run_webkit_tests.TestRunner really
        # put the http tests first in the queue.
        runner = TestRunnerWrapper(port=Mock(), options=Mock(), printer=Mock())
        runner._options.experimental_fully_parallel = False

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

        runner._options.child_processes = 1
        test_queue_for_single_thread = runner._get_test_file_queue(test_list)
        runner._options.child_processes = 2
        test_queue_for_multi_thread = runner._get_test_file_queue(test_list)

        single_thread_results = self.queue_to_list(test_queue_for_single_thread)
        multi_thread_results = self.queue_to_list(test_queue_for_multi_thread)

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


class TestThread(dump_render_tree_thread.WatchableThread):
    def __init__(self, started_queue, stopping_queue):
        dump_render_tree_thread.WatchableThread.__init__(self)
        self._started_queue = started_queue
        self._stopping_queue = stopping_queue
        self._timeout = False
        self._timeout_queue = Queue.Queue()

    def run(self):
        self._covered_run()

    def _covered_run(self):
        # FIXME: this is a separate routine to work around a bug
        # in coverage: see http://bitbucket.org/ned/coveragepy/issue/85.
        self._thread_id = thread.get_ident()
        try:
            self._started_queue.put('')
            msg = self._stopping_queue.get()
            if msg == 'KeyboardInterrupt':
                raise KeyboardInterrupt
            elif msg == 'Exception':
                raise ValueError()
            elif msg == 'Timeout':
                self._timeout = True
                self._timeout_queue.get()
        except:
            self._exception_info = sys.exc_info()

    def next_timeout(self):
        if self._timeout:
            self._timeout_queue.put('done')
            return time.time() - 10
        return time.time()


class TestHandler(logging.Handler):
    def __init__(self, astream):
        logging.Handler.__init__(self)
        self._stream = astream

    def emit(self, record):
        self._stream.write(self.format(record))


class WaitForThreadsToFinishTest(unittest.TestCase):
    class MockTestRunner(run_webkit_tests.TestRunner):
        def __init__(self):
            pass

        def __del__(self):
            pass

        def update_summary(self, result_summary):
            pass

    def run_one_thread(self, msg):
        runner = self.MockTestRunner()
        starting_queue = Queue.Queue()
        stopping_queue = Queue.Queue()
        child_thread = TestThread(starting_queue, stopping_queue)
        child_thread.start()
        started_msg = starting_queue.get()
        stopping_queue.put(msg)
        threads = [child_thread]
        return runner._wait_for_threads_to_finish(threads, None)

    def test_basic(self):
        interrupted = self.run_one_thread('')
        self.assertFalse(interrupted)

    def test_interrupt(self):
        interrupted = self.run_one_thread('KeyboardInterrupt')
        self.assertTrue(interrupted)

    def test_timeout(self):
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        interrupted = self.run_one_thread('Timeout')
        self.assertFalse(interrupted)
        oc.restore_output()

    def test_exception(self):
        self.assertRaises(ValueError, self.run_one_thread, 'Exception')


class StandaloneFunctionsTest(unittest.TestCase):
    def test_log_wedged_thread(self):
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        logger = run_webkit_tests._log
        astream = array_stream.ArrayStream()
        handler = TestHandler(astream)
        logger.addHandler(handler)

        starting_queue = Queue.Queue()
        stopping_queue = Queue.Queue()
        child_thread = TestThread(starting_queue, stopping_queue)
        child_thread.start()
        msg = starting_queue.get()

        run_webkit_tests._log_wedged_thread(child_thread)
        stopping_queue.put('')
        child_thread.join(timeout=1.0)

        self.assertFalse(astream.empty())
        self.assertFalse(child_thread.isAlive())
        oc.restore_output()


if __name__ == '__main__':
    unittest.main()
