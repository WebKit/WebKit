#!/usr/bin/python
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

"""Package that private an array-based implementation of a stream."""


class ArrayStream(object):
    """Simple class that implmements a stream interface on top of an array.

    This is used primarily by unit test classes to mock output streams. It
    performs a similar function to StringIO, but (a) it is write-only, and
    (b) it can be used to retrieve each individual write(); StringIO 
    concatenates all of the writes together.
    """

    def __init__(self):
        self._contents = []

    def write(self, msg):
        """Implement stream.write() by appending to the stream's contents."""
        self._contents.append(msg)

    def get(self):
        """Return the contents of a stream (as an array)."""
        return self._contents

    def reset(self):
        """Empty the stream."""
        self._contents = []

    def empty(self):
        """Return whether the stream is empty."""
        return (len(self._contents) == 0)

    def flush(self):
        """Flush the stream (a no-op implemented for compatibility)."""
        pass

    def __repr__(self):
        return '<ArrayStream: ' + str(self._contents) + '>'
