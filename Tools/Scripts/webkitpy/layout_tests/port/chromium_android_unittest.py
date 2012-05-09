# Copyright (C) 2012 Google Inc. All rights reserved.
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

import StringIO
import unittest

from webkitpy.common.system import executive_mock
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.thirdparty.mock import Mock

from webkitpy.layout_tests.port import chromium_android
from webkitpy.layout_tests.port import port_testcase
from webkitpy.layout_tests.port import Port


class ChromiumAndroidPortTest(port_testcase.PortTestCase):
    port_name = 'chromium-android'
    port_maker = chromium_android.ChromiumAndroidPort

    def test_attributes(self):
        port = self.make_port()
        self.assertTrue(port.get_option('enable_hardware_gpu'))
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-android'))

    def test_get_last_stacktrace(self):
        port = self.make_port()

        def mock_run_command_no_dir(args):
            return '/data/tombstones: No such file or directory'
        port._executive = MockExecutive2(run_command_fn=mock_run_command_no_dir)
        self.assertEquals(port.get_last_stacktrace(), '')

        def mock_run_command_no_file(args):
            return ''
        port._executive = MockExecutive2(run_command_fn=mock_run_command_no_file)
        self.assertEquals(port.get_last_stacktrace(), '')

        def mock_run_command_non_empty(args):
            if args[2] == 'ls':
                # For 'adb shell ls -n /data/tombstones'
                return '''-rw------- 1000     1000       218643 2012-04-26 18:15 tombstone_00
-rw------- 1000     1000       241695 2012-04-26 18:15 tombstone_01
-rw------- 1000     1000       219472 2012-04-26 18:16 tombstone_02
-rw------- 1000     1000        45316 2012-04-27 16:33 tombstone_03
-rw------- 1000     1000        82022 2012-04-23 16:57 tombstone_04
-rw------- 1000     1000        82015 2012-04-23 16:57 tombstone_05
-rw------- 1000     1000        81974 2012-04-23 16:58 tombstone_06
-rw------- 1000     1000       237409 2012-04-26 17:41 tombstone_07
-rw------- 1000     1000       276089 2012-04-26 18:15 tombstone_08
-rw------- 1000     1000       219618 2012-04-26 18:15 tombstone_09
'''
            else:
                # For 'adb shell cat {tombstone}'
                return args[3]
        port._executive = MockExecutive2(run_command_fn=mock_run_command_non_empty)
        self.assertEquals(port.get_last_stacktrace(), '/data/tombstones/tombstone_03')


class ChromiumAndroidDriverTest(unittest.TestCase):
    def setUp(self):
        mock_port = Port(MockSystemHost())
        self.driver = chromium_android.ChromiumAndroidDriver(mock_port, worker_number=0, pixel_tests=True)

    def test_get_drt_return_value(self):
        self.assertEquals(self.driver._get_drt_return_value('#DRT_RETURN 0'), 0)
        self.assertEquals(self.driver._get_drt_return_value(''), None)

    def test_has_crash_hint(self):
        self.assertTrue(self.driver._has_crash_hint('[1] + Stopped (signal)'))
        self.assertFalse(self.driver._has_crash_hint(''))

    def test_read_prompt(self):
        self.driver._proc = Mock()  # FIXME: This should use a tighter mock.
        self.driver._proc.stdout = StringIO.StringIO("root@android:/ # ")
        self.assertEquals(self.driver._read_prompt(), None)
        self.driver._proc.stdout = StringIO.StringIO("$ ")
        self.assertRaises(AssertionError, self.driver._read_prompt)

    def test_test_shell_command(self):
        uri = 'file://%s/test.html' % self.driver._port.layout_tests_dir()
        self.assertEquals(uri, 'file:///mock-checkout/LayoutTests/test.html')
        expected_command = 'file:///data/local/tmp/third_party/WebKit/LayoutTests/test.html 2 checksum\n'
        self.assertEquals(self.driver._test_shell_command(uri, 2, 'checksum'), expected_command)
        self.assertEquals(self.driver._test_shell_command('http://test.html', 2, 'checksum'), 'http://test.html 2 checksum\n')

    def test_write_command_and_read_line(self):
        self.driver._proc = Mock()  # FIXME: This should use a tighter mock.
        self.driver._proc.stdout = StringIO.StringIO("#URL:file:///data/local/tmp/third_party/WebKit/LayoutTests/test.html\noutput\n[1] + Stopped (signal)\n")
        self.assertEquals(self.driver._write_command_and_read_line(), ('#URL:file:///mock-checkout/LayoutTests/test.html\n', False))
        self.assertEquals(self.driver._write_command_and_read_line(), ('output\n', False))
        self.assertEquals(self.driver._write_command_and_read_line(), ('[1] + Stopped (signal)\n', True))


if __name__ == '__main__':
    unittest.main()
