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

This module implements a message broker that connects the manager to the
workers: it provides a messaging abstraction and message loops (building on
top of message_broker), and handles starting workers by launching processes.

There are a lot of classes and objects involved in a fully connected system.
They interact more or less like:

  Manager  -->  _InlineManager ---> _InlineWorker <-> Worker
     ^                    \               /              ^
     |                     v             v               |
     \--------------------  MessageBroker   -------------/
"""

import logging
import optparse
import Queue
import sys

# Handle Python < 2.6 where multiprocessing isn't available.
try:
    import multiprocessing
except ImportError:
    multiprocessing = None

# These are needed when workers are launched in new child processes.
from webkitpy.common.host import Host

from webkitpy.layout_tests.controllers import message_broker
from webkitpy.layout_tests.views import printing


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
            "'inline' and 'processes'.")),
    ]
    return options


def get(port, options, client, worker_class):
    """Return a connection to a manager/worker message_broker

    Args:
        port - handle to layout_tests/port object for port-specific stuff
        options - optparse argument for command-line options
        client - message_broker.BrokerClient implementation to dispatch
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
    elif worker_model == 'processes' and multiprocessing:
        queue_class = multiprocessing.Queue
        manager_class = _MultiProcessManager
    else:
        raise ValueError("unsupported value for --worker-model: %s" % worker_model)

    broker = message_broker.Broker(options, queue_class)
    return manager_class(broker, port, options, client, worker_class)


class AbstractWorker(message_broker.BrokerClient):
    def __init__(self, worker_connection, worker_number, results_directory, options):
        """The constructor should be used to do any simple initialization
        necessary, but should not do anything that creates data structures
        that cannot be Pickled or sent across processes (like opening
        files or sockets). Complex initialization should be done at the
        start of the run() call.

        Args:
            worker_connection - handle to the BrokerConnection object creating
                the worker and that can be used for messaging.
            worker_number - identifier for this particular worker
            options - command-line argument object from optparse"""
        message_broker.BrokerClient.__init__(self)
        self._worker_connection = worker_connection
        self._options = options
        self._worker_number = worker_number
        self._name = 'worker/%d' % worker_number
        self._results_directory = results_directory

    def run(self, port):
        """Callback for the worker to start executing. Typically does any
        remaining initialization and then calls broker_connection.run_message_loop()."""
        raise NotImplementedError

    def cancel(self):
        """Called when possible to indicate to the worker to stop processing
        messages and shut down. Note that workers may be stopped without this
        method being called, so clients should not rely solely on this."""
        raise NotImplementedError


class _ManagerConnection(message_broker.BrokerConnection):
    def __init__(self, broker, options, client, worker_class):
        """Base initialization for all Manager objects.

        Args:
            broker: handle to the message_broker object
            options: command line options object
            client: callback object (the caller)
            worker_class: class object to use to create workers.
        """
        message_broker.BrokerConnection.__init__(self, broker, client,
            MANAGER_TOPIC, ANY_WORKER_TOPIC)
        self._options = options
        self._worker_class = worker_class

    def start_worker(self, worker_number, results_directory):
        raise NotImplementedError


class _InlineManager(_ManagerConnection):
    def __init__(self, broker, port, options, client, worker_class):
        _ManagerConnection.__init__(self, broker, options, client, worker_class)
        self._port = port
        self._inline_worker = None

    def start_worker(self, worker_number, results_directory):
        self._inline_worker = _InlineWorkerConnection(self._broker, self._port,
            self._client, self._worker_class, worker_number, results_directory)
        return self._inline_worker

    def run_message_loop(self, delay_secs=None):
        # Note that delay_secs is ignored in this case since we can't easily
        # implement it.
        self._inline_worker.run()
        self._broker.run_all_pending(MANAGER_TOPIC, self._client)


class _MultiProcessManager(_ManagerConnection):
    def __init__(self, broker, port, options, client, worker_class):
        # Note that this class does not keep a handle to the actual port
        # object, because it isn't Picklable. Instead it keeps the port
        # name and recreates the port in the child process from the name
        # and options.
        _ManagerConnection.__init__(self, broker, options, client, worker_class)
        self._platform_name = port.real_name()

    def start_worker(self, worker_number, results_directory):
        worker_connection = _MultiProcessWorkerConnection(self._broker, self._platform_name,
            self._worker_class, worker_number, results_directory, self._options)
        worker_connection.start()
        return worker_connection


class _WorkerConnection(message_broker.BrokerConnection):
    def __init__(self, broker, worker_class, worker_number, results_directory, options):
        self._client = worker_class(self, worker_number, results_directory, options)
        self.name = self._client.name()
        message_broker.BrokerConnection.__init__(self, broker, self._client,
                                                 ANY_WORKER_TOPIC, MANAGER_TOPIC)

    def cancel(self):
        raise NotImplementedError

    def is_alive(self):
        raise NotImplementedError

    def join(self, timeout):
        raise NotImplementedError

    def yield_to_broker(self):
        pass


class _InlineWorkerConnection(_WorkerConnection):
    def __init__(self, broker, port, manager_client, worker_class, worker_number, results_directory):
        _WorkerConnection.__init__(self, broker, worker_class, worker_number, results_directory, port.options)
        self._alive = False
        self._port = port
        self._manager_client = manager_client

    def cancel(self):
        self._client.cancel()

    def is_alive(self):
        return self._alive

    def join(self, timeout):
        assert not self._alive

    def run(self):
        self._alive = True
        self._client.run(self._port)
        self._alive = False

    def yield_to_broker(self):
        self._broker.run_all_pending(MANAGER_TOPIC, self._manager_client)

    def raise_exception(self, exc_info):
        # Since the worker is in the same process as the manager, we can
        # raise the exception directly, rather than having to send it through
        # the queue. This allows us to preserve the traceback.
        raise exc_info[0], exc_info[1], exc_info[2]


if multiprocessing:

    class _Process(multiprocessing.Process):
        def __init__(self, worker_connection, platform_name, options, client):
            multiprocessing.Process.__init__(self)
            self._worker_connection = worker_connection
            self._platform_name = platform_name
            self._options = options
            self._client = client

        def run(self):
            # We need to create a new Host object here because this is
            # running in a new process and we can't require the parent's
            # Host to be pickleable and passed to the child.
            host = Host()
            host._initialize_scm()

            options = self._options
            port_obj = host.port_factory.get(self._platform_name, options)

            # The unix multiprocessing implementation clones the
            # log handler configuration into the child processes,
            # but the win implementation doesn't.
            configure_logging = (sys.platform == 'win32')

            # FIXME: this won't work if the calling process is logging
            # somewhere other than sys.stderr and sys.stdout, but I'm not sure
            # if this will be an issue in practice.
            printer = printing.Printer(port_obj, options, sys.stderr, sys.stdout, configure_logging)
            self._client.run(port_obj)
            printer.cleanup()


class _MultiProcessWorkerConnection(_WorkerConnection):
    def __init__(self, broker, platform_name, worker_class, worker_number, results_directory, options):
        _WorkerConnection.__init__(self, broker, worker_class, worker_number, results_directory, options)
        self._proc = _Process(self, platform_name, options, self._client)

    def cancel(self):
        return self._proc.terminate()

    def is_alive(self):
        return self._proc.is_alive()

    def join(self, timeout):
        return self._proc.join(timeout)

    def start(self):
        self._proc.start()
