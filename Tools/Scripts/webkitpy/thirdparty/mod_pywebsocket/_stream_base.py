# Copyright 2011, Google Inc.
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


"""Base stream class.
"""


# Note: request.connection.write/read are used in this module, even though
# mod_python document says that they should be used only in connection
# handlers. Unfortunately, we have no other options. For example,
# request.write/read are not suitable because they don't allow direct raw bytes
# writing/reading.


from mod_pywebsocket import util


# Exceptions


class ConnectionTerminatedException(Exception):
    """This exception will be raised when a connection is terminated
    unexpectedly.
    """

    pass


class InvalidFrameException(ConnectionTerminatedException):
    """This exception will be raised when we received an invalid frame we
    cannot parse.
    """

    pass


class BadOperationException(Exception):
    """This exception will be raised when send_message() is called on
    server-terminated connection or receive_message() is called on
    client-terminated connection.
    """

    pass


class UnsupportedFrameException(Exception):
    """This exception will be raised when we receive a frame with flag, opcode
    we cannot handle. Handlers can just catch and ignore this exception and
    call receive_message() again to continue processing the next frame.
    """

    pass


class InvalidUTF8Exception(Exception):
    """This exception will be raised when we receive a text frame which
    contains invalid UTF-8 strings.
    """

    pass


class StreamBase(object):
    """Base stream class."""

    def __init__(self, request):
        """Construct an instance.

        Args:
            request: mod_python request.
        """

        self._logger = util.get_class_logger(self)

        self._request = request

    def _read(self, length):
        """Reads length bytes from connection. In case we catch any exception,
        prepends remote address to the exception message and raise again.

        Raises:
            ConnectionTerminatedException: when read returns empty string.
        """

        bytes = self._request.connection.read(length)
        if not bytes:
            raise ConnectionTerminatedException(
                'Receiving %d byte failed. Peer (%r) closed connection' %
                (length, (self._request.connection.remote_addr,)))
        return bytes

    def _write(self, bytes):
        """Writes given bytes to connection. In case we catch any exception,
        prepends remote address to the exception message and raise again.
        """

        try:
            self._request.connection.write(bytes)
        except Exception, e:
            util.prepend_message_to_exception(
                    'Failed to send message to %r: ' %
                            (self._request.connection.remote_addr,),
                    e)
            raise

    def receive_bytes(self, length):
        """Receives multiple bytes. Retries read when we couldn't receive the
        specified amount.

        Raises:
            ConnectionTerminatedException: when read returns empty string.
        """

        bytes = []
        while length > 0:
            new_bytes = self._read(length)
            bytes.append(new_bytes)
            length -= len(new_bytes)
        return ''.join(bytes)

    def _read_until(self, delim_char):
        """Reads bytes until we encounter delim_char. The result will not
        contain delim_char.

        Raises:
            ConnectionTerminatedException: when read returns empty string.
        """

        bytes = []
        while True:
            ch = self._read(1)
            if ch == delim_char:
                break
            bytes.append(ch)
        return ''.join(bytes)


# vi:sts=4 sw=4 et
