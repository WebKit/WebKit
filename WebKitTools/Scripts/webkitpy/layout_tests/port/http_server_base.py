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

import logging
import os
import time
import urllib

_log = logging.getLogger("webkitpy.layout_tests.port.http_server_base")


class HttpServerBase(object):

    def __init__(self, port_obj):
        self._port_obj = port_obj

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

    def is_server_running_on_all_ports(self):
        """Returns whether the server is running on all the desired ports."""
        for mapping in self.mappings:
            if 'sslcert' in mapping:
                http_suffix = 's'
            else:
                http_suffix = ''

            url = 'http%s://127.0.0.1:%d/' % (http_suffix, mapping['port'])

            try:
                response = urllib.urlopen(url)
                _log.debug("Server running at %s" % url)
            except IOError, e:
                _log.debug("Server NOT running at %s: %s" % (url, e))
                return False

        return True

    def remove_log_files(self, folder, starts_with):
        files = os.listdir(folder)
        for file in files:
            if file.startswith(starts_with):
                full_path = os.path.join(folder, file)
                os.remove(full_path)
