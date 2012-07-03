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

import optparse
import StringIO
import unittest

from webkitpy.common.system import executive_mock
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.thirdparty.mock import Mock

from webkitpy.layout_tests.port import chromium_android
from webkitpy.layout_tests.port import chromium_port_testcase
from webkitpy.layout_tests.port import Port


class ChromiumAndroidPortTest(chromium_port_testcase.ChromiumPortTestCase):
    port_name = 'chromium-android'
    port_maker = chromium_android.ChromiumAndroidPort
    mock_logcat = ''

    def test_attributes(self):
        port = self.make_port()
        self.assertTrue(port.get_option('enable_hardware_gpu'))
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-android'))

    def test_default_timeout_ms(self):
        self.assertEquals(self.make_port(options=optparse.Values({'configuration': 'Release'})).default_timeout_ms(), 10000)
        self.assertEquals(self.make_port(options=optparse.Values({'configuration': 'Debug'})).default_timeout_ms(), 10000)

    def test_expectations_files(self):
        # FIXME: override this test temporarily while we're still upstreaming the android port and
        # using a custom expectations file.
        pass

    @staticmethod
    def mock_run_command_fn(args):
        if args[1] == 'shell':
            if args[2:] == ['ls', '-n', '/data/tombstones']:
                # For 'adb shell ls -n /data/tombstones'
                return ('-rw------- 1000     1000       218643 2012-04-26 18:15 tombstone_00\n'
                        '-rw------- 1000     1000       241695 2012-04-26 18:15 tombstone_01\n'
                        '-rw------- 1000     1000       219472 2012-04-26 18:16 tombstone_02\n'
                        '-rw------- 1000     1000        45316 2012-04-27 16:33 tombstone_03\n'
                        '-rw------- 1000     1000        82022 2012-04-23 16:57 tombstone_04\n'
                        '-rw------- 1000     1000        82015 2012-04-23 16:57 tombstone_05\n'
                        '-rw------- 1000     1000        81974 2012-04-23 16:58 tombstone_06\n'
                        '-rw------- 1000     1000       237409 2012-04-26 17:41 tombstone_07\n'
                        '-rw------- 1000     1000       276089 2012-04-26 18:15 tombstone_08\n'
                        '-rw------- 1000     1000       219618 2012-04-26 18:15 tombstone_09\n')
            elif args[2] == 'cat':
                return args[3] + '\nmock_contents\n'
        elif args[1] == 'logcat':
            return ChromiumAndroidPortTest.mock_logcat
        else:
            return ''

    def test_get_last_stacktrace(self):
        port = self.make_port()

        def mock_run_command_no_dir(args):
            return '/data/tombstones: No such file or directory'
        port._executive = MockExecutive2(run_command_fn=mock_run_command_no_dir)
        self.assertEquals(port._get_last_stacktrace(), '')

        def mock_run_command_no_file(args):
            return ''
        port._executive = MockExecutive2(run_command_fn=mock_run_command_no_file)
        self.assertEquals(port._get_last_stacktrace(), '')

        port._executive = MockExecutive2(run_command_fn=ChromiumAndroidPortTest.mock_run_command_fn)
        self.assertEquals(port._get_last_stacktrace(),
                          '-rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
                          '/data/tombstones/tombstone_03\nmock_contents\n')

    def test_get_crash_log(self):
        port = self.make_port()
        port._executive = MockExecutive2(run_command_fn=ChromiumAndroidPortTest.mock_run_command_fn)
        ChromiumAndroidPortTest.mock_logcat = 'logcat contents\n'
        self.assertEquals(port._get_crash_log('foo', 1234, 'out bar\nout baz\n', 'err bar\nerr baz\n', newer_than=None),
            (u'crash log for foo (pid 1234):\n'
             u'STDOUT: out bar\n'
             u'STDOUT: out baz\n'
             u'STDOUT: ********* Logcat:\n'
             u'STDOUT: logcat contents\n'
             u'STDERR: err bar\n'
             u'STDERR: err baz\n'
             u'STDERR: ********* Tombstone file:\n'
             u'STDERR: -rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             u'STDERR: /data/tombstones/tombstone_03\n'
             u'STDERR: mock_contents\n'))
        self.assertEquals(port._get_crash_log(None, None, None, None, newer_than=None),
            (u'crash log for <unknown process name> (pid <unknown>):\n'
             u'STDOUT: ********* Logcat:\n'
             u'STDOUT: logcat contents\n'
             u'STDERR: ********* Tombstone file:\n'
             u'STDERR: -rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             u'STDERR: /data/tombstones/tombstone_03\n'
             u'STDERR: mock_contents\n'))


class ChromiumAndroidDriverTest(unittest.TestCase):
    def setUp(self):
        mock_port = chromium_android.ChromiumAndroidPort(MockSystemHost(), 'chromium-android')
        self.driver = chromium_android.ChromiumAndroidDriver(mock_port, worker_number=0, pixel_tests=True)

    def test_cmd_line(self):
        cmd_line = self.driver.cmd_line(True, ['--a'])
        self.assertTrue('--a' in cmd_line)
        self.assertTrue('--in-fifo=' + chromium_android.DRT_APP_FILE_DIR + 'DumpRenderTree.in' in cmd_line)
        self.assertTrue('--out-fifo=' + chromium_android.DRT_APP_FILE_DIR + 'DumpRenderTree.out' in cmd_line)
        self.assertTrue('--err-file=' + chromium_android.DRT_APP_FILE_DIR + 'DumpRenderTree.err' in cmd_line)

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
        self.driver._proc.stdout = StringIO.StringIO("#URL:file:///data/local/tmp/third_party/WebKit/LayoutTests/test.html\noutput\n\n")
        self.assertEquals(self.driver._write_command_and_read_line(), ('#URL:file:///mock-checkout/LayoutTests/test.html\n', False))
        self.assertEquals(self.driver._write_command_and_read_line(), ('output\n', False))
        self.assertEquals(self.driver._write_command_and_read_line(), ('\n', False))
        # Unexpected EOF is treated as crash.
        self.assertEquals(self.driver._write_command_and_read_line(), ('', True))


if __name__ == '__main__':
    unittest.main()
