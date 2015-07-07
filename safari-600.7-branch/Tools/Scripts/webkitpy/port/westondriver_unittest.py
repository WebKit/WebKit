# Copyright (C) 2013 Igalia S.L.
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
#    * Neither the name of Igalia S.L. nor the names of its
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

import logging
import re
import unittest2 as unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.port import Port
from webkitpy.port.server_process_mock import MockServerProcess
from webkitpy.port.westondriver import WestonDriver
from webkitpy.tool.mocktool import MockOptions

_log = logging.getLogger(__name__)


class WestonDriverTest(unittest.TestCase):
    def make_driver(self, filesystem=None):
        port = Port(MockSystemHost(log_executive=True, filesystem=filesystem), 'westondrivertestport', options=MockOptions(configuration='Release'))
        port._config.build_directory = lambda configuration: "/mock_build"
        port._server_process_constructor = MockServerProcess

        driver = WestonDriver(port, worker_number=0, pixel_tests=True)
        driver._startup_delay_secs = 0
        return driver

    def test_start(self):
        driver = self.make_driver()
        output_capture = OutputCapture()

        output_capture.capture_output()
        driver.start(pixel_tests=True, per_test_args=[])
        _, _, logs = output_capture.restore_output()

        self.assertTrue(re.match(r"MOCK popen: \['weston', '--socket=WKTesting-weston-[0-9a-f]{32}', '--width=800', '--height=600'\]\n", logs), None)
        self.assertTrue(re.match(r"WKTesting-weston-[0-9a-f]{32}", driver._server_process.env['WAYLAND_DISPLAY']))
        self.assertEqual(driver._server_process.env['GDK_BACKEND'], 'wayland')
        self.assertTrue(driver._server_process.started)

        # This prevents improper cleanup of the subprocess.Popen mock object in implicitly-invoked WestonDriver.stop.
        driver._weston_process = None

    def test_stop(self):
        class FakeWestonProcess(object):
            def terminate(self):
                _log.info("MOCK FakeWestonProcess.terminate")

        filesystem = MockFileSystem(dirs=['/tmp/weston-driver-directory'])
        driver = self.make_driver(filesystem)
        driver._weston_process = FakeWestonProcess()
        driver._driver_directory = '/tmp/weston-driver-directory'

        expected_logs = "MOCK FakeWestonProcess.terminate\n"
        OutputCapture().assert_outputs(self, driver.stop, [], expected_logs=expected_logs)

        self.assertIsNone(driver._weston_process)
        self.assertFalse(driver._port._filesystem.exists(driver._driver_directory))
