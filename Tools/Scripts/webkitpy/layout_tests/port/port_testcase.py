# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Unit testing base class for Port implementations."""

import errno
import socket

import sys
import time
import unittest

# Handle Python < 2.6 where multiprocessing isn't available.
try:
    import multiprocessing
except ImportError:
    multiprocessing = None

from webkitpy.layout_tests.servers import http_server_base

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.systemhost_mock import MockSystemHost


class PortTestCase(unittest.TestCase):
    """Tests that all Port implementations must pass."""
    HTTP_PORTS = (8000, 8080, 8443)
    WEBSOCKET_PORTS = (8880,)

    # Subclasses override this to point to their Port subclass.
    os_name = None
    os_version = None
    port_maker = None

    def make_port(self, host=None, options=None, os_name=None, os_version=None, **kwargs):
        host = host or MockSystemHost(os_name=(os_name or self.os_name), os_version=(os_version or self.os_version))
        options = options or MockOptions(configuration='Release')
        return self.port_maker(host, options=options, **kwargs)

    def test_default_worker_model(self):
        port = self.make_port()
        if multiprocessing:
            self.assertEqual(port.default_worker_model(), 'processes')
        else:
            self.assertEqual(port.default_worker_model(), 'inline')

    def test_driver_cmd_line(self):
        port = self.make_port()
        self.assertTrue(len(port.driver_cmd_line()))

        options = MockOptions(additional_drt_flag=['--foo=bar', '--foo=baz'])
        port = self.make_port(options=options)
        cmd_line = port.driver_cmd_line()
        self.assertTrue('--foo=bar' in cmd_line)
        self.assertTrue('--foo=baz' in cmd_line)

    def test_uses_apache(self):
        self.assertTrue(self.make_port()._uses_apache())

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

    def integration_test_http_lock(self):
        port = self.make_port()
        # Only checking that no exception is raised.
        port.acquire_http_lock()
        port.release_http_lock()

    def integration_test_check_sys_deps(self):
        port = self.make_port()
        # Only checking that no exception is raised.
        port.check_sys_deps(True)

    def integration_test_helper(self):
        port = self.make_port()
        # Only checking that no exception is raised.
        port.start_helper()
        port.stop_helper()

    def integration_test_http_server__normal(self):
        port = self.make_port()
        self.assert_servers_are_down('localhost', self.HTTP_PORTS)
        port.start_http_server()
        self.assert_servers_are_up('localhost', self.HTTP_PORTS)
        port.stop_http_server()
        self.assert_servers_are_down('localhost', self.HTTP_PORTS)

    def integration_test_http_server__fails(self):
        port = self.make_port()
        # Test that if a port isn't available, the call fails.
        for port_number in self.HTTP_PORTS:
            test_socket = socket.socket()
            try:
                try:
                    test_socket.bind(('localhost', port_number))
                except socket.error, e:
                    if e.errno in (errno.EADDRINUSE, errno.EALREADY):
                        self.fail('could not bind to port %d' % port_number)
                    raise
                try:
                    port.start_http_server()
                    self.fail('should not have been able to start the server while bound to %d' % port_number)
                except http_server_base.ServerError, e:
                    pass
            finally:
                port.stop_http_server()
                test_socket.close()

        # Test that calling start() twice fails.
        try:
            port.start_http_server()
            self.assertRaises(AssertionError, port.start_http_server)
        finally:
            port.stop_http_server()

    def integration_test_http_server__two_servers(self):
        # Test that calling start() on two different ports causes the
        # first port to be treated as stale and killed.
        port = self.make_port()
        # Test that if a port isn't available, the call fails.
        port.start_http_server()
        new_port = self.make_port()
        try:
            new_port.start_http_server()

            # Check that the first server was killed.
            self.assertFalse(port._executive.check_running_pid(port._http_server._pid))

            # Check that there is something running.
            self.assert_servers_are_up('localhost', self.HTTP_PORTS)

            # Test that calling stop() on a killed server is harmless.
            port.stop_http_server()
        finally:
            port.stop_http_server()
            new_port.stop_http_server()

            # Test that calling stop() twice is harmless.
            new_port.stop_http_server()

    def integration_test_image_diff(self):
        port = self.make_port()
        # FIXME: This test will never run since we are using a MockFilesystem for these tests!?!?
        if not port.check_image_diff():
            # The port hasn't been built - don't run the tests.
            return

        dir = port.layout_tests_dir()
        file1 = port._filesystem.join(dir, 'fast', 'css', 'button_center.png')
        contents1 = port._filesystem.read_binary_file(file1)
        file2 = port._filesystem.join(dir, 'fast', 'css',
                                      'remove-shorthand-expected.png')
        contents2 = port._filesystem.read_binary_file(file2)
        tmpfd, tmpfile = port._filesystem.open_binary_tempfile('')
        tmpfd.close()

        self.assertFalse(port.diff_image(contents1, contents1)[0])
        self.assertTrue(port.diff_image(contents1, contents2)[0])

        self.assertTrue(port.diff_image(contents1, contents2, tmpfile)[0])

        port._filesystem.remove(tmpfile)

    def test_diff_image__missing_both(self):
        port = self.make_port()
        self.assertFalse(port.diff_image(None, None)[0])
        self.assertFalse(port.diff_image(None, '')[0])
        self.assertFalse(port.diff_image('', None)[0])
        self.assertFalse(port.diff_image('', '')[0])

    def test_diff_image__missing_actual(self):
        port = self.make_port()
        self.assertTrue(port.diff_image(None, 'foo')[0])
        self.assertTrue(port.diff_image('', 'foo')[0])

    def test_diff_image__missing_expected(self):
        port = self.make_port()
        self.assertTrue(port.diff_image('foo', None)[0])
        self.assertTrue(port.diff_image('foo', '')[0])

    def test_check_build(self):
        port = self.make_port()
        port.check_build(needs_http=True)

    def test_check_wdiff(self):
        port = self.make_port()
        port.check_wdiff()

    def integration_test_websocket_server__normal(self):
        port = self.make_port()
        self.assert_servers_are_down('localhost', self.WEBSOCKET_PORTS)
        port.start_websocket_server()
        self.assert_servers_are_up('localhost', self.WEBSOCKET_PORTS)
        port.stop_websocket_server()
        self.assert_servers_are_down('localhost', self.WEBSOCKET_PORTS)

    def integration_test_websocket_server__fails(self):
        port = self.make_port()

        # Test that start() fails if a port isn't available.
        for port_number in self.WEBSOCKET_PORTS:
            test_socket = socket.socket()
            try:
                test_socket.bind(('localhost', port_number))
                try:
                    port.start_websocket_server()
                    self.fail('should not have been able to start the server while bound to %d' % port_number)
                except http_server_base.ServerError, e:
                    pass
            finally:
                port.stop_websocket_server()
                test_socket.close()

        # Test that calling start() twice fails.
        try:
            port.start_websocket_server()
            self.assertRaises(AssertionError, port.start_websocket_server)
        finally:
            port.stop_websocket_server()

    def integration_test_websocket_server__two_servers(self):
        port = self.make_port()

        # Test that calling start() on two different ports causes the
        # first port to be treated as stale and killed.
        port.start_websocket_server()
        new_port = self.make_port()
        try:
            new_port.start_websocket_server()

            # Check that the first server was killed.
            self.assertFalse(port._executive.check_running_pid(port._websocket_server._pid))

            # Check that there is something running.
            self.assert_servers_are_up('localhost', self.WEBSOCKET_PORTS)

            # Test that calling stop() on a killed server is harmless.
            port.stop_websocket_server()
        finally:
            port.stop_websocket_server()
            new_port.stop_websocket_server()

            # Test that calling stop() twice is harmless.
            new_port.stop_websocket_server()

    def test_test_configuration(self):
        port = self.make_port()
        self.assertTrue(port.test_configuration())

    def test_all_test_configurations(self):
        port = self.make_port()
        self.assertTrue(len(port.all_test_configurations()) > 0)
        self.assertTrue(port.test_configuration() in port.all_test_configurations(), "%s not in %s" % (port.test_configuration(), port.all_test_configurations()))

    def integration_test_http_server__loop(self):
        port = self.make_port()

        i = 0
        while i < 10:
            self.assert_servers_are_down('localhost', self.HTTP_PORTS)
            port.start_http_server()

            # We sleep in between alternating runs to ensure that this
            # test handles both back-to-back starts and stops and
            # starts and stops separated by a delay.
            if i % 2:
                time.sleep(0.1)

            self.assert_servers_are_up('localhost', self.HTTP_PORTS)
            port.stop_http_server()
            if i % 2:
                time.sleep(0.1)

            i += 1

# FIXME: This class and main() should be merged into test-webkitpy.
class EnhancedTestLoader(unittest.TestLoader):
    integration_tests = False
    unit_tests = True

    def getTestCaseNames(self, testCaseClass):
        def isTestMethod(attrname, testCaseClass=testCaseClass):
            if not hasattr(getattr(testCaseClass, attrname), '__call__'):
                return False
            return ((self.unit_tests and attrname.startswith('test_')) or
                    (self.integration_tests and attrname.startswith('integration_test_')))
        testFnNames = filter(isTestMethod, dir(testCaseClass))
        testFnNames.sort()
        return testFnNames


def main(argv=None):
    if argv is None:
        argv = sys.argv

    test_loader = EnhancedTestLoader()
    if '-i' in argv:
        test_loader.integration_tests = True
        argv.remove('-i')
    if '--no-unit-tests' in argv:
        test_loader.unit_tests = False
        argv.remove('--no-unit-tests')

    unittest.main(argv=argv, testLoader=test_loader)
