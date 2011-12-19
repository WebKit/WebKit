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
   'dryrun-XXX' (e.g., 'dryrun-chromium-cg-mac-leopard'). If you use just
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
import time

from webkitpy.layout_tests.port import Driver, DriverOutput, factory


# Note that we don't inherit from base.Port in order for delegation to
# work properly: except for the methods defined here, we want to ensure that
# all of the methods are passed to the __delegate, not to the base class.
class DryRunPort(object):
    """DryRun implementation of the Port interface."""

    def __init__(self, host, **kwargs):
        pfx = 'dryrun-'
        if 'port_name' in kwargs:
            if kwargs['port_name'].startswith(pfx):
                kwargs['port_name'] = kwargs['port_name'][len(pfx):]
            else:
                kwargs['port_name'] = None
        self.__delegate = factory.PortFactory(host).get(**kwargs)

    def __getattr__(self, name):
        return getattr(self.__delegate, name)

    def acquire_http_lock(self):
        pass

    def release_http_lock(self):
        pass

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

    def driver_class(self):
        return DryrunDriver


class DryrunDriver(Driver):
    """Dryrun implementation of the DumpRenderTree / Driver interface."""

    def cmd_line(self):
        return ['None']

    def run_test(self, driver_input):
        start_time = time.time()
        fs = self._port._filesystem
        if (self._port.is_reftest(driver_input.test_name) or driver_input.test_name.endswith('-expected.html')):
            text = 'test-text'
            image = 'test-image'
            checksum = 'test-checksum'
            audio = None
        elif driver_input.test_name.endswith('-expected-mismatch.html'):
            text = 'test-text-mismatch'
            image = 'test-image-mismatch'
            checksum = 'test-checksum-mismatch'
            audio = None
        else:
            text = self._port.expected_text(driver_input.test_name)
            image = self._port.expected_image(driver_input.test_name)
            checksum = self._port.expected_checksum(driver_input.test_name)
            audio = self._port.expected_audio(driver_input.test_name)
        return DriverOutput(text, image, checksum, audio, crash=False, test_time=time.time() - start_time, timeout=False, error='')

    def stop(self):
        pass
