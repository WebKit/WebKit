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


def fake_sys(platform_str='darwin', windows_version_tuple=None):

    class FakeSysModule(object):
        platform = platform_str
        if windows_version_tuple:
            getwindowsversion = lambda x: windows_version_tuple

    return FakeSysModule()


def fake_platform(mac_version_string='10.6.3'):

    class FakePlatformModule(object):
        def mac_ver(self):
            return tuple([mac_version_string, tuple(['', '', '']), 'i386'])

        def platform(self):
            return 'foo'

    return FakePlatformModule()


def fake_executive(output=None):
    if output:
        return MockExecutive2(output=output)
    return MockExecutive2(exception=SystemError)


class TestPlatformInfo(unittest.TestCase):
    def make_info(self, sys_module=None, platform_module=None, executive=None):
        return PlatformInfo(sys_module or fake_sys(), platform_module or fake_platform(), executive or fake_executive())

    # FIXME: This should be called integration_test_real_code(), but integration tests aren't
    # yet run by default and there's no reason not to run this everywhere by default.
    def test_real_code(self):
        # This test makes sure the real (unmocked) code actually works.
        info = PlatformInfo(sys, platform, Executive())
        self.assertNotEquals(info.os_name, '')
        self.assertNotEquals(info.os_version, '')
        self.assertNotEquals(info.display_name(), '')
        self.assertTrue(info.is_mac() or info.is_win() or info.is_linux())

        if info.is_mac():
            self.assertTrue(info.total_bytes_memory() > 0)
            self.assertTrue(info.free_bytes_memory() > 0)
        else:
            self.assertEquals(info.total_bytes_memory(), None)
            self.assertEquals(info.free_bytes_memory(), None)

    def test_os_name_and_wrappers(self):
        info = self.make_info(fake_sys('linux2'))
        self.assertTrue(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertFalse(info.is_win())

        info = self.make_info(fake_sys('linux3'))
        self.assertTrue(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertFalse(info.is_win())

        info = self.make_info(fake_sys('darwin'), fake_platform('10.6.3'))
        self.assertEquals(info.os_name, 'mac')
        self.assertFalse(info.is_linux())
        self.assertTrue(info.is_mac())
        self.assertFalse(info.is_win())

        info = self.make_info(fake_sys('win32', tuple([6, 1, 7600])))
        self.assertEquals(info.os_name, 'win')
        self.assertFalse(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertTrue(info.is_win())

        info = self.make_info(fake_sys('cygwin'), executive=fake_executive('6.1.7600'))
        self.assertEquals(info.os_name, 'win')
        self.assertFalse(info.is_linux())
        self.assertFalse(info.is_mac())
        self.assertTrue(info.is_win())

        self.assertRaises(AssertionError, self.make_info, fake_sys('vms'))

    def test_os_version(self):
        self.assertRaises(AssertionError, self.make_info, fake_sys('darwin'), fake_platform('10.4.3'))
        self.assertEquals(self.make_info(fake_sys('darwin'), fake_platform('10.5.1')).os_version, 'leopard')
        self.assertEquals(self.make_info(fake_sys('darwin'), fake_platform('10.6.1')).os_version, 'snowleopard')
        self.assertEquals(self.make_info(fake_sys('darwin'), fake_platform('10.7.1')).os_version, 'lion')
        self.assertEquals(self.make_info(fake_sys('darwin'), fake_platform('10.8.0')).os_version, 'future')

        self.assertEquals(self.make_info(fake_sys('linux2')).os_version, 'lucid')

        self.assertRaises(AssertionError, self.make_info, fake_sys('win32', tuple([5, 0, 1234])))
        self.assertEquals(self.make_info(fake_sys('win32', tuple([6, 2, 1234]))).os_version, 'future')
        self.assertEquals(self.make_info(fake_sys('win32', tuple([6, 1, 7600]))).os_version, '7sp0')
        self.assertEquals(self.make_info(fake_sys('win32', tuple([6, 0, 1234]))).os_version, 'vista')
        self.assertEquals(self.make_info(fake_sys('win32', tuple([5, 1, 1234]))).os_version, 'xp')

        self.assertRaises(AssertionError, self.make_info, fake_sys('win32'), executive=fake_executive('5.0.1234'))
        self.assertEquals(self.make_info(fake_sys('cygwin'), executive=fake_executive('6.2.1234')).os_version, 'future')
        self.assertEquals(self.make_info(fake_sys('cygwin'), executive=fake_executive('6.1.7600')).os_version, '7sp0')
        self.assertEquals(self.make_info(fake_sys('cygwin'), executive=fake_executive('6.0.1234')).os_version, 'vista')
        self.assertEquals(self.make_info(fake_sys('cygwin'), executive=fake_executive('5.1.1234')).os_version, 'xp')

    def test_display_name(self):
        info = self.make_info(fake_sys('darwin'))
        self.assertNotEquals(info.display_name(), '')

        info = self.make_info(fake_sys('win32', tuple([6, 1, 7600])))
        self.assertNotEquals(info.display_name(), '')

        info = self.make_info(fake_sys('linux2'))
        self.assertNotEquals(info.display_name(), '')

    def test_total_bytes_memory(self):
        info = self.make_info(fake_sys('darwin'), fake_platform('10.6.3'), fake_executive('1234'))
        self.assertEquals(info.total_bytes_memory(), 1234)

        info = self.make_info(fake_sys('win32', tuple([6, 1, 7600])))
        self.assertEquals(info.total_bytes_memory(), None)

        info = self.make_info(fake_sys('linux2'))
        self.assertEquals(info.total_bytes_memory(), None)

    def test_free_bytes_memory(self):
        vmstat_output = ("Mach Virtual Memory Statistics: (page size of 4096 bytes)\n"
                         "Pages free:                        1.\n"
                         "Pages inactive:                    1.\n")
        info = self.make_info(fake_sys('darwin'), fake_platform('10.6.3'), fake_executive(vmstat_output))
        self.assertEquals(info.free_bytes_memory(), 8192)

        info = self.make_info(fake_sys('win32', tuple([6, 1, 7600])))
        self.assertEquals(info.free_bytes_memory(), None)

        info = self.make_info(fake_sys('linux2'))
        self.assertEquals(info.free_bytes_memory(), None)


if __name__ == '__main__':
    unittest.main()
