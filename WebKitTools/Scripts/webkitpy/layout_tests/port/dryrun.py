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
#     * Neither the Google name nor the names of its
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

"""This is a test implementation of the Port interface that generates the
   correct output for every test. It can be used for perf testing, because
   it is pretty much a lower limit on how fast a port can possibly run.

   This implementation acts as a wrapper around a real port (the real port
   is held as a delegate object). To specify which port, use the port name
   'dryrun-XXX' (e.g., 'dryrun-chromium-mac-leopard'). If you use just
   'dryrun', it uses the default port.

   Note that because this is really acting as a wrapper around the underlying
   port, you must be able to run the underlying port as well
   (check_build() and check_sys_deps() must pass and auxiliary binaries
   like layout_test_helper and httpd must work).

   This implementation also modifies the test expectations so that all
   tests are either SKIPPED or expected to PASS."""

from __future__ import with_statement

import os
import sys

import base
import factory


class DryRunPort(object):
    """DryRun implementation of the Port interface."""

    def __init__(self, port_name=None, options=None):
        pfx = 'dryrun-'
        if port_name.startswith(pfx):
            port_name = port_name[len(pfx):]
        else:
            port_name = None
        self._options = options
        self.__delegate = factory.get(port_name, options)

    def __getattr__(self, name):
        return getattr(self.__delegate, name)

    def check_build(self, needs_http):
        return True

    def check_sys_deps(self, needs_http):
        return True

    def start_helper(self):
        pass

    def start_http_server(self):
        pass

    def start_websocket_server(self):
        pass

    def stop_helper(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def create_driver(self, image_path, options):
        return DryrunDriver(self, image_path, options, executive=None)


class DryrunDriver(base.Driver):
    """Dryrun implementation of the DumpRenderTree / Driver interface."""

    def __init__(self, port, image_path, options, executive):
        self._port = port
        self._options = options
        self._image_path = image_path
        self._executive = executive
        self._layout_tests_dir = None

    def poll(self):
        return None

    def run_test(self, uri, timeoutms, image_hash):
        test_name = self._port.uri_to_test_name(uri)
        path = os.path.join(self._port.layout_tests_dir(), test_name)
        text_output = self._port.expected_text(path)

        if image_hash is not None:
            image = self._port.expected_image(path)
            if image and self._image_path:
                with open(self._image_path, 'w') as f:
                    f.write(image)
            hash = self._port.expected_checksum(path)
        else:
            hash = None
        return (False, False, hash, text_output, None)

    def start(self):
        pass

    def stop(self):
        pass
