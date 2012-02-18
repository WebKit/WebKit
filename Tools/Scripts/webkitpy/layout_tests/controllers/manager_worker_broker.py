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
import multiprocessing
import optparse
import Queue
import sys

from webkitpy.layout_tests.controllers import message_broker


_log = logging.getLogger(__name__)

#
# Topic names for Manager <-> Worker messaging
#
MANAGER_TOPIC = 'managers'
ANY_WORKER_TOPIC = 'workers'


def get(worker_model, client, worker_class):
    """Return a connection to a manager/worker message_broker

    Args:
        worker_model - concurrency model to use (inline/processes)
        client - message_broker.BrokerClient implementation to dispatch
            replies to.
        worker_class - type of workers to create. This class should override
            the methods in AbstractWorker.
    Returns:
        A handle to an object that will talk to a message broker configured
        for the normal manager/worker communication."""
    if worker_model == 'inline':
        queue_class = Queue.Queue
        manager_class = _InlineManager
    elif worker_model == 'processes':
        queue_class = multiprocessing.Queue
        manager_class = _MultiProcessManager
    else:
        raise ValueError("unsupported value for --worker-model: %s" % worker_model)

    broker = message_broker.Broker(queue_class)
    return manager_class(broker, client, worker_class)


class AbstractWorker(message_broker.BrokerClient):
    def __init__(self, worker_connection, worker_arguments=None):
        """The constructor should be used to do any simple initialization
        necessary, but should not do anything that creates data structures
        that cannot be Pickled or sent across processes (like opening
        files or sockets). Complex initialization should be done at the
        start of the run() call.

        Args:
            worker_connection - handle to the BrokerConnection object creating
                the worker and that can be used for messaging.
            worker_arguments - (optional, Picklable) object passed to the worker from the manager"""
        message_broker.BrokerClient.__init__(self)
        self._worker_connection = worker_connection
        self._name = 'worker'
        self._done = False
        self._canceled = False

    def name(self):
        return self._name

    def is_done(self):
        return self._done or self._canceled

    def stop_handling_messages(self):
        self._done = True

    def run(self):
        """Callback for the worker to start executing. Typically does any
        remaining initialization and then calls broker_connection.run_message_loop()."""
        exception_msg = ""
        _log.debug("%s starting" % self._name)

        try:
            self._worker_connection.run_message_loop()
            if not self.is_done():
                raise AssertionError("%s: ran out of messages in worker queue."
                                     % self._name)
        except KeyboardInterrupt:
            exception_msg = ", interrupted"
            self._worker_connection.raise_exception(sys.exc_info())
        except:
            exception_msg = ", exception raised"
            self._worker_connection.raise_exception(sys.exc_info())
        finally:
            _log.debug("%s done with message loop%s" % (self._name, exception_msg))

    def cancel(self):
        """Called when possible to indicate to the worker to stop processing
        messages and shut down. Note that workers may be stopped without this
        method being called, so clients should not rely solely on this."""
        self._canceled = True


class _ManagerConnection(message_broker.BrokerConnection):
    def __init__(self, broker, client, worker_class):
        """Base initialization for all Manager objects.

        Args:
            broker: handle to the message_broker object
            client: callback object (the caller)
            worker_class: class object to use to create workers.
        """
        message_broker.BrokerConnection.__init__(self, broker, client,
            MANAGER_TOPIC, ANY_WORKER_TOPIC)
        self._worker_class = worker_class

    def start_worker(self, worker_arguments=None):
        """Starts a new worker.

        Args:
            worker_arguments - an optional Picklable object that is passed to the worker constructor
        """
        raise NotImplementedError


class _InlineManager(_ManagerConnection):
    def __init__(self, broker, client, worker_class):
        _ManagerConnection.__init__(self, broker, client, worker_class)
        self._inline_worker = None

    def start_worker(self, worker_arguments=None):
        self._inline_worker = _InlineWorkerConnection(self._broker,
            self._client, self._worker_class, worker_arguments)
        return self._inline_worker

    def set_inline_arguments(self, arguments=None):
        # Note that this method only exists here, and not on all
        # ManagerConnections; calling this method on a MultiProcessManager
        # will deliberately result in a runtime error.
        self._inline_worker.set_inline_arguments(arguments)

    def run_message_loop(self, delay_secs=None):
        # Note that delay_secs is ignored in this case since we can't easily
        # implement it.
        self._inline_worker.run()
        self._broker.run_all_pending(MANAGER_TOPIC, self._client)


class _MultiProcessManager(_ManagerConnection):
    def start_worker(self, worker_arguments=None):
        worker_connection = _MultiProcessWorkerConnection(self._broker,
            self._worker_class, worker_arguments)
        worker_connection.start()
        return worker_connection


class _WorkerConnection(message_broker.BrokerConnection):
    def __init__(self, broker, worker_class, worker_arguments=None):
        self._client = worker_class(self, worker_arguments)
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
    def __init__(self, broker, manager_client, worker_class, worker_arguments):
        _WorkerConnection.__init__(self, broker, worker_class, worker_arguments)
        self._alive = False
        self._manager_client = manager_client

    def cancel(self):
        self._client.cancel()

    def is_alive(self):
        return self._alive

    def join(self, timeout):
        assert not self._alive

    def set_inline_arguments(self, arguments):
        self._client.set_inline_arguments(arguments)

    def run(self):
        self._alive = True
        try:
            self._client.run()
        finally:
            self._alive = False

    def yield_to_broker(self):
        self._broker.run_all_pending(MANAGER_TOPIC, self._manager_client)

    def raise_exception(self, exc_info):
        # Since the worker is in the same process as the manager, we can
        # raise the exception directly, rather than having to send it through
        # the queue. This allows us to preserve the traceback.
        raise exc_info[0], exc_info[1], exc_info[2]


class _Process(multiprocessing.Process):
    def __init__(self, worker_connection, client):
        multiprocessing.Process.__init__(self)
        self._worker_connection = worker_connection
        self._client = client

    def run(self):
        self._client.run()


class _MultiProcessWorkerConnection(_WorkerConnection):
    def __init__(self, broker, worker_class, worker_arguments):
        _WorkerConnection.__init__(self, broker, worker_class, worker_arguments)
        self._proc = _Process(self, self._client)

    def cancel(self):
        return self._proc.terminate()

    def is_alive(self):
        return self._proc.is_alive()

    def join(self, timeout):
        return self._proc.join(timeout)

    def start(self):
        self._proc.start()
