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

import logging
import Queue
import sys
import thread
import threading
import time
import unittest

from webkitpy.common import array_stream
from webkitpy.common.system import outputcapture
from webkitpy.tool import mocktool

from webkitpy.layout_tests import run_webkit_tests

import message_broker


class TestThread(threading.Thread):
    def __init__(self, started_queue, stopping_queue):
        threading.Thread.__init__(self)
        self._thread_id = None
        self._started_queue = started_queue
        self._stopping_queue = stopping_queue
        self._timeout = False
        self._timeout_queue = Queue.Queue()
        self._exception_info = None

    def id(self):
        return self._thread_id

    def getName(self):
        return "worker-0"

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

    def exception_info(self):
        return self._exception_info

    def next_timeout(self):
        if self._timeout:
            self._timeout_queue.put('done')
            return time.time() - 10
        return time.time()

    def clear_next_timeout(self):
        self._next_timeout = None

class TestHandler(logging.Handler):
    def __init__(self, astream):
        logging.Handler.__init__(self)
        self._stream = astream

    def emit(self, record):
        self._stream.write(self.format(record))


class MultiThreadedBrokerTest(unittest.TestCase):
    class MockTestRunner(object):
        def __init__(self):
            pass

        def __del__(self):
            pass

        def update(self):
            pass

    def run_one_thread(self, msg):
        runner = self.MockTestRunner()
        port = None
        options = mocktool.MockOptions(child_processes='1')
        starting_queue = Queue.Queue()
        stopping_queue = Queue.Queue()
        broker = message_broker.MultiThreadedBroker(port, options)
        broker._test_runner = runner
        child_thread = TestThread(starting_queue, stopping_queue)
        broker._workers['worker-0'] = message_broker._WorkerState('worker-0')
        broker._workers['worker-0'].thread = child_thread
        child_thread.start()
        started_msg = starting_queue.get()
        stopping_queue.put(msg)
        return broker.run_message_loop()

    def test_basic(self):
        interrupted = self.run_one_thread('')
        self.assertFalse(interrupted)

    def test_interrupt(self):
        self.assertRaises(KeyboardInterrupt, self.run_one_thread, 'KeyboardInterrupt')

    def test_timeout(self):
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        interrupted = self.run_one_thread('Timeout')
        self.assertFalse(interrupted)
        oc.restore_output()

    def test_exception(self):
        self.assertRaises(ValueError, self.run_one_thread, 'Exception')


class Test(unittest.TestCase):
    def test_find_thread_stack_found(self):
        id, stack = sys._current_frames().items()[0]
        found_stack = message_broker._find_thread_stack(id)
        self.assertNotEqual(found_stack, None)

    def test_find_thread_stack_not_found(self):
        found_stack = message_broker._find_thread_stack(0)
        self.assertEqual(found_stack, None)

    def test_log_wedged_worker(self):
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        logger = message_broker._log
        astream = array_stream.ArrayStream()
        handler = TestHandler(astream)
        logger.addHandler(handler)

        starting_queue = Queue.Queue()
        stopping_queue = Queue.Queue()
        child_thread = TestThread(starting_queue, stopping_queue)
        child_thread.start()
        msg = starting_queue.get()

        message_broker.log_wedged_worker(child_thread.getName(),
                                         child_thread.id())
        stopping_queue.put('')
        child_thread.join(timeout=1.0)

        self.assertFalse(astream.empty())
        self.assertFalse(child_thread.isAlive())
        oc.restore_output()


if __name__ == '__main__':
    unittest.main()
