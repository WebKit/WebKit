# Copyright (C) 2017 Igalia S.L. All rights reserved.
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
#    * Neither the name of the copyright holder nor the names of its
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

from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.port import Port
from webkitpy.port.server_process_mock import MockServerProcess
from webkitpy.port.xorgdriver import XorgDriver
from webkitpy.thirdparty.mock import patch
from webkitpy.tool.mocktool import MockOptions

_log = logging.getLogger(__name__)


class XorgDriverTest(unittest.TestCase):
    def make_driver(self, worker_number=0, xorg_running=False, executive=None):
        port = Port(MockSystemHost(log_executive=True, executive=executive), 'xorgdrivertestport', options=MockOptions(configuration='Release'))
        port._config.build_directory = lambda configuration: '/mock-build'
        port._server_process_constructor = MockServerProcess
        driver = XorgDriver(port, worker_number=worker_number, pixel_tests=True)
        driver._startup_delay_secs = 0
        driver._environment = port.setup_environ_for_server(port.driver_name())
        return driver

    def make_environment(self):
        environment_user = {'DISPLAY': ':0.0',
                           'XAUTHORITY': '/home/igalia/.Xauthority',
                           'WAYLAND_DISPLAY': 'wayland-0',
                           'WAYLAND_SOCKET': 'wayland-0'}
        return environment_user

    def test_checkdriver(self):
        driver = self.make_driver()
        with patch('os.environ', {}):
            self.assertFalse(driver.check_driver(driver._port))
        with patch('os.environ', {'XAUTHORITY': '/home/igalia/.Xauthority'}):
            self.assertFalse(driver.check_driver(driver._port))
        with patch('os.environ', {'DISPLAY': ':0.0'}):
            self.assertTrue(driver.check_driver(driver._port))
        with patch('os.environ', self.make_environment()):
            self.assertTrue(driver.check_driver(driver._port))

    def test_environment_needed_variables(self):
        driver = self.make_driver()
        environment_user = self.make_environment()
        with patch('os.environ', environment_user):
            driver_environment = driver._setup_environ_for_test()
            self.assertIn('DISPLAY', driver_environment)
            self.assertIn('XAUTHORITY', driver_environment)
            self.assertIn('GDK_BACKEND', driver_environment)
            self.assertEqual(driver_environment['DISPLAY'], environment_user['DISPLAY'])
            self.assertEqual(driver_environment['XAUTHORITY'], environment_user['XAUTHORITY'])
            self.assertEqual(driver_environment['GDK_BACKEND'], 'x11')

    def test_environment_forbidden_variables(self):
        driver = self.make_driver()
        environment_user = self.make_environment()
        with patch('os.environ', environment_user):
            driver_environment = driver._setup_environ_for_test()
            self.assertNotIn('WAYLAND_SOCKET', driver_environment)
            self.assertNotIn('WAYLAND_DISPLAY', driver_environment)

    def test_environment_optional_variables(self):
        driver = self.make_driver()
        environment_user = self.make_environment()
        with patch('os.environ', environment_user):
            driver_environment = driver._setup_environ_for_test()
            self.assertIn('XAUTHORITY', driver_environment)
            self.assertIn('DISPLAY', driver_environment)
            self.assertEqual(driver_environment['DISPLAY'], environment_user['DISPLAY'])
            self.assertEqual(driver_environment['XAUTHORITY'], environment_user['XAUTHORITY'])

        environment_user.pop('XAUTHORITY')
        with patch('os.environ', environment_user):
            driver_environment = driver._setup_environ_for_test()
            self.assertNotIn('XAUTHORITY', driver_environment)
            self.assertIn('DISPLAY', driver_environment)
            self.assertEqual(driver_environment['DISPLAY'], environment_user['DISPLAY'])
