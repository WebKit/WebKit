# Copyright (C) 2011 Google Inc. All rights reserved.
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

"""Module for handling messages and concurrency for run-webkit-tests.

This module implements a message broker that connects the manager
(TestRunner2) to the workers: it provides a messaging abstraction and
message loops (building on top of message_broker2), and handles starting
workers by launching threads and/or processes depending on the
requested configuration.

There are a lot of classes and objects involved in a fully connected system.
They interact more or less like:

TestRunner2  --> _InlineManager ---> _InlineWorker <-> Worker
     ^                    \               /              ^
     |                     v             v               |
     \--------------------  MessageBroker   -------------/
"""

import logging
import optparse
import printing
import Queue
import sys
import thread
import threading
import time


# Handle Python < 2.6 where multiprocessing isn't available.
try:
    import multiprocessing
except ImportError:
    multiprocessing = None


from webkitpy.common.system import stack_utils
from webkitpy.layout_tests import port
from webkitpy.layout_tests.layout_package import message_broker2


_log = logging.getLogger(__name__)

#
# Topic names for Manager <-> Worker messaging
#
MANAGER_TOPIC = 'managers'
ANY_WORKER_TOPIC = 'workers'


def runtime_options():
    """Return a list of optparse.Option objects for any runtime values used
    by this module."""
    options = [
        optparse.make_option("--worker-model", action="store",
            help=("controls worker model. Valid values are "
            "'inline', 'threads', and 'processes'.")),
    ]
    return options


def get(port, options, client, worker_class):
    """Return a connection to a manager/worker message_broker

    Args:
        port - handle to layout_tests/port object for port-specific stuff
        options - optparse argument for command-line options
        client - message_broker2.BrokerClient implementation to dispatch
            replies to.
        worker_class - type of workers to create. This class must implement
            the methods in AbstractWorker.
    Returns:
        A handle to an object that will talk to a message broker configured
        for the normal manager/worker communication.
    """
    worker_model = options.worker_model
    if worker_model == 'inline':
        queue_class = Queue.Queue
        manager_class = _InlineManager
    elif worker_model == 'threads':
        queue_class = Queue.Queue
        manager_class = _ThreadedManager
    elif worker_model == 'processes' and multiprocessing:
        queue_class = multiprocessing.Queue
        manager_class = _MultiProcessManager
    else:
        raise ValueError("unsupported value for --worker-model: %s" %
                         worker_model)

    broker = message_broker2.Broker(options, queue_class)
    return manager_class(broker, port, options, client, worker_class)


class AbstractWorker(message_broker2.BrokerClient):
    def __init__(self, broker_connection, worker_number, options):
        """The constructor should be used to do any simple initialization
        necessary, but should not do anything that creates data structures
        that cannot be Pickled or sent across processes (like opening
        files or sockets). Complex initialization should be done at the
        start of the run() call.

        Args:
            broker_connection - handle to the BrokerConnection object creating
                the worker and that can be used for messaging.
            worker_number - identifier for this particular worker
            options - command-line argument object from optparse"""

        raise NotImplementedError

    def run(self, port):
        """Callback for the worker to start executing. Typically does any
        remaining initialization and then calls broker_connection.run_message_loop()."""
        raise NotImplementedError

    def cancel(self):
        """Called when possible to indicate to the worker to stop processing
        messages and shut down. Note that workers may be stopped without this
        method being called, so clients should not rely solely on this."""
        raise NotImplementedError


class _ManagerConnection(message_broker2.BrokerConnection):
    def __init__(self, broker, options, client, worker_class):
        """Base initialization for all Manager objects.

        Args:
            broker: handle to the message_broker2 object
            options: command line options object
            client: callback object (the caller)
            worker_class: class object to use to create workers.
        """
        message_broker2.BrokerConnection.__init__(self, broker, client,
            MANAGER_TOPIC, ANY_WORKER_TOPIC)
        self._options = options
        self._worker_class = worker_class

    def start_worker(self, worker_number):
        raise NotImplementedError


class _InlineManager(_ManagerConnection):
    def __init__(self, broker, port, options, client, worker_class):
        _ManagerConnection.__init__(self, broker, options, client, worker_class)
        self._port = port
        self._inline_worker = None

    def start_worker(self, worker_number):
        self._inline_worker = _InlineWorkerConnection(self._broker, self._port,
            self._client, self._worker_class, worker_number)
        return self._inline_worker

    def run_message_loop(self, delay_secs=None):
        # Note that delay_secs is ignored in this case since we can't easily
        # implement it.
        self._inline_worker.run()
        self._broker.run_all_pending(MANAGER_TOPIC, self._client)


class _ThreadedManager(_ManagerConnection):
    def __init__(self, broker, port, options, client, worker_class):
        _ManagerConnection.__init__(self, broker, options, client, worker_class)
        self._port = port

    def start_worker(self, worker_number):
        worker_connection = _ThreadedWorkerConnection(self._broker, self._port,
            self._worker_class, worker_number)
        worker_connection.start()
        return worker_connection


class _MultiProcessManager(_ManagerConnection):
    def __init__(self, broker, port, options, client, worker_class):
        # Note that this class does not keep a handle to the actual port
        # object, because it isn't Picklable. Instead it keeps the port
        # name and recreates the port in the child process from the name
        # and options.
        _ManagerConnection.__init__(self, broker, options, client, worker_class)
        self._platform_name = port.real_name()

    def start_worker(self, worker_number):
        worker_connection = _MultiProcessWorkerConnection(self._broker, self._platform_name,
            self._worker_class, worker_number, self._options)
        worker_connection.start()
        return worker_connection


class _WorkerConnection(message_broker2.BrokerConnection):
    def __init__(self, broker, worker_class, worker_number, options):
        self._client = worker_class(self, worker_number, options)
        self.name = self._client.name()
        message_broker2.BrokerConnection.__init__(self, broker, self._client,
                                                  ANY_WORKER_TOPIC, MANAGER_TOPIC)

    def cancel(self):
        raise NotImplementedError

    def is_alive(self):
        raise NotImplementedError

    def join(self, timeout):
        raise NotImplementedError

    def log_wedged_worker(self, test_name):
        raise NotImplementedError

    def yield_to_broker(self):
        pass


class _InlineWorkerConnection(_WorkerConnection):
    def __init__(self, broker, port, manager_client, worker_class, worker_number):
        _WorkerConnection.__init__(self, broker, worker_class, worker_number, port._options)
        self._alive = False
        self._port = port
        self._manager_client = manager_client

    def cancel(self):
        self._client.cancel()

    def is_alive(self):
        return self._alive

    def join(self, timeout):
        assert not self._alive

    def log_wedged_worker(self, test_name):
        assert False, "_InlineWorkerConnection.log_wedged_worker() called"

    def run(self):
        self._alive = True
        self._client.run(self._port)
        self._alive = False

    def yield_to_broker(self):
        self._broker.run_all_pending(MANAGER_TOPIC, self._manager_client)


class _Thread(threading.Thread):
    def __init__(self, worker_connection, port, client):
        threading.Thread.__init__(self)
        self._worker_connection = worker_connection
        self._port = port
        self._client = client

    def cancel(self):
        return self._client.cancel()

    def log_wedged_worker(self, test_name):
        stack_utils.log_thread_state(_log.error, self._client.name(), self.ident, " is wedged on test %s" % test_name)

    def run(self):
        # FIXME: We can remove this once everyone is on 2.6.
        if not hasattr(self, 'ident'):
            self.ident = thread.get_ident()
        self._client.run(self._port)


class _ThreadedWorkerConnection(_WorkerConnection):
    def __init__(self, broker, port, worker_class, worker_number):
        _WorkerConnection.__init__(self, broker, worker_class, worker_number, port._options)
        self._thread = _Thread(self, port, self._client)

    def cancel(self):
        return self._thread.cancel()

    def is_alive(self):
        # FIXME: Change this to is_alive once everyone is on 2.6.
        return self._thread.isAlive()

    def join(self, timeout):
        return self._thread.join(timeout)

    def log_wedged_worker(self, test_name):
        return self._thread.log_wedged_worker(test_name)

    def start(self):
        self._thread.start()


if multiprocessing:

    class _Process(multiprocessing.Process):
        def __init__(self, worker_connection, platform_name, options, client):
            multiprocessing.Process.__init__(self)
            self._worker_connection = worker_connection
            self._platform_name = platform_name
            self._options = options
            self._client = client

        def log_wedged_worker(self, test_name):
            _log.error("%s (pid %d) is wedged on test %s" % (self.name, self.pid, test_name))

        def run(self):
            options = self._options
            port_obj = port.get(self._platform_name, options)
            # FIXME: this won't work if the calling process is logging
            # somewhere other than sys.stderr and sys.stdout, but I'm not sure
            # if this will be an issue in practice.
            printer = printing.Printer(port_obj, options, sys.stderr, sys.stdout,
                int(options.child_processes), options.experimental_fully_parallel)
            self._client.run(port_obj)
            printer.cleanup()


class _MultiProcessWorkerConnection(_WorkerConnection):
    def __init__(self, broker, platform_name, worker_class, worker_number, options):
        _WorkerConnection.__init__(self, broker, worker_class, worker_number, options)
        self._proc = _Process(self, platform_name, options, self._client)

    def cancel(self):
        return self._proc.terminate()

    def is_alive(self):
        return self._proc.is_alive()

    def join(self, timeout):
        return self._proc.join(timeout)

    def log_wedged_worker(self, test_name):
        return self._proc.log_wedged_worker(test_name)

    def start(self):
        self._proc.start()
