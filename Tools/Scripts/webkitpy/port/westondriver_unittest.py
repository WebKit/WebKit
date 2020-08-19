# Copyright (C) 2013 Igalia S.L.
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
import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.port import Port
from webkitpy.port.server_process_mock import MockServerProcess
from webkitpy.port.westondriver import WestonDriver
from webkitpy.tool.mocktool import MockOptions

from webkitcorepy import OutputCapture

_log = logging.getLogger(__name__)


class WestonXvfbDriverDisplayTest():
    def __init__(self, expected_xvfbdisplay):
        self._expected_xvfbdisplay = expected_xvfbdisplay

    def _xvfb_run(self, environment):
        return self._expected_xvfbdisplay


class WestonDriverTest(unittest.TestCase):
    def make_driver(self):
        port = Port(MockSystemHost(log_executive=True), 'westondrivertestport', options=MockOptions(configuration='Release'))
        port._config.build_directory = lambda configuration: "/mock_build"
        port._test_runner_process_constructor = MockServerProcess

        driver = WestonDriver(port, worker_number=0, pixel_tests=True)
        driver._startup_delay_secs = 0
        driver._expected_xvfbdisplay = 23
        driver._xvfbdriver = WestonXvfbDriverDisplayTest(driver._expected_xvfbdisplay)
        driver._environment = port.setup_environ_for_server(port.driver_name())
        return driver

    def test_start(self):
        driver = self.make_driver()
        with OutputCapture(level=logging.INFO) as captured:
            driver.start(pixel_tests=True, per_test_args=[])

        self.assertTrue(
            re.match(
                r"MOCK popen: \['weston', '--socket=WKTesting-weston-[0-9a-f]{32}', '--width=1024', '--height=768', '--use-pixman'\], env=.*\n",
                captured.root.log.getvalue(),
            ),
            None,
        )
        self.assertTrue(re.match(r"WKTesting-weston-[0-9a-f]{32}", driver._server_process.env['WAYLAND_DISPLAY']))
        self.assertFalse('DISPLAY' in driver._server_process.env)
        self.assertTrue("'DISPLAY': ':{}'".format(driver._expected_xvfbdisplay) in captured.root.log.getvalue())
        self.assertEqual(driver._server_process.env['GDK_BACKEND'], 'wayland')
        self.assertTrue(driver._server_process.started)

        # This prevents improper cleanup of the subprocess.Popen mock object in implicitly-invoked WestonDriver.stop.
        driver._weston_process = None

    def test_stop(self):
        class FakeWestonProcess(object):
            def terminate(self):
                _log.info("MOCK FakeWestonProcess.terminate")

        driver = self.make_driver()
        driver._weston_process = FakeWestonProcess()

        with OutputCapture(level=logging.INFO) as captured:
            driver.stop()
        self.assertEqual(captured.root.log.getvalue(), 'MOCK FakeWestonProcess.terminate\n')

        self.assertIsNone(driver._weston_process)
