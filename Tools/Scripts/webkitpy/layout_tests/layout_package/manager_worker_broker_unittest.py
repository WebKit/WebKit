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


class TestWorker(manager_worker_broker.AbstractWorker):
    def __init__(self, broker_connection, worker_number, options):
        self._broker_connection = broker_connection
        self._options = options
        self._worker_number = worker_number
        self._name = 'TestWorker/%d' % worker_number
        self._stopped = False

    def handle_stop(self, src):
        self._stopped = True

    def handle_test(self, src, an_int, a_str):
        assert an_int == 1
        assert a_str == "hello, world"
        self._broker_connection.post_message('test', 2, 'hi, everybody')

    def is_done(self):
        return self._stopped

    def name(self):
        return self._name

    def start(self):
        pass

    def run(self, port):
        try:
            self._broker_connection.run_message_loop()
            self._broker_connection.yield_to_broker()
            self._broker_connection.post_message('done')
        except Exception, e:
            self._broker_connection.post_message('exception', (type(e), str(e), None))


def get_options(worker_model):
    option_list = manager_worker_broker.runtime_options()
    parser = optparse.OptionParser(option_list=option_list)
    options, args = parser.parse_args(args=['--worker-model', worker_model])
    return options


def make_broker(manager, worker_model):
    options = get_options(worker_model)
    return manager_worker_broker.get(port.get("test"), options, manager,
                                     TestWorker)


class FunctionTests(unittest.TestCase):
    def test_get__inline(self):
        self.assertTrue(make_broker(self, 'inline') is not None)

    def test_get__threads(self):
        self.assertTrue(make_broker(self, 'threads') is not None)

    def test_get__processes(self):
        if multiprocessing:
            self.assertTrue(make_broker(self, 'processes') is not None)
        else:
            self.assertRaises(ValueError, make_broker, self, 'processes')

    def test_get__unknown(self):
        self.assertRaises(ValueError, make_broker, self, 'unknown')


class _TestsMixin(object):
    """Mixin class that implements a series of tests to enforce the
    contract all implementations must follow."""

    #
    # Methods to implement the Manager side of the ClientInterface
    #
    def name(self):
        return 'Tester'

    def is_done(self):
        return self._done

    #
    # Handlers for the messages the TestWorker may send.
    #
    def handle_done(self, src):
        self._done = True

    def handle_test(self, src, an_int, a_str):
        self._an_int = an_int
        self._a_str = a_str

    def handle_exception(self, src, exc_info):
        self._exception = exc_info
        self._done = True

    #
    # Testing helper methods
    #
    def setUp(self):
        self._an_int = None
        self._a_str = None
        self._broker = None
        self._done = False
        self._exception = None
        self._worker_model = None

    def make_broker(self):
        self._broker = make_broker(self, self._worker_model)

    #
    # Actual unit tests
    #
    def test_done(self):
        if not self._worker_model:
            return
        self.make_broker()
        worker = self._broker.start_worker(0)
        self._broker.post_message('test', 1, 'hello, world')
        self._broker.post_message('stop')
        self._broker.run_message_loop()
        self.assertTrue(self.is_done())
        self.assertEqual(self._an_int, 2)
        self.assertEqual(self._a_str, 'hi, everybody')

    def test_unknown_message(self):
        if not self._worker_model:
            return
        self.make_broker()
        worker = self._broker.start_worker(0)
        self._broker.post_message('unknown')
        self._broker.run_message_loop()

        self.assertTrue(self.is_done())
        self.assertEquals(self._exception[0], ValueError)
        self.assertEquals(self._exception[1],
            "TestWorker/0: received message 'unknown' it couldn't handle")


class InlineBrokerTests(_TestsMixin, unittest.TestCase):
    def setUp(self):
        _TestsMixin.setUp(self)
        self._worker_model = 'inline'


class MultiProcessBrokerTests(_TestsMixin, unittest.TestCase):
    def setUp(self):
        _TestsMixin.setUp(self)
        if multiprocessing:
            self._worker_model = 'processes'
        else:
            self._worker_model = None

    def queue(self):
        return multiprocessing.Queue()


class ThreadedBrokerTests(_TestsMixin, unittest.TestCase):
    def setUp(self):
        _TestsMixin.setUp(self)
        self._worker_model = 'threads'


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


if __name__ == '__main__':
    unittest.main()
