#!/usr/bin/env python
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

"""
Package that implements a stream wrapper that has 'meters' as well as
regular output. A 'meter' is a single line of text that can be erased
and rewritten repeatedly, without producing multiple lines of output. It
can be used to produce effects like progress bars.
"""


class MeteredStream:
    """This class is a wrapper around a stream that allows you to implement
    meters.

    It can be used like a stream, but calling update() will print
    the string followed by only a carriage return (instead of a carriage
    return and a line feed). This can be used to implement progress bars and
    other sorts of meters. Note that anything written by update() will be
    erased by a subsequent update(), write(), or flush()."""

    def __init__(self, verbose, stream):
        """
        Args:
          verbose: whether update is a no-op
          stream: output stream to write to
        """
        self._dirty = False
        self._verbose = verbose
        self._stream = stream
        self._last_update = ""

    def write(self, txt):
        """Write text directly to the stream, overwriting and resetting the
        meter."""
        if self._dirty:
            self.update("")
            self._dirty = False
        self._stream.write(txt)

    def flush(self):
        """Flush any buffered output."""
        self._stream.flush()

    def update(self, str):
        """Write an update to the stream that will get overwritten by the next
        update() or by a write().

        This is used for progress updates that don't need to be preserved in
        the log. Note that verbose disables this routine; we have this in
        case we are logging lots of output and the update()s will get lost
        or won't work properly (typically because verbose streams are
        redirected to files.

        TODO(dpranke): figure out if there is a way to detect if we're writing
        to a stream that handles CRs correctly (e.g., terminals). That might
        be a cleaner way of handling this.
        """
        if self._verbose:
            return

        # Print the necessary number of backspaces to erase the previous
        # message.
        self._stream.write("\b" * len(self._last_update))
        self._stream.write(str)
        num_remaining = len(self._last_update) - len(str)
        if num_remaining > 0:
            self._stream.write(" " * num_remaining + "\b" * num_remaining)
        self._last_update = str
        self._dirty = True
