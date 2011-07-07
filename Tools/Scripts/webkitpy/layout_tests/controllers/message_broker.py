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

"""Module for handling messaging for run-webkit-tests.

This module implements a simple message broker abstraction that will be
used to coordinate messages between the main run-webkit-tests thread
(aka TestRunner) and the individual worker threads (previously known as
dump_render_tree_threads).

The broker simply distributes messages onto topics (named queues); the actual
queues themselves are provided by the caller, as the queue's implementation
requirements varies vary depending on the desired concurrency model
(none/threads/processes).

In order for shared-nothing messaging between processing to be possible,
Messages must be picklable.

The module defines one interface and two classes. Callers of this package
must implement the BrokerClient interface, and most callers will create
BrokerConnections as well as Brokers.

The classes relate to each other as:

    BrokerClient   ------>    BrokerConnection
         ^                         |
         |                         v
         \----------------      Broker

(The BrokerClient never calls broker directly after it is created, only
BrokerConnection.  BrokerConnection passes a reference to BrokerClient to
Broker, and Broker only invokes that reference, never talking directly to
BrokerConnection).
"""

import cPickle
import logging
import Queue
import time


_log = logging.getLogger(__name__)


class BrokerClient(object):
    """Abstract base class / interface that all message broker clients must
    implement. In addition to the methods below, by convention clients
    implement routines of the signature type

        handle_MESSAGE_NAME(self, src, ...):

    where MESSAGE_NAME matches the string passed to post_message(), and
    src indicates the name of the sender. If the message contains values in
    the message body, those will be provided as optparams."""

    def __init__(self, *optargs, **kwargs):
        raise NotImplementedError

    def is_done(self):
        """Called from inside run_message_loop() to indicate whether to exit."""
        raise NotImplementedError

    def name(self):
        """Return a name that identifies the client."""
        raise NotImplementedError


class Broker(object):
    """Brokers provide the basic model of a set of topics. Clients can post a
    message to any topic using post_message(), and can process messages on one
    topic at a time using run_message_loop()."""

    def __init__(self, options, queue_maker):
        """Args:
            options: a runtime option class from optparse
            queue_maker: a factory method that returns objects implementing a
                Queue interface (put()/get()).
        """
        self._options = options
        self._queue_maker = queue_maker
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
    def loads(str):
        obj = cPickle.loads(str)
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


class BrokerConnection(object):
    """BrokerConnection provides a connection-oriented facade on top of a
    Broker, so that callers don't have to repeatedly pass the same topic
    names over and over."""

    def __init__(self, broker, client, run_topic, post_topic):
        """Create a BrokerConnection on top of a Broker. Note that the Broker
        is passed in rather than created so that a single Broker can be used
        by multiple BrokerConnections."""
        self._broker = broker
        self._client = client
        self._post_topic = post_topic
        self._run_topic = run_topic
        broker.add_topic(run_topic)
        broker.add_topic(post_topic)

    def run_message_loop(self, delay_secs=None):
        self._broker.run_message_loop(self._run_topic, self._client, delay_secs)

    def post_message(self, message_name, *message_args):
        self._broker.post_message(self._client, self._post_topic,
                                  message_name, *message_args)
