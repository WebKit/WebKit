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

import optparse
import Queue
import sys
import unittest

from webkitpy.common.system import outputcapture
from webkitpy.layout_tests.controllers import manager_worker_broker


# In order to reliably control when child workers are starting and stopping,
# we use a pair of global variables to hold queues used for messaging. Ideally
# we wouldn't need globals, but we can't pass these through a lexical closure
# because those can't be Pickled and sent to a subprocess, and we'd prefer not
# to have to pass extra arguments to the worker in the start_worker() call.
starting_queue = None
stopping_queue = None


WORKER_NAME = 'worker/1'

def make_broker(manager, max_workers, start_queue=None, stop_queue=None):
    global starting_queue
    global stopping_queue
    starting_queue = start_queue
    stopping_queue = stop_queue
    return manager_worker_broker.get(max_workers, manager, _TestWorker)


class _TestWorker(object):
    def __init__(self, caller, worker_number):
        self._caller = caller
        self._thing_to_greet = 'everybody'
        self._starting_queue = starting_queue
        self._stopping_queue = stopping_queue
        self._options = optparse.Values({'verbose': False})

    def name(self):
        return WORKER_NAME

    def cleanup(self):
        pass

    def handle(self, message, src, an_int, a_str):
        assert an_int == 1
        assert a_str == "hello, world"
        self._caller.post_message('finished_test', 2)

    def safe_init(self):
        if self._starting_queue:
            self._starting_queue.put('')

        if self._stopping_queue:
            self._stopping_queue.get()

    def stop(self):
        self._caller.post_message('done')


class FunctionTests(unittest.TestCase):
    def test_get__inline(self):
        self.assertTrue(make_broker(self, 1) is not None)

    def test_get__processes(self):
        # This test sometimes fails on Windows. See <http://webkit.org/b/55087>.
        if sys.platform in ('cygwin', 'win32'):
            return
        self.assertTrue(make_broker(self, 2) is not None)


class _TestsMixin(object):
    """Mixin class that implements a series of tests to enforce the
    contract all implementations must follow."""

    def name(self):
        return 'TesterManager'

    def is_done(self):
        return self._done

    def handle_done(self, src, log_messages):
        self._done = True

    def handle_finished_test(self, src, an_int, log_messages):
        self._an_int = an_int

    def handle_exception(self, src, exception_type, exception_value, stack):
        raise exception_type(exception_value)

    def setUp(self):
        self._an_int = None
        self._broker = None
        self._done = False
        self._exception = None
        self._max_workers = None

    def make_broker(self, starting_queue=None, stopping_queue=None):
        self._broker = make_broker(self, self._max_workers, starting_queue,
                                   stopping_queue)

    def test_name(self):
        self.make_broker()
        worker = self._broker.start_worker(1)
        self.assertEquals(worker.name(), WORKER_NAME)
        worker.cancel()
        worker.join(0.1)
        self.assertFalse(worker.is_alive())
        self._broker.cleanup()

    def test_cancel(self):
        self.make_broker()
        worker = self._broker.start_worker(1)
        self._broker.post_message('test_list', 1, 'hello, world')
        worker.cancel()
        worker.join(0.1)
        self.assertFalse(worker.is_alive())
        self._broker.cleanup()

    def test_done(self):
        self.make_broker()
        worker = self._broker.start_worker(1)
        self._broker.post_message('test_list', 1, 'hello, world')
        self._broker.post_message('stop')
        self._broker.run_message_loop()
        worker.join(0.5)
        self.assertFalse(worker.is_alive())
        self.assertTrue(self.is_done())
        self.assertEqual(self._an_int, 2)
        self._broker.cleanup()

    def test_unknown_message(self):
        self.make_broker()
        worker = self._broker.start_worker(1)
        self._broker.post_message('unknown')
        try:
            self._broker.run_message_loop()
            self.fail()
        except ValueError, e:
            self.assertEquals(str(e),
                              "%s: received message 'unknown' it couldn't handle" % WORKER_NAME)
        finally:
            worker.join(0.5)
        self.assertFalse(worker.is_alive())
        self._broker.cleanup()


class InlineBrokerTests(_TestsMixin, unittest.TestCase):
    def setUp(self):
        _TestsMixin.setUp(self)
        self._max_workers = 1


# FIXME: https://bugs.webkit.org/show_bug.cgi?id=54520.
if sys.platform not in ('cygwin', 'win32'):

    class MultiProcessBrokerTests(_TestsMixin, unittest.TestCase):
        def setUp(self):
            _TestsMixin.setUp(self)
            self._max_workers = 2


class MessageTest(unittest.TestCase):
    def test__no_body(self):
        msg = manager_worker_broker._Message('src', 'topic_name', 'message_name', None)
        self.assertTrue(repr(msg))
        s = msg.dumps()
        new_msg = manager_worker_broker._Message.loads(s)
        self.assertEqual(new_msg.name, 'message_name')
        self.assertEqual(new_msg.args, None)
        self.assertEqual(new_msg.topic_name, 'topic_name')
        self.assertEqual(new_msg.src, 'src')

    def test__body(self):
        msg = manager_worker_broker._Message('src', 'topic_name', 'message_name', ('body', 0))
        self.assertTrue(repr(msg))
        s = msg.dumps()
        new_msg = manager_worker_broker._Message.loads(s)
        self.assertEqual(new_msg.name, 'message_name')
        self.assertEqual(new_msg.args, ('body', 0))
        self.assertEqual(new_msg.topic_name, 'topic_name')
        self.assertEqual(new_msg.src, 'src')



if __name__ == '__main__':
    unittest.main()
