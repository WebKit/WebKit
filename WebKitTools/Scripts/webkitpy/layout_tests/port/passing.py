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
   'passing-XXX' (e.g., 'passing-chromium-mac-leopard'). If you use just
   'passing', it uses the default port.

   Note that because this is really acting as a wrapper around the underlying
   port, you must be able to run the underlying port as well
   (check_build() and check_sys_deps() must pass and auxiliary binaries
   like layout_test_helper and httpd must work).

   This implementation also modifies the test expectations so that all
   tests are either SKIPPED or expected to PASS."""

import base
import factory

class PassingPort(object):
    """Passing implementation of the Port interface."""

    def __init__(self, port_name=None, options=None):
        pfx = 'passing-'
        if port_name.startswith(pfx):
            port_name = port_name[len(pfx):]
        else:
            port_name = None
        self.__delegate = factory.get(port_name, options)

    def __getattr__(self, name):
        return getattr(self.__delegate, name)

    def start_driver(self, image_path, options):
        return PassingDriver(self, image_path, options)

    def test_expectations(self):
        exps = self.__delegate.test_expectations()
        skips = []
        for line in exps.split('\n'):
            if line.find("SKIP") != -1:
                skips.append(line)
        return '\n'.join(skips)

class PassingDriver(base.Driver):
    """Passing implementation of the DumpRenderTree / Driver interface."""

    def __init__(self, port, image_path, test_driver_options):
        self._port = port
        self._driver_options = test_driver_options
        self._image_path = image_path
        self._layout_tests_dir = None

    def poll(self):
        return None

    def returncode(self):
        return 0

    def run_test(self, uri, timeoutms, image_hash):
        test_name = self._uri_to_test(uri)

        text_filename = self._port.expected_filename(test_name, '.txt')
        try:
            text_output = open(text_filename, 'r').read()
        except IOError:
            text_output = ''

        if image_hash:
            image_filename = self._port.expected_filename(test_name, '.png')
            try:
                image = file(image_filename, 'rb').read()
            except IOError:
                image = ''
            if self._image_path:
                output_file = file(self._image_path, 'w')
                output_file.write(image)
                output_file.close()
            hash_filename = self._port.expected_filename(test_name,
                '.checksum')
            try:
                hash = file(hash_filename, 'r').read()
            except IOError:
                hash = ''
        else:
            hash = None
        return (False, False, hash, text_output, None)

    def stop(self):
        pass

    def _uri_to_test(self, uri):
        if not self._layout_tests_dir:
            self._layout_tests_dir = self._port.layout_tests_dir()
        test = uri

        if uri.startswith("file:///"):
            test = test.replace('file://', '')
            return test
        elif uri.startswith("http://127.0.0.1:8880/"):
            # websocket tests
            test = test.replace('http://127.0.0.1:8880/',
                                self._layout_tests_dir + '/')
            return test
        elif uri.startswith("http://"):
            # regular HTTP test
            test = test.replace('http://127.0.0.1:8000/',
                                self._layout_tests_dir + '/http/tests/')
            return test
        elif uri.startswith("https://"):
            test = test.replace('https://127.0.0.1:8443/',
                                self._layout_tests_dir + '/http/tests/')
            return test
        else:
            raise NotImplementedError('unknown url type: %s' % uri)

