# Copyright 2009, Google Inc.
# All rights reserved.
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


"""Message related utilities.

Note: request.connection.write/read are used in this module, even though
mod_python document says that they should be used only in connection handlers.
Unfortunately, we have no other options. For example, request.write/read are
not suitable because they don't allow direct raw bytes writing/reading.
"""


import Queue
import threading
import util


class MsgUtilException(Exception):
    pass


def _read(request, length):
    bytes = request.connection.read(length)
    if not bytes:
        raise MsgUtilException(
                'Failed to receive message from %r' %
                        (request.connection.remote_addr,))
    return bytes


def _write(request, bytes):
    try:
        request.connection.write(bytes)
    except Exception, e:
        util.prepend_message_to_exception(
                'Failed to send message to %r: ' %
                        (request.connection.remote_addr,),
                e)
        raise


def send_message(request, message):
    """Send message.

    Args:
        request: mod_python request.
        message: unicode string to send.
    """

    _write(request, '\x00' + message.encode('utf-8') + '\xff')


def receive_message(request):
    """Receive a Web Socket frame and return its payload as unicode string.

    Args:
        request: mod_python request.
    """

    while True:
        # Read 1 byte.
        # mp_conn.read will block if no bytes are available.
        # Timeout is controlled by TimeOut directive of Apache.
        frame_type_str = _read(request, 1)
        frame_type = ord(frame_type_str[0])
        if (frame_type & 0x80) == 0x80:
            # The payload length is specified in the frame.
            # Read and discard.
            length = _payload_length(request)
            _receive_bytes(request, length)
        else:
            # The payload is delimited with \xff.
            bytes = _read_until(request, '\xff')
            # The Web Socket protocol section 4.4 specifies that invalid
            # characters must be replaced with U+fffd REPLACEMENT CHARACTER.
            message = bytes.decode('utf-8', 'replace')
            if frame_type == 0x00:
                return message
            # Discard data of other types.


def _payload_length(request):
    length = 0
    while True:
        b_str = _read(request, 1)
        b = ord(b_str[0])
        length = length * 128 + (b & 0x7f)
        if (b & 0x80) == 0:
            break
    return length


def _receive_bytes(request, length):
    bytes = []
    while length > 0:
        new_bytes = _read(request, length)
        bytes.append(new_bytes)
        length -= len(new_bytes)
    return ''.join(bytes)


def _read_until(request, delim_char):
    bytes = []
    while True:
        ch = _read(request, 1)
        if ch == delim_char:
            break
        bytes.append(ch)
    return ''.join(bytes)


class MessageReceiver(threading.Thread):
    """This class receives messages from the client.

    This class provides three ways to receive messages: blocking, non-blocking,
    and via callback. Callback has the highest precedence.

    Note: This class should not be used with the standalone server for wss
    because pyOpenSSL used by the server raises a fatal error if the socket
    is accessed from multiple threads.
    """
    def __init__(self, request, onmessage=None):
        """Construct an instance.

        Args:
            request: mod_python request.
            onmessage: a function to be called when a message is received.
                       May be None. If not None, the function is called on
                       another thread. In that case, MessageReceiver.receive
                       and MessageReceiver.receive_nowait are useless because
                       they will never return any messages.
        """
        threading.Thread.__init__(self)
        self._request = request
        self._queue = Queue.Queue()
        self._onmessage = onmessage
        self._stop_requested = False
        self.setDaemon(True)
        self.start()

    def run(self):
        while not self._stop_requested:
            message = receive_message(self._request)
            if self._onmessage:
                self._onmessage(message)
            else:
                self._queue.put(message)

    def receive(self):
        """ Receive a message from the channel, blocking.

        Returns:
            message as a unicode string.
        """
        return self._queue.get()

    def receive_nowait(self):
        """ Receive a message from the channel, non-blocking.

        Returns:
            message as a unicode string if available. None otherwise.
        """
        try:
            message = self._queue.get_nowait()
        except Queue.Empty:
            message = None
        return message

    def stop(self):
        """Request to stop this instance.

        The instance will be stopped after receiving the next message.
        This method may not be very useful, but there is no clean way
        in Python to forcefully stop a running thread.
        """
        self._stop_requested = True


class MessageSender(threading.Thread):
    """This class sends messages to the client.

    This class provides both synchronous and asynchronous ways to send
    messages.

    Note: This class should not be used with the standalone server for wss
    because pyOpenSSL used by the server raises a fatal error if the socket
    is accessed from multiple threads.
    """
    def __init__(self, request):
        """Construct an instance.

        Args:
            request: mod_python request.
        """
        threading.Thread.__init__(self)
        self._request = request
        self._queue = Queue.Queue()
        self.setDaemon(True)
        self.start()

    def run(self):
        while True:
            message, condition = self._queue.get()
            condition.acquire()
            send_message(self._request, message)
            condition.notify()
            condition.release()

    def send(self, message):
        """Send a message, blocking."""

        condition = threading.Condition()
        condition.acquire()
        self._queue.put((message, condition))
        condition.wait()

    def send_nowait(self, message):
        """Send a message, non-blocking."""

        self._queue.put((message, threading.Condition()))


# vi:sts=4 sw=4 et
