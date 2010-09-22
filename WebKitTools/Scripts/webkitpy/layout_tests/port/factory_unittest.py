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

import sys
import unittest

import chromium_gpu
import chromium_linux
import chromium_mac
import chromium_win
import dryrun
import factory
import google_chrome
import gtk
import mac
import qt
import test
import win


class FactoryTest(unittest.TestCase):
    """Test factory creates proper port object for the target.

    Target is specified by port_name, sys.platform and options.

    """
    # FIXME: The ports themselves should expose what options they require,
    # instead of passing generic "options".

    class WebKitOptions(object):
        """Represents the minimum options for WebKit port."""
        def __init__(self):
            self.pixel_tests = False

    class ChromiumOptions(WebKitOptions):
        """Represents minimum options for Chromium port."""
        def __init__(self):
            FactoryTest.WebKitOptions.__init__(self)
            self.chromium = True

    def setUp(self):
        self.real_sys_platform = sys.platform
        self.webkit_options = FactoryTest.WebKitOptions()
        self.chromium_options = FactoryTest.ChromiumOptions()

    def tearDown(self):
        sys.platform = self.real_sys_platform

    def assert_port(self, port_name, expected_port, port_obj=None):
        """Helper assert for port_name.

        Args:
          port_name: port name to get port object.
          expected_port: class of expected port object.
          port_obj: optional port object
        """
        port_obj = port_obj or factory.get(port_name=port_name)
        self.assertTrue(isinstance(port_obj, expected_port))

    def assert_platform_port(self, platform, options, expected_port):
        """Helper assert for platform and options.

        Args:
          platform: sys.platform.
          options: options to get port object.
          expected_port: class of expected port object.

        """
        orig_platform = sys.platform
        sys.platform = platform
        self.assertTrue(isinstance(factory.get(options=options),
                                   expected_port))
        sys.platform = orig_platform

    def test_test(self):
        self.assert_port("test", test.TestPort)

    def test_dryrun(self):
        self.assert_port("dryrun-test", dryrun.DryRunPort)
        self.assert_port("dryrun-mac", dryrun.DryRunPort)

    def test_mac(self):
        self.assert_port("mac", mac.MacPort)
        self.assert_platform_port("darwin", None, mac.MacPort)
        self.assert_platform_port("darwin", self.webkit_options, mac.MacPort)

    def test_win(self):
        self.assert_port("win", win.WinPort)
        self.assert_platform_port("win32", None, win.WinPort)
        self.assert_platform_port("win32", self.webkit_options, win.WinPort)
        self.assert_platform_port("cygwin", None, win.WinPort)
        self.assert_platform_port("cygwin", self.webkit_options, win.WinPort)

    def test_google_chrome(self):
        # The actual Chrome class names aren't available so we test that the
        # objects we get are at least subclasses of the Chromium versions.
        self.assert_port("google-chrome-linux32",
                         chromium_linux.ChromiumLinuxPort)
        self.assert_port("google-chrome-linux64",
                         chromium_linux.ChromiumLinuxPort)
        self.assert_port("google-chrome-win",
                         chromium_win.ChromiumWinPort)
        self.assert_port("google-chrome-mac",
                         chromium_mac.ChromiumMacPort)

    def test_gtk(self):
        self.assert_port("gtk", gtk.GtkPort)

    def test_qt(self):
        self.assert_port("qt", qt.QtPort)

    def test_chromium_gpu_linux(self):
        self.assert_port("chromium-gpu-linux", chromium_gpu.ChromiumGpuLinuxPort)

    def test_chromium_gpu_mac(self):
        self.assert_port("chromium-gpu-mac", chromium_gpu.ChromiumGpuMacPort)

    def test_chromium_gpu_win(self):
        self.assert_port("chromium-gpu-win", chromium_gpu.ChromiumGpuWinPort)

    def test_chromium_mac(self):
        self.assert_port("chromium-mac", chromium_mac.ChromiumMacPort)
        self.assert_platform_port("darwin", self.chromium_options,
                                  chromium_mac.ChromiumMacPort)

    def test_chromium_linux(self):
        self.assert_port("chromium-linux", chromium_linux.ChromiumLinuxPort)
        self.assert_platform_port("linux2", self.chromium_options,
                                  chromium_linux.ChromiumLinuxPort)

    def test_chromium_win(self):
        self.assert_port("chromium-win", chromium_win.ChromiumWinPort)
        self.assert_platform_port("win32", self.chromium_options,
                                  chromium_win.ChromiumWinPort)
        self.assert_platform_port("cygwin", self.chromium_options,
                                  chromium_win.ChromiumWinPort)

    def test_get_all_ports(self):
        ports = factory.get_all()
        for name in factory.ALL_PORT_NAMES:
            self.assertTrue(name in ports.keys())
        self.assert_port("test", test.TestPort, ports["test"])
        self.assert_port("dryrun-test", dryrun.DryRunPort, ports["dryrun"])
        self.assert_port("dryrun-mac", dryrun.DryRunPort, ports["dryrun"])
        self.assert_port("mac", mac.MacPort, ports["mac"])
        self.assert_port("win", win.WinPort, ports["win"])
        self.assert_port("gtk", gtk.GtkPort, ports["gtk"])
        self.assert_port("qt", qt.QtPort, ports["qt"])
        self.assert_port("chromium-mac", chromium_mac.ChromiumMacPort,
                         ports["chromium-mac"])
        self.assert_port("chromium-linux", chromium_linux.ChromiumLinuxPort,
                         ports["chromium-linux"])
        self.assert_port("chromium-win", chromium_win.ChromiumWinPort,
                         ports["chromium-win"])

    def test_unknown_specified(self):
        # Test what happens when you specify an unknown port.
        orig_platform = sys.platform
        self.assertRaises(NotImplementedError, factory.get,
                          port_name='unknown')

    def test_unknown_default(self):
        # Test what happens when you're running on an unknown platform.
        orig_platform = sys.platform
        sys.platform = 'unknown'
        self.assertRaises(NotImplementedError, factory.get)
        sys.platform = orig_platform


if __name__ == '__main__':
    unittest.main()
