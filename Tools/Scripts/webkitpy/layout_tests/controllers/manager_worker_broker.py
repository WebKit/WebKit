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
     \-----------------------  Broker   ----------------/

The broker simply distributes messages onto topics (named queues); the actual
queues themselves are provided by the caller, as the queue's implementation
requirements varies vary depending on the desired concurrency model
(none/threads/processes).

In order for shared-nothing messaging between processing to be possible,
Messages must be picklable.

The module defines one interface and two classes. Callers of this package
must implement the BrokerClient interface, and most callers will create
_BrokerConnections as well as Brokers.

The classes relate to each other as:

    BrokerClient   ------>    _BrokerConnection
         ^                         |
         |                         v
         \----------------      _Broker

(The BrokerClient never calls broker directly after it is created, only
_BrokerConnection.  _BrokerConnection passes a reference to BrokerClient to
_Broker, and _Broker only invokes that reference, never talking directly to
BrokerConnection).
"""

import cPickle
import logging
import multiprocessing
import optparse
import Queue
import sys
import traceback


from webkitpy.common.system import stack_utils


_log = logging.getLogger(__name__)


#
# Topic names for Manager <-> Worker messaging
#
MANAGER_TOPIC = 'managers'
ANY_WORKER_TOPIC = 'workers'


def get(max_workers, client, worker_class):
    """Return a connection to a manager/worker message_broker

    Args:
        max_workers - max # of workers to run concurrently.
        client - BrokerClient implementation to dispatch
            replies to.
        worker_class - type of workers to create. This class should override
            the methods in AbstractWorker.
    Returns:
        A handle to an object that will talk to a message broker configured
        for the normal manager/worker communication."""
    if max_workers == 1:
        queue_class = Queue.Queue
        manager_class = _InlineManager
    else:
        queue_class = multiprocessing.Queue
        manager_class = _MultiProcessManager

    broker = _Broker(queue_class)
    return manager_class(broker, client, worker_class)


class BrokerClient(object):
    """Abstract base class / interface that all message broker clients must
    implement. In addition to the methods below, by convention clients
    implement routines of the signature type

        handle_MESSAGE_NAME(self, src, ...):

    where MESSAGE_NAME matches the string passed to post_message(), and
    src indicates the name of the sender. If the message contains values in
    the message body, those will be provided as optparams."""

    def is_done(self):
        """Called from inside run_message_loop() to indicate whether to exit."""
        raise NotImplementedError

    def name(self):
        """Return a name that identifies the client."""
        raise NotImplementedError


class _Broker(object):
    """Brokers provide the basic model of a set of topics. Clients can post a
    message to any topic using post_message(), and can process messages on one
    topic at a time using run_message_loop()."""

    def __init__(self, queue_maker):
        """Args:
            queue_maker: a factory method that returns objects implementing a
                Queue interface (put()/get()).
        """
        self._queue_maker = queue_maker
        self._topics = {}

    def __del__(self):
        self.cleanup()

    def cleanup(self):
        for queue in self._topics.values():
            if hasattr(queue, 'close'):
                queue.close()
        self._topics = {}

    def add_topic(self, topic_name):
        if topic_name not in self._topics:
            self._topics[topic_name] = self._queue_maker()

    def _get_queue_for_topic(self, topic_name):
        return self._topics[topic_name]

    def post_message(self, client, topic_name, message_name, *message_args):
        """Post a message to the appropriate topic name.

        Messages have a name and a tuple of optional arguments. Both must be picklable."""
        message = _Message(client.name(), topic_name, message_name, message_args)
        queue = self._get_queue_for_topic(topic_name)
        queue.put(_Message.dumps(message))

    def run_message_loop(self, topic_name, client, delay_secs=None):
        """Loop processing messages until client.is_done() or delay passes.

        To run indefinitely, set delay_secs to None."""
        assert delay_secs is None or delay_secs > 0
        self._run_loop(topic_name, client, block=True, delay_secs=delay_secs)

    def run_all_pending(self, topic_name, client):
        """Process messages until client.is_done() or caller would block."""
        self._run_loop(topic_name, client, block=False, delay_secs=None)

    def _run_loop(self, topic_name, client, block, delay_secs):
        queue = self._get_queue_for_topic(topic_name)
        while not client.is_done():
            try:
                s = queue.get(block, delay_secs)
            except Queue.Empty:
                return
            msg = _Message.loads(s)
            self._dispatch_message(msg, client)

    def _dispatch_message(self, message, client):
        if not hasattr(client, 'handle_' + message.name):
            raise ValueError(
               "%s: received message '%s' it couldn't handle" %
               (client.name(), message.name))
        optargs = message.args
        message_handler = getattr(client, 'handle_' + message.name)
        message_handler(message.src, *optargs)


class _Message(object):
    @staticmethod
    def loads(string_value):
        obj = cPickle.loads(string_value)
        assert(isinstance(obj, _Message))
        return obj

    def __init__(self, src, topic_name, message_name, message_args):
        self.src = src
        self.topic_name = topic_name
        self.name = message_name
        self.args = message_args

    def dumps(self):
        return cPickle.dumps(self)

    def __repr__(self):
        return ("_Message(from='%s', topic_name='%s', message_name='%s')" %
                (self.src, self.topic_name, self.name))


class _BrokerConnection(object):
    """_BrokerConnection provides a connection-oriented facade on top of a
    Broker, so that callers don't have to repeatedly pass the same topic
    names over and over."""

    def __init__(self, broker, client, run_topic, post_topic):
        """Create a _BrokerConnection on top of a _Broker. Note that the _Broker
        is passed in rather than created so that a single _Broker can be used
        by multiple _BrokerConnections."""
        self._broker = broker
        self._client = client
        self._post_topic = post_topic
        self._run_topic = run_topic
        broker.add_topic(run_topic)
        broker.add_topic(post_topic)

    def cleanup(self):
        self._broker.cleanup()
        self._broker = None

    def run_message_loop(self, delay_secs=None):
        self._broker.run_message_loop(self._run_topic, self._client, delay_secs)

    def post_message(self, message_name, *message_args):
        self._broker.post_message(self._client, self._post_topic,
                                  message_name, *message_args)

    def raise_exception(self, exc_info):
        # Since tracebacks aren't picklable, send the extracted stack instead.
        exception_type, exception_value, exception_traceback = sys.exc_info()
        stack_utils.log_traceback(_log.debug, exception_traceback)
        stack = traceback.extract_tb(exception_traceback)
        self._broker.post_message(self._client, self._post_topic, 'exception', exception_type, exception_value, stack)


class AbstractWorker(BrokerClient):
    def __init__(self, worker_connection, worker_arguments=None):
        """The constructor should be used to do any simple initialization
        necessary, but should not do anything that creates data structures
        that cannot be Pickled or sent across processes (like opening
        files or sockets). Complex initialization should be done at the
        start of the run() call.

        Args:
            worker_connection - handle to the _BrokerConnection object creating
                the worker and that can be used for messaging.
            worker_arguments - (optional, Picklable) object passed to the worker from the manager"""
        BrokerClient.__init__(self)
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


class _ManagerConnection(_BrokerConnection):
    def __init__(self, broker, client, worker_class):
        """Base initialization for all Manager objects.

        Args:
            broker: handle to the message_broker object
            client: callback object (the caller)
            worker_class: class object to use to create workers.
        """
        _BrokerConnection.__init__(self, broker, client, MANAGER_TOPIC, ANY_WORKER_TOPIC)
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


class _WorkerConnection(_BrokerConnection):
    def __init__(self, broker, worker_class, worker_arguments=None):
        self._client = worker_class(self, worker_arguments)
        _BrokerConnection.__init__(self, broker, self._client, ANY_WORKER_TOPIC, MANAGER_TOPIC)

    def name(self):
        return self._client.name()

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
