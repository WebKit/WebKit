#  Copyright (c) 2014, Canon Inc. All rights reserved.
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1.  Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#  2.  Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#  3.  Neither the name of Canon Inc. nor the names of
#      its contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#  THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
#  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#  DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
#  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import optparse
import socket
import sys
import time
import unittest
from unittest import mock

from webkitcorepy import OutputCapture

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.port import Port
from webkitpy.tool.mocktool import MockOptions

from webkitpy.layout_tests.servers.http_server_base import ServerError
from webkitpy.layout_tests.servers.web_platform_test_server import WebPlatformTestServer


class TestWebPlatformTestServer(unittest.TestCase):
    def setUp(self):
        # WebPlatformTestServer.__init__ triggers a real aioquic autoinstall (import + possible network
        # download) to provision the WebTransport-over-HTTP/3 server. Stub it out so these construction-based
        # tests stay hermetic; None means "aioquic unavailable", exercising the graceful-degrade path.
        patcher = mock.patch(
            "webkitpy.layout_tests.servers.web_platform_test_server._aioquic_autoinstall_directory",
            return_value=None)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_previously_spawned_instance(self):
        with OutputCapture():
            host = MockHost()
            options = optparse.Values()
            options.ensure_value("results_directory", "/mock/output_dir")
            port = Port(host, "test", options)
            server = WebPlatformTestServer(port, "wpttest", "/mock/output_dir/pid.txt")
            server._check_that_all_ports_are_available = lambda: True
            server._is_server_running_on_all_ports = lambda: True
            host.filesystem.write_text_file("/mock-checkout/LayoutTests/resources/testharness.js", "0")
            host.filesystem.write_text_file("/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/resources/testharness.js", "0")

            host.filesystem.write_text_file("/mock_output_dir/pid.txt", "0")
            server.start()
            server.stop()

    def test_custom_layout_tests_directory(self):
        with OutputCapture():
            host = MockHost()
            options = optparse.Values()
            options.ensure_value("layout_tests_dir", "/mock-layout-tests-directory/LayoutTests")
            options.ensure_value("results_directory", "/mock/output_dir")
            port = Port(host, "test", options)
            server = WebPlatformTestServer(port, "wpttest", "/mock/output_dir/pid.txt")
            server._check_that_all_ports_are_available = lambda: True
            server._is_server_running_on_all_ports = lambda: True
            host.filesystem.write_text_file("/mock-layout-tests-directory/LayoutTests/resources/testharness.js", "0")
            host.filesystem.write_text_file("/mock-layout-tests-directory/LayoutTests/imported/w3c/web-platform-tests/resources/testharness.js", "0")

            host.filesystem.write_text_file("/mock_output_dir/pid.txt", "0")
            server.start()
            server.stop()

    def test_corrupted_subserver_files(self):
        with OutputCapture():
            host = MockHost()
            options = optparse.Values()
            options.ensure_value("results_directory", "/mock/output_dir")
            port = Port(host, "test", options)
            server = WebPlatformTestServer(port, "wpttest", "/mock/output_dir/pid.txt")
            server._check_that_all_ports_are_available = lambda: True
            server._is_server_running_on_all_ports = lambda: True
            host.filesystem.write_text_file("/mock-checkout/LayoutTests/resources/testharness.js", "0")
            host.filesystem.write_text_file("/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/resources/testharness.js", "0")

            host.filesystem.write_text_file("/mock_output_dir/wpttest_servers.json", "0")
            server.stop()
            self.assertFalse(host.filesystem.exists("/mock/output_dir/wpttest_servers.json"))

            host.filesystem.write_text_file("/mock_output_dir/wpttest_servers.json", "[0,")
            server.start()
            self.assertFalse(host.filesystem.exists("/mock/output_dir/wpttest_servers.json"))
            server.stop()

            host.filesystem.write_text_file("/mock_output_dir/wpttest_servers.json", "[{'protocol': 'http', 'port': 80 }]")
            server.start()
            self.assertFalse(host.filesystem.exists("/mock/output_dir/wpttest_servers.json"))
            server.stop()

            host.filesystem.write_text_file("/mock_output_dir/wpttest_servers.json", "[{'protocol': 'http', 'port': 80, 'pid': {} }]")
            server.start()
            self.assertFalse(host.filesystem.exists("/mock/output_dir/wpttest_servers.json"))
            server.stop()

    def test_server_fails_to_start_throws_exception(self):
        with OutputCapture():
            host = MockHost()
            options = optparse.Values()
            options.ensure_value("results_directory", "/mock/output_dir")
            port = Port(host, "test", options)
            server = WebPlatformTestServer(port, "wpttest", "/mock/output_dir/pid.txt")
            server._check_that_all_ports_are_available = lambda: True
            server._is_server_running_on_all_ports = lambda: True
            host.filesystem.write_text_file("/mock-checkout/LayoutTests/resources/testharness.js", "0")
            host.filesystem.write_text_file("/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/resources/testharness.js", "0")

            server.start()
            server.stop()
            server._process.poll = lambda: 1
            self.assertRaises(ServerError, server.start)

    def test_is_udp_port_listening(self):
        # A free UDP port reports not-listening; a port held by a live UDP socket reports listening.
        # This mirrors how the WebTransport-over-HTTP/3 server holds its UDP port bound while running.
        held = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            try:
                held.bind(('127.0.0.1', 0))
            except (PermissionError, OSError) as e:
                self.skipTest("Environment does not permit binding a UDP socket: %s" % e)
            held_port = held.getsockname()[1]

            probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            probe.bind(('127.0.0.1', 0))
            free_port = probe.getsockname()[1]
            probe.close()

            self.assertFalse(WebPlatformTestServer._is_udp_port_listening(free_port))
            self.assertTrue(WebPlatformTestServer._is_udp_port_listening(held_port))
        finally:
            held.close()
