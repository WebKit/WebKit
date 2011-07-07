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


class MeteredStream:
    """This class is a wrapper around a stream that allows you to implement
    meters (progress bars, etc.).

    It can be used directly as a stream, by calling write(), but also provides
    a method called update() that will overwite prior updates().
   """

    def __init__(self, stream):
        """
        Args:
          stream: output stream to write to
        """
        self._stream = stream
        self._dirty = False
        self._last_update = ""

    def write(self, txt):
        """Write to the stream, overwriting and resetting the meter."""

        # This routine is called by the logging infrastructure, and
        # must not call back into logging. It is not a public function.
        self._overwrite(txt)
        self._reset()

    def update(self, txt):
        """Write a message that will be overwritten by subsequent update() or write() calls."""
        self._overwrite(txt)

    def flush(self):
        # This seems to be needed on Python 2.5 for some reason.
        self._stream.flush()

    def _overwrite(self, txt):
        # Print the necessary number of backspaces to erase the previous
        # message.
        if len(self._last_update):
            self._stream.write("\b" * len(self._last_update) +
                               " " * len(self._last_update) +
                               "\b" * len(self._last_update))
        self._stream.write(txt)
        last_newline = txt.rfind("\n")
        self._last_update = txt[(last_newline + 1):]
        self._dirty = True

    def _reset(self):
        self._dirty = False
        self._last_update = ''
