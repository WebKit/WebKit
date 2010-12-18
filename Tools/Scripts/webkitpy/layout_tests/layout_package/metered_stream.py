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

This package should only be called by the printing module in the layout_tests
package.
"""

import logging

_log = logging.getLogger("webkitpy.layout_tests.metered_stream")


class MeteredStream:
    """This class is a wrapper around a stream that allows you to implement
    meters (progress bars, etc.).

    It can be used directly as a stream, by calling write(), but provides
    two other methods for output, update(), and progress().

    In normal usage, update() will overwrite the output of the immediately
    preceding update() (write() also will overwrite update()). So, calling
    multiple update()s in a row can provide an updating status bar (note that
    if an update string contains newlines, only the text following the last
    newline will be overwritten/erased).

    If the MeteredStream is constructed in "verbose" mode (i.e., by passing
    verbose=true), then update() no longer overwrite a previous update(), and
    instead the call is equivalent to write(), although the text is
    actually sent to the logger rather than to the stream passed
    to the constructor.

    progress() is just like update(), except that if you are in verbose mode,
    progress messages are not output at all (they are dropped). This is
    used for things like progress bars which are presumed to be unwanted in
    verbose mode.

    Note that the usual usage for this class is as a destination for
    a logger that can also be written to directly (i.e., some messages go
    through the logger, some don't). We thus have to dance around a
    layering inversion in update() for things to work correctly.
    """

    def __init__(self, verbose, stream):
        """
        Args:
          verbose: whether progress is a no-op and updates() aren't overwritten
          stream: output stream to write to
        """
        self._dirty = False
        self._verbose = verbose
        self._stream = stream
        self._last_update = ""

    def write(self, txt):
        """Write to the stream, overwriting and resetting the meter."""
        if self._dirty:
            self._write(txt)
            self._dirty = False
            self._last_update = ''
        else:
            self._stream.write(txt)

    def flush(self):
        """Flush any buffered output."""
        self._stream.flush()

    def progress(self, str):
        """
        Write a message to the stream that will get overwritten.

        This is used for progress updates that don't need to be preserved in
        the log. If the MeteredStream was initialized with verbose==True,
        then this output is discarded. We have this in case we are logging
        lots of output and the update()s will get lost or won't work
        properly (typically because verbose streams are redirected to files).

        """
        if self._verbose:
            return
        self._write(str)

    def update(self, str):
        """
        Write a message that is also included when logging verbosely.

        This routine preserves the same console logging behavior as progress(),
        but will also log the message if verbose() was true.

        """
        # Note this is a separate routine that calls either into the logger
        # or the metering stream. We have to be careful to avoid a layering
        # inversion (stream calling back into the logger).
        if self._verbose:
            _log.info(str)
        else:
            self._write(str)

    def _write(self, str):
        """Actually write the message to the stream."""

        # FIXME: Figure out if there is a way to detect if we're writing
        # to a stream that handles CRs correctly (e.g., terminals). That might
        # be a cleaner way of handling this.

        # Print the necessary number of backspaces to erase the previous
        # message.
        if len(self._last_update):
            self._stream.write("\b" * len(self._last_update) +
                               " " * len(self._last_update) +
                               "\b" * len(self._last_update))
        self._stream.write(str)
        last_newline = str.rfind("\n")
        self._last_update = str[(last_newline + 1):]
        self._dirty = True
