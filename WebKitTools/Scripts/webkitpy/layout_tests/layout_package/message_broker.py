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

"""Module for handling messages, threads, processes, and concurrency for run-webkit-tests.

The model we use is that of a message broker - it provides a messaging
abstraction and message loops, and handles launching threads and/or processes
depending on the requested configuration.
"""

import logging
import sys
import traceback


_log = logging.getLogger(__name__)


def log_wedged_thread(id):
    """Log information about the given thread state."""
    stack = _find_thread_stack(id)
    assert(stack is not None)
    _log.error("")
    _log.error("Thread %d is wedged" % id)
    _log_stack(stack)
    _log.error("")


def _find_thread_stack(id):
    """Returns a stack object that can be used to dump a stack trace for
    the given thread id (or None if the id is not found)."""
    for thread_id, stack in sys._current_frames().items():
        if thread_id == id:
            return stack
    return None


def _log_stack(stack):
    """Log a stack trace to log.error()."""
    for filename, lineno, name, line in traceback.extract_stack(stack):
        _log.error('File: "%s", line %d, in %s' % (filename, lineno, name))
        if line:
            _log.error('  %s' % line.strip())
