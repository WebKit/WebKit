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

try:
    import multiprocessing
except ImportError:
    multiprocessing = None


from webkitpy.common.system import outputcapture

from webkitpy.layout_tests import port
from webkitpy.layout_tests.layout_package import manager_worker_broker
from webkitpy.layout_tests.layout_package import message_broker2
from webkitpy.layout_tests.layout_package import printing

# In order to reliably control when child workers are starting and stopping,
# we use a pair of global variables to hold queues used for messaging. Ideally
# we wouldn't need globals, but we can't pass these through a lexical closure
# because those can't be Pickled and sent to a subprocess, and we'd prefer not
# to have to pass extra arguments to the worker in the start_worker() call.
starting_queue = None
stopping_queue = None


def make_broker(manager, worker_model, start_queue=None, stop_queue=None):
    global starting_queue
    global stopping_queue
    starting_queue = start_queue
    stopping_queue = stop_queue
    options = get_options(worker_model)
    return manager_worker_broker.get(port.get("test"), options, manager, _TestWorker)


class _TestWorker(manager_worker_broker.AbstractWorker):
    def __init__(self, broker_connection, worker_number, options):
        self._broker_connection = broker_connection
        self._options = options
        self._worker_number = worker_number
        self._name = 'TestWorker/%d' % worker_number
        self._stopped = False
        self._canceled = False
        self._starting_queue = starting_queue
        self._stopping_queue = stopping_queue

    def handle_stop(self, src):
        self._stopped = True

    def handle_test(self, src, an_int, a_str):
        assert an_int == 1
        assert a_str == "hello, world"
        self._broker_connection.post_message('test', 2, 'hi, everybody')

    def is_done(self):
        return self._stopped or self._canceled

    def name(self):
        return self._name

    def cancel(self):
        self._canceled = True

    def run(self, port):
        if self._starting_queue:
            self._starting_queue.put('')

        if self._stopping_queue:
            self._stopping_queue.get()
        try:
            self._broker_connection.run_message_loop()
            self._broker_connection.yield_to_broker()
            self._broker_connection.post_message('done')
        except Exception, e:
            self._broker_connection.post_message('exception', (type(e), str(e), None))


def get_options(worker_model):
    option_list = (manager_worker_broker.runtime_options() +
                   printing.print_options() +
                   [optparse.make_option("--experimental-fully-parallel", default=False),
                    optparse.make_option("--child-processes", default='2')])
    parser = optparse.OptionParser(option_list=option_list)
    options, args = parser.parse_args(args=['--worker-model', worker_model])
    return options



class FunctionTests(unittest.TestCase):
    def test_get__inline(self):
        self.assertTrue(make_broker(self, 'inline') is not None)

    def test_get__threads(self):
        self.assertTrue(make_broker(self, 'threads') is not None)

    def test_get__processes(self):
        # This test sometimes fails on Windows. See <http://webkit.org/b/55087>.
        if sys.platform in ('cygwin', 'win32'):
            return

        if multiprocessing:
            self.assertTrue(make_broker(self, 'processes') is not None)
        else:
            self.assertRaises(ValueError, make_broker, self, 'processes')

    def test_get__unknown(self):
        self.assertRaises(ValueError, make_broker, self, 'unknown')


class _TestsMixin(object):
    """Mixin class that implements a series of tests to enforce the
    contract all implementations must follow."""

    def name(self):
        return 'Tester'

    def is_done(self):
        return self._done

    def handle_done(self, src):
        self._done = True

    def handle_test(self, src, an_int, a_str):
        self._an_int = an_int
        self._a_str = a_str

    def handle_exception(self, src, exc_info):
        self._exception = exc_info
        self._done = True

    def setUp(self):
        self._an_int = None
        self._a_str = None
        self._broker = None
        self._done = False
        self._exception = None
        self._worker_model = None

    def make_broker(self, starting_queue=None, stopping_queue=None):
        self._broker = make_broker(self, self._worker_model, starting_queue,
                                   stopping_queue)

    def test_cancel(self):
        self.make_broker()
        worker = self._broker.start_worker(0)
        worker.cancel()
        self._broker.post_message('test', 1, 'hello, world')
        worker.join(0.5)
        self.assertFalse(worker.is_alive())

    def test_done(self):
        self.make_broker()
        worker = self._broker.start_worker(0)
        self._broker.post_message('test', 1, 'hello, world')
        self._broker.post_message('stop')
        self._broker.run_message_loop()
        worker.join(0.5)
        self.assertFalse(worker.is_alive())
        self.assertTrue(self.is_done())
        self.assertEqual(self._an_int, 2)
        self.assertEqual(self._a_str, 'hi, everybody')

    def test_log_wedged_worker(self):
        starting_queue = self.queue()
        stopping_queue = self.queue()
        self.make_broker(starting_queue, stopping_queue)
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        try:
            worker = self._broker.start_worker(0)
            starting_queue.get()
            worker.log_wedged_worker('test_name')
            stopping_queue.put('')
            self._broker.post_message('stop')
            self._broker.run_message_loop()
            worker.join(0.5)
            self.assertFalse(worker.is_alive())
            self.assertTrue(self.is_done())
        finally:
            oc.restore_output()

    def test_unknown_message(self):
        self.make_broker()
        worker = self._broker.start_worker(0)
        self._broker.post_message('unknown')
        self._broker.run_message_loop()
        worker.join(0.5)

        self.assertTrue(self.is_done())
        self.assertFalse(worker.is_alive())
        self.assertEquals(self._exception[0], ValueError)
        self.assertEquals(self._exception[1],
            "TestWorker/0: received message 'unknown' it couldn't handle")


class InlineBrokerTests(_TestsMixin, unittest.TestCase):
    def setUp(self):
        _TestsMixin.setUp(self)
        self._worker_model = 'inline'

    def test_log_wedged_worker(self):
        self.make_broker()
        worker = self._broker.start_worker(0)
        self.assertRaises(AssertionError, worker.log_wedged_worker, None)


# FIXME: https://bugs.webkit.org/show_bug.cgi?id=54520.
if multiprocessing and sys.platform not in ('cygwin', 'win32'):

    class MultiProcessBrokerTests(_TestsMixin, unittest.TestCase):
        def setUp(self):
            _TestsMixin.setUp(self)
            self._worker_model = 'processes'

        def queue(self):
            return multiprocessing.Queue()


class ThreadedBrokerTests(_TestsMixin, unittest.TestCase):
    def setUp(self):
        _TestsMixin.setUp(self)
        self._worker_model = 'threads'

    def queue(self):
        return Queue.Queue()


class FunctionsTest(unittest.TestCase):
    def test_runtime_options(self):
        option_list = manager_worker_broker.runtime_options()
        parser = optparse.OptionParser(option_list=option_list)
        options, args = parser.parse_args([])
        self.assertTrue(options)


class InterfaceTest(unittest.TestCase):
    # These tests mostly exist to pacify coverage.

    # FIXME: There must be a better way to do this and also verify
    # that classes do implement every abstract method in an interface.
    def test_managerconnection_is_abstract(self):
        # Test that all the base class methods are abstract and have the
        # signature we expect.
        broker = make_broker(self, 'inline')
        obj = manager_worker_broker._ManagerConnection(broker._broker, None, self, None)
        self.assertRaises(NotImplementedError, obj.start_worker, 0)

    def test_workerconnection_is_abstract(self):
        # Test that all the base class methods are abstract and have the
        # signature we expect.
        broker = make_broker(self, 'inline')
        obj = manager_worker_broker._WorkerConnection(broker._broker, _TestWorker, 0, None)
        self.assertRaises(NotImplementedError, obj.cancel)
        self.assertRaises(NotImplementedError, obj.is_alive)
        self.assertRaises(NotImplementedError, obj.join, None)
        self.assertRaises(NotImplementedError, obj.log_wedged_worker, None)


if __name__ == '__main__':
    unittest.main()
