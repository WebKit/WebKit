# Copyright (C) 2012 Zan Dobersek <zandobersek@gmail.com>
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import logging
import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.port import Port
from webkitpy.port.server_process_mock import MockServerProcess
from webkitpy.port.xvfbdriver import XvfbDriver
from webkitpy.tool.mocktool import MockOptions

from webkitcorepy import OutputCapture

_log = logging.getLogger(__name__)


class XvfbDriverTest(unittest.TestCase):
    def make_driver(self, worker_number=0, xorg_running=False, executive=None):
        port = Port(MockSystemHost(log_executive=True, executive=executive), 'xvfbdrivertestport', options=MockOptions(configuration='Release'))
        port._config.build_directory = lambda configuration: "/mock-build"
        port._test_runner_process_constructor = MockServerProcess
        if xorg_running:
            port._executive._running_pids['Xorg'] = 108

        driver = XvfbDriver(port, worker_number=worker_number, pixel_tests=True)
        driver._startup_delay_secs = 0
        driver._xvfb_screen_depth = lambda: '24'
        driver._xvfb_pipe = lambda: (3, 4)
        driver._xvfb_read_display_id = lambda x: 1
        driver._xvfb_close_pipe = lambda p: None
        driver._port_server_environment = port.setup_environ_for_server(port.driver_name())
        return driver

    def cleanup_driver(self, driver):
        # Setting _xvfb_process member to None is necessary as the Driver object is stopped on deletion,
        # killing the Xvfb process if present. Thus, this method should only be called from tests that do not
        # intend to test the behavior of XvfbDriver.stop.
        driver._xvfb_process = None

    def assertDriverStartSuccessful(self, driver, expected_logs, expected_display, pixel_tests=False):
        with OutputCapture(level=logging.INFO) as captured:
            driver.start(pixel_tests, [])
        self.assertEqual(captured.root.log.getvalue(), expected_logs)

        self.assertTrue(driver._server_process.started)
        self.assertEqual(driver._server_process.env['DISPLAY'], expected_display)
        self.assertEqual(driver._server_process.env['GDK_BACKEND'], 'x11')

    def test_start(self):
        driver = self.make_driver()
        expected_logs = ("MOCK popen: ['Xvfb', '-displayfd', '4', '-screen', '0', '1024x768x24', '-nolisten', 'tcp'], env=%s\n" % driver._port_server_environment)
        self.assertDriverStartSuccessful(driver, expected_logs=expected_logs, expected_display=":1")
        self.cleanup_driver(driver)

    def test_start_arbitrary_worker_number(self):
        driver = self.make_driver(worker_number=17)
        expected_logs = ("MOCK popen: ['Xvfb', '-displayfd', '4', '-screen', '0', '1024x768x24', '-nolisten', 'tcp'], env=%s\n" % driver._port_server_environment)
        self.assertDriverStartSuccessful(driver, expected_logs=expected_logs, expected_display=":1", pixel_tests=True)
        self.cleanup_driver(driver)

    def test_stop(self):
        port = Port(MockSystemHost(log_executive=True), 'xvfbdrivertestport', options=MockOptions(configuration='Release'))
        port._executive.kill_process = lambda x: _log.info("MOCK kill_process pid: " + str(x))
        driver = XvfbDriver(port, worker_number=0, pixel_tests=True)

        class FakeXvfbProcess(object):
            pid = 1234

        driver._xvfb_process = FakeXvfbProcess()

        with OutputCapture(level=logging.INFO) as captured:
            driver.stop()
        self.assertEqual(captured.root.log.getvalue(), "MOCK kill_process pid: 1234\n")

        self.assertIsNone(driver._xvfb_process)
