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

This module builds on the functionality of message_broker2 to provide
a simple message broker for a manager sending messages to (and getting
replies from) a pool of workers. In addition it provides a choice of
concurrency models for callers to choose from (single-threaded inline
model, threads, or processes).

FIXME: At the moment, this module is just a bunch of stubs, but eventually
it will provide three sets of manager classes and worker classes for
the three concurrency models. The get() factory method selects between
them.
"""

import logging
import optparse
import Queue
import threading

#
# Handle Python < 2.6 where multiprocessing isn't available.
#
try:
    import multiprocessing
except ImportError:
    multiprocessing = None


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
        client - object to dispatch replies to
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
    def __init__(self, broker_connection, worker_number, options, **kwargs):
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
    def __init__(self, broker, port, options, client, worker_class):
        """Base initialization for all Manager objects.

        Args:
            broker: handle to the message_broker2 object
            port: handle to port-specific functionality
            options: command line options object
            client: callback object (the caller)
            worker_class: class object to use to create workers.
        """
        message_broker2.BrokerConnection.__init__(self, broker, client,
            MANAGER_TOPIC, ANY_WORKER_TOPIC)
        self._options = options
        self._worker_class = worker_class


class _InlineManager(_ManagerConnection):
    pass


class _ThreadedManager(_ManagerConnection):
    pass


class _MultiProcessManager(_ManagerConnection):
    pass
