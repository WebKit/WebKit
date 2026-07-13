# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Integration test for WebPlatformTestServer."""

import os
import tempfile
import unittest

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.layout_tests.servers.web_platform_test_server import WebPlatformTestServer
from webkitpy.port.factory import PortFactory


class WPTServeIntegrationTest(unittest.TestCase):
    def integration_test_serve_starts_without_pip_install(self):
        port = PortFactory(SystemHost.get_default()).get()
        wpt_dir = port.host.filesystem.join(port.layout_tests_dir(), 'imported', 'w3c', 'web-platform-tests')
        venv_path = os.path.join(wpt_dir, '_venv3')

        if os.path.exists(venv_path):
            holding_path = tempfile.mkdtemp(prefix='_venv3-integrationtest-', dir=wpt_dir)
            os.rename(venv_path, holding_path)
            self.addCleanup(os.rename, holding_path, venv_path)

        pidfile = os.path.join(tempfile.mkdtemp(prefix='wpt-serve-integrationtest-'), 'wpttest.pid')
        server = WebPlatformTestServer(port, 'wpttest_integrationtest', pidfile)
        self.addCleanup(server.stop)
        server.start()
        self.assertTrue(server._is_server_running_on_all_ports())
        self.assertFalse(os.path.exists(venv_path))
