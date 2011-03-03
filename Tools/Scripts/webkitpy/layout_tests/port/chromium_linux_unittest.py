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

import unittest

from webkitpy.common.system import executive_mock
from webkitpy.common.system import filesystem_mock

from webkitpy.layout_tests.port import chromium_linux
from webkitpy.layout_tests.port import port_testcase


class ChromiumLinuxPortTest(port_testcase.PortTestCase):
    def port_maker(self, platform):
        if platform != 'linux':
            return None
        return chromium_linux.ChromiumLinuxPort

    def assert_architecture(self, port_name=None, file_output=None,
                            expected_architecture=None):
        filesystem = filesystem_mock.MockFileSystem()
        filesystem.exists = lambda x: 'DumpRenderTree' in x
        executive = None
        if file_output:
            executive = executive_mock.MockExecutive2(file_output)

        port = chromium_linux.ChromiumLinuxPort(port_name=port_name,
            executive=executive, filesystem=filesystem)
        self.assertEquals(port.architecture(), expected_architecture)
        if expected_architecture == 'x86':
            self.assertTrue(port.baseline_path().endswith('chromium-linux'))
            self.assertTrue(port.baseline_search_path()[0].endswith('chromium-linux'))
        else:
            self.assertTrue(port.baseline_path().endswith('chromium-linux-x86_64'))
            self.assertTrue(port.baseline_search_path()[0].endswith('chromium-linux-x86_64'))
            self.assertTrue(port.baseline_search_path()[1].endswith('chromium-linux'))

    def test_architectures(self):
        self.assert_architecture(port_name='chromium-linux-x86',
                                 expected_architecture='x86')
        self.assert_architecture(port_name='chromium-linux-x86_64',
                                 expected_architecture='x86_64')
        self.assert_architecture(file_output='ELF 32-bit LSB executable',
                                 expected_architecture='x86')
        self.assert_architecture(file_output='ELF 64-bit LSB executable',
                                 expected_architecture='x86_64')

    def test_check_illegal_port_names(self):
        # FIXME: Check that, for now, these are illegal port names.
        # Eventually we should be able to do the right thing here.
        self.assertRaises(AssertionError, chromium_linux.ChromiumLinuxPort,
                          port_name='chromium-x86-linux')
        self.assertRaises(AssertionError, chromium_linux.ChromiumLinuxPort,
                          port_name='chromium-linux-x86-gpu')

    def test_determine_architecture_fails(self):
        # Test that we default to 'x86' if the driver doesn't exist.
        filesystem = filesystem_mock.MockFileSystem()
        port = chromium_linux.ChromiumLinuxPort(filesystem=filesystem)
        self.assertEquals(port.architecture(), 'x86')

        # Test that we default to 'x86' on an unknown architecture.
        filesystem = filesystem_mock.MockFileSystem()
        filesystem.exists = lambda x: True
        executive = executive_mock.MockExecutive2('win32')
        port = chromium_linux.ChromiumLinuxPort(filesystem=filesystem,
                                                executive=executive)
        self.assertEquals(port.architecture(), 'x86')

        # Test that we raise errors if something weird happens.
        filesystem = filesystem_mock.MockFileSystem()
        filesystem.exists = lambda x: True
        executive = executive_mock.MockExecutive2(exception=AssertionError)
        self.assertRaises(AssertionError, chromium_linux.ChromiumLinuxPort,
                          filesystem=filesystem, executive=executive)


if __name__ == '__main__':
    unittest.main()
