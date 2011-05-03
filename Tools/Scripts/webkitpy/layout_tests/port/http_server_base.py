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

"""Base class with common routines between the Apache and Lighttpd servers."""

import errno
import logging
import socket
import time

from webkitpy.common.system import filesystem

_log = logging.getLogger("webkitpy.layout_tests.port.http_server_base")


class ServerError(Exception):
    pass


class HttpServerBase(object):

    def __init__(self, port_obj):
        self._port_obj = port_obj
        self._executive = port_obj._executive
        self._filesystem = port_obj._filesystem
        self._process = None

    def wait_for_action(self, action):
        """Repeat the action for 20 seconds or until it succeeds. Returns
        whether it succeeded."""
        start_time = time.time()
        while time.time() - start_time < 20:
            if action():
                return True
            _log.debug("Waiting for action: %s" % action)
            time.sleep(1)

        return False

    def is_running(self):
        return self._process and self._process.returncode is None

    def is_server_running_on_all_ports(self):
        """Returns whether the server is running on all the desired ports."""
        if not self._executive.check_running_pid(self._process.pid):
            _log.debug("Server isn't running at all")
            raise ServerError("Server exited")

        for mapping in self.mappings:
            s = socket.socket()
            port = mapping['port']
            try:
                s.connect(('localhost', port))
                _log.debug("Server running on %d" % port)
            except IOError, e:
                if e.errno not in (errno.ECONNREFUSED, errno.ECONNRESET):
                    raise
                _log.debug("Server NOT running on %d: %s" % (port, e))
                return False
            finally:
                s.close()
        return True

    def check_that_all_ports_are_available(self):
        for mapping in self.mappings:
            s = socket.socket()
            port = mapping['port']
            try:
                s.bind(('localhost', port))
            except IOError, e:
                if e.errno in (errno.EALREADY, errno.EADDRINUSE):
                    raise ServerError('Port %d is already in use.' % port)
                else:
                    raise ServerError('Unexpected error: %s.' % str(e))
            finally:
                s.close()

    def remove_log_files(self, folder, starts_with):
        files = self._filesystem.listdir(folder)
        for file in files:
            if file.startswith(starts_with):
                full_path = self._filesystem.join(folder, file)
                self._filesystem.remove(full_path)
