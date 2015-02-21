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

import imp
import sys
import time
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.port import Port
from webkitpy.tool.mocktool import MockOptions

from webkitpy.layout_tests.servers.http_server_base import ServerError
from webkitpy.layout_tests.servers.web_platform_test_server import WebPlatformTestServer


class TestWebPlatformTestServer(unittest.TestCase):
    def test_previously_spawned_instance(self):
        host = MockHost()
        port = Port(host, "test")
        server = WebPlatformTestServer(port, "wpttest", "/mock/output_dir", "/mock/output_dir/pid.txt")
        server._check_that_all_ports_are_available = lambda: True
        server._is_server_running_on_all_ports = lambda: True

        host.filesystem.write_text_file("/mock_output_dir/pid.txt", "0")
        server.start()
        server.stop()

    def test_import_web_platform_test_modules(self):
        fs = FileSystem()
        current_dir, name = fs.split(fs.realpath(__file__))
        doc_root_dir = fs.join(current_dir, "..", "..", "..", "..", "..", "LayoutTests", "imported", "w3c", "web-platform-tests")
        tools_dir = fs.join(doc_root_dir, "tools")

        sys.path.insert(0, doc_root_dir)
        try:
            file, pathname, description = imp.find_module("tools")
        except ImportError, e:
            self.fail(e)
        self.assertEqual(pathname, tools_dir)
        sys.path.pop(0)

        sys.path.insert(0, tools_dir)
        try:
            file, pathname, description = imp.find_module("scripts")
        except ImportError, e:
            self.fail(e)
        self.assertEqual(pathname, fs.join(tools_dir, "scripts"))
        sys.path.pop(0)

    def test_corrupted_subserver_files(self):
        host = MockHost()
        port = Port(host, "test")
        server = WebPlatformTestServer(port, "wpttest", "/mock/output_dir", "/mock/output_dir/pid.txt")
        server._check_that_all_ports_are_available = lambda: True
        server._is_server_running_on_all_ports = lambda: True

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
