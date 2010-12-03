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

# FIXME: Boy do we need a lot more tests here ...


class TestThreadStacks(unittest.TestCase):
    class Thread(threading.Thread):
        def __init__(self, started_queue, stopping_queue):
            threading.Thread.__init__(self)
            self._id = None
            self._started_queue = started_queue
            self._stopping_queue = stopping_queue

        def id(self):
            return self._id

        def name(self):
            return 'worker/0'

        def run(self):
            self._id = thread.get_ident()
            self._started_queue.put('')
            msg = self._stopping_queue.get()

    def make_broker(self):
        options = mocktool.MockOptions()
        return message_broker._MultiThreadedBroker(port=None,
                                                     options=options)

    def test_find_thread_stack_found(self):
        broker = self.make_broker()
        id, stack = sys._current_frames().items()[0]
        found_stack = broker._find_thread_stack(id)
        self.assertNotEqual(found_stack, None)

    def test_find_thread_stack_not_found(self):
        broker = self.make_broker()
        found_stack = broker._find_thread_stack(0)
        self.assertEqual(found_stack, None)

    def test_log_wedged_worker(self):
        broker = self.make_broker()
        oc = outputcapture.OutputCapture()
        oc.capture_output()

        starting_queue = Queue.Queue()
        stopping_queue = Queue.Queue()
        child_thread = TestThreadStacks.Thread(starting_queue, stopping_queue)
        child_thread.start()
        broker._threads[child_thread.name()] = child_thread
        msg = starting_queue.get()

        broker.log_wedged_worker(child_thread.name())
        stopping_queue.put('')
        child_thread.join(timeout=1.0)

        self.assertFalse(child_thread.isAlive())
        oc.restore_output()


if __name__ == '__main__':
    unittest.main()
