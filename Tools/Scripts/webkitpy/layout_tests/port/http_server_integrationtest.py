# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

"""Integration testing for the new-run-webkit-httpd script"""

import errno
import os
import socket
import subprocess
import sys
import tempfile
import unittest

from webkitpy.layout_tests.port import port_testcase


class NewRunWebKitHTTPdTest(unittest.TestCase):
    """Tests that new-run-webkit-httpd must pass."""
    HTTP_PORTS = (8000, 8080, 8443)

    def assert_servers_are_down(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
                self.fail()
            except IOError, e:
                self.assertTrue(e.errno in (errno.ECONNREFUSED, errno.ECONNRESET))
            finally:
                test_socket.close()

    def assert_servers_are_up(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
            except IOError, e:
                self.fail('failed to connect to %s:%d' % (host, port))
            finally:
                test_socket.close()

    def run_script(self, args):
        script_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        script_path = os.path.join(script_dir, 'new-run-webkit-httpd')
        return subprocess.call([sys.executable, script_path] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def integration_test_http_server__normal(self):
        self.assert_servers_are_down('localhost', self.HTTP_PORTS)
        self.assertEquals(self.run_script(['--server', 'start']), 0)
        self.assert_servers_are_up('localhost', self.HTTP_PORTS)
        self.assertEquals(self.run_script(['--server', 'stop']), 0)
        self.assert_servers_are_down('localhost', self.HTTP_PORTS)

    def integration_test_http_server__fails(self):
        # Test that if a port isn't available, the call fails.
        for port_number in self.HTTP_PORTS:
            test_socket = socket.socket()
            try:
                try:
                    test_socket.bind(('localhost', port_number))
                except socket.error, e:
                    if e.errno in (errno.EADDRINUSE, errno.EALREADY):
                        self.fail('could not bind to port %d: %s' % (port_number, str(e)))
                    raise
                self.assertEquals(self.run_script(['--server', 'start']), 1)
            finally:
                self.run_script(['--server', 'stop'])
                test_socket.close()

        # Test that calling start() twice fails.
        try:
            self.assertEquals(self.run_script(['--server', 'start']), 0)
            self.assertEquals(self.run_script(['--server', 'start']), 1)
        finally:
            self.run_script(['--server', 'stop'])

        # Test that calling stop() twice is harmless.
        self.assertEquals(self.run_script(['--server', 'stop']), 0)

    def maybe_make_dir(self, *comps):
        try:
            os.makedirs(os.path.join(*comps))
        except OSError, e:
            if e.errno != errno.EEXIST:
                raise

    def integration_test_http_server_port_and_root(self):
        tmpdir = tempfile.mkdtemp(prefix='webkitpytest')
        self.maybe_make_dir(tmpdir, 'http', 'tests')
        self.maybe_make_dir(tmpdir, 'fast', 'js', 'resources')
        self.maybe_make_dir(tmpdir, 'media')

        self.assert_servers_are_down('localhost', [18000])
        self.assertEquals(self.run_script(['--server', 'start', '--port=18000', '--root', tmpdir]), 0)
        self.assert_servers_are_up('localhost', [18000])
        self.assertEquals(self.run_script(['--server', 'stop']), 0)
        self.assert_servers_are_down('localhost', [18000])


if __name__ == '__main__':
    port_testcase.main()
