#!/usr/bin/python
# Copyright (C) 2010 Google Inc. All rights reserved.
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
import pdb
import Queue
import sys
import thread
import time
import threading
import unittest

from webkitpy.common import array_stream
from webkitpy.layout_tests import port
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.layout_package import dump_render_tree_thread

from webkitpy.thirdparty.mock import Mock


def passing_run(args, port_obj=None, record_results=False,
                tests_included=False):
    args.extend(['--print', 'nothing'])
    if not tests_included:
        # We use the glob to test that globbing works.
        args.extend(['passes', 'failures/expected/*'])
    if not record_results:
        args.append('--no-record-results')
    options, args = run_webkit_tests.parse_args(args)
    if port_obj is None:
        port_obj = port.get(options.platform, options)
    res = run_webkit_tests.run(port_obj, options, args)
    return res == 0


def logging_run(args, tests_included=False):
    args.extend(['--no-record-results'])
    if not tests_included:
        args.extend(['passes', 'failures/expected/*'])
    options, args = run_webkit_tests.parse_args(args)
    port_obj = port.get(options.platform, options)
    buildbot_output = array_stream.ArrayStream()
    regular_output = array_stream.ArrayStream()
    res = run_webkit_tests.run(port_obj, options, args,
                               buildbot_output=buildbot_output,
                               regular_output=regular_output)
    return (res, buildbot_output, regular_output)


class MainTest(unittest.TestCase):
    def test_fast(self):
        self.assertTrue(passing_run(['--platform', 'test']))
        self.assertTrue(passing_run(['--platform', 'test', '--run-singly']))
        self.assertTrue(passing_run(['--platform', 'test',
                                     'passes/text.html'], tests_included=True))

    def test_unexpected_failures(self):
        # Run tests including the unexpected failures.
        self.assertFalse(passing_run(['--platform', 'test'],
                         tests_included=True))

    def test_one_child_process(self):
        (res, buildbot_output, regular_output) = logging_run(
             ['--platform', 'test', '--print', 'config', '--child-processes',
              '1'])
        self.assertTrue('Running one DumpRenderTree\n'
                        in regular_output.get())

    def test_two_child_processes(self):
        (res, buildbot_output, regular_output) = logging_run(
             ['--platform', 'test', '--print', 'config', '--child-processes',
              '2'])
        self.assertTrue('Running 2 DumpRenderTrees in parallel\n'
                        in regular_output.get())

    def test_last_results(self):
        passing_run(['--platform', 'test'], record_results=True)
        (res, buildbot_output, regular_output) = logging_run(
            ['--platform', 'test', '--print-last-failures'])
        self.assertEqual(regular_output.get(), ['\n\n'])
        self.assertEqual(buildbot_output.get(), [])

    def test_no_tests_found(self):
        self.assertRaises(SystemExit, logging_run,
                          ['--platform', 'test', 'resources'],
                          tests_included=True)
        self.assertRaises(SystemExit, logging_run,
                          ['--platform', 'test', 'foo'],
                          tests_included=True)

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
            passing_run(['--platform', 'test', '--pixel-tests',
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
            passing_run(['--platform', 'test', '--pixel-tests',
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


class DryrunTest(unittest.TestCase):
    # FIXME: it's hard to know which platforms are safe to test; the
    # chromium platforms require a chromium checkout, and the mac platform
    # requires fcntl, so it can't be tested on win32, etc. There is
    # probably a better way of handling this.
    def test_darwin(self):
        if sys.platform != "darwin":
            return

        self.assertTrue(passing_run(['--platform', 'dryrun',
                                     'fast/html']))
        self.assertTrue(passing_run(['--platform', 'dryrun-mac',
                                     'fast/html']))


class TestThread(dump_render_tree_thread.WatchableThread):
    def __init__(self, started_queue, stopping_queue):
        dump_render_tree_thread.WatchableThread.__init__(self)
        self._started_queue = started_queue
        self._stopping_queue = stopping_queue
        self._timeout = False
        self._timeout_queue = Queue.Queue()

    def run(self):
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
        interrupted = self.run_one_thread('Timeout')
        self.assertFalse(interrupted)

    def test_exception(self):
        self.assertRaises(ValueError, self.run_one_thread, 'Exception')


class StandaloneFunctionsTest(unittest.TestCase):
    def test_log_wedged_thread(self):
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

    def test_find_thread_stack(self):
        id, stack = sys._current_frames().items()[0]
        found_stack = run_webkit_tests._find_thread_stack(id)
        self.assertNotEqual(found_stack, None)

        found_stack = run_webkit_tests._find_thread_stack(0)
        self.assertEqual(found_stack, None)

if __name__ == '__main__':
    unittest.main()
