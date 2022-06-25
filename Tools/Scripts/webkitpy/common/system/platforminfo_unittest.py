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

import platform
import sys
import unittest

from webkitpy.common.system.executive import Executive
from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2
from webkitpy.common.system.platforminfo import PlatformInfo


def fake_sys(platform_str='darwin'):

    class FakeSysModule(object):
        platform = platform_str

    return FakeSysModule()


def fake_platform(mac_version_string='10.6.3', release_string='bar', win_version_string=''):

    class FakePlatformModule(object):
        def mac_ver(self):
            return tuple([mac_version_string, tuple(['', '', '']), 'i386'])

        def platform(self):
            return 'foo'

        def release(self):
            return release_string

        def win32_ver(self):
            return tuple(['', win_version_string, ''])

    return FakePlatformModule()


def fake_executive(output=None):
    if output:
        return MockExecutive2(output=output)
    return MockExecutive2(output='10.15.0\n')


class TestPlatformInfo(unittest.TestCase):
    def make_info(self, sys_module=None, platform_module=None, executive=None):
        return PlatformInfo(sys_module or fake_sys(), platform_module or fake_platform(), executive or fake_executive())

    # FIXME: This should be called integration_test_real_code(), but integration tests aren't
    # yet run by default and there's no reason not to run this everywhere by default.
    def test_real_code(self):
        # This test makes sure the real (unmocked) code actually works.
        info = PlatformInfo(executive=Executive())
        self.assertNotEqual(info.os_name, '')
        if info.is_mac() or info.is_win():
            self.assertIsNotNone(info.os_version)
        self.assertNotEqual(info.display_name(), '')
        self.assertTrue(info.is_mac() or info.is_win() or info.is_linux() or info.is_freebsd())
        self.assertIsNotNone(info.terminal_width())

        if info.is_mac():
            self.assertTrue(info.total_bytes_memory() > 0)
        else:
            self.assertIsNone(info.total_bytes_memory())

    def test_os_name_and_wrappers(self):
        info = self.make_info(fake_sys('linux2'), fake_platform('', '10.4'))
        self.assertTrue(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertFalse(info.is_win())
        self.assertFalse(info.is_freebsd())

        info = self.make_info(fake_sys('linux3'), fake_platform('', '10.4'))
        self.assertTrue(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertFalse(info.is_win())
        self.assertFalse(info.is_freebsd())

        info = self.make_info(fake_sys('darwin'), fake_platform('10.6.3'))
        self.assertEqual(info.os_name, 'mac')
        self.assertFalse(info.is_linux())
        self.assertTrue(info.is_mac())
        self.assertFalse(info.is_win())
        self.assertFalse(info.is_freebsd())

        info = self.make_info(fake_sys('win32'), fake_platform(win_version_string='6.1.7600'))
        self.assertEqual(info.os_name, 'win')
        self.assertFalse(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertTrue(info.is_win())
        self.assertTrue(info.is_native_win())
        self.assertFalse(info.is_freebsd())

        info = self.make_info(fake_sys('cygwin'), executive=fake_executive('6.1.7600'))
        self.assertEqual(info.os_name, 'win')
        self.assertFalse(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertTrue(info.is_win())
        self.assertFalse(info.is_native_win())
        self.assertFalse(info.is_freebsd())

        info = self.make_info(fake_sys('freebsd8'), fake_platform('', '8.3-PRERELEASE'))
        self.assertEqual(info.os_name, 'freebsd')
        self.assertFalse(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertFalse(info.is_win())
        self.assertTrue(info.is_freebsd())

        self.assertRaises(AssertionError, self.make_info, fake_sys('vms'))

    def test_display_name(self):
        info = self.make_info(fake_sys('darwin'))
        self.assertNotEqual(info.display_name(), '')

        info = self.make_info(fake_sys('win32'), fake_platform(win_version_string='6.1.7600'))
        self.assertNotEqual(info.display_name(), '')

        info = self.make_info(fake_sys('linux2'), fake_platform('', '10.4'))
        self.assertNotEqual(info.display_name(), '')

        info = self.make_info(fake_sys('freebsd9'), fake_platform('', '9.0-RELEASE'))
        self.assertNotEqual(info.display_name(), '')

    def test_total_bytes_memory(self):
        info = self.make_info(fake_sys('darwin'), fake_platform('10.6.3'), fake_executive('1234'))
        self.assertEqual(info.total_bytes_memory(), 1234)

        info = self.make_info(fake_sys('win32'), fake_platform(win_version_string='6.1.7600'))
        self.assertIsNone(info.total_bytes_memory())

        info = self.make_info(fake_sys('linux2'), fake_platform('', '10.4'))
        self.assertIsNone(info.total_bytes_memory())

        info = self.make_info(fake_sys('freebsd9'), fake_platform('', '9.0-RELEASE'))
        self.assertIsNone(info.total_bytes_memory())

    def test_available_sdks(self):
        sdk_version_output = '10.16\n'
        info = self.make_info(fake_sys('darwin'), fake_platform('10.14.0'), fake_executive(sdk_version_output))
        info.xcode_sdk_version('macosx')

        show_sdks_output = """iOS SDKs:
    iOS 12.0                          -sdk iphoneos12.0

iOS Simulator SDKs:
    Simulator - iOS 12.0              -sdk iphonesimulator12.0
    Simulator - iOS 12.0 Internal     -sdk iphonesimulator12.0.type

macOS SDKs:
    macOS 10.14                       -sdk macosx10.14

watchOS SDKs:
    watchOS 5.0                       -sdk watchos5.0

watchOS Simulator SDKs:
    Simulator - watchOS 5.0           -sdk watchsimulator5.0
    Simulator - watchOS 5.0 Internal    -sdk watchsimulator5.0.type
"""
        info._executive = fake_executive(show_sdks_output)
        self.assertEqual(info.available_sdks(), [
            'iphoneos',
            'iphonesimulator', 'iphonesimulator.type',
            'macosx',
            'watchos',
            'watchsimulator', 'watchsimulator.type',
        ])
