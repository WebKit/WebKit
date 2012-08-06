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
import time
import unittest

from webkitpy.common.system import executive_mock
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost

from webkitpy.layout_tests.port import chromium_android
from webkitpy.layout_tests.port import chromium_port_testcase
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import driver_unittest


class MockRunCommand(object):
    def __init__(self):
        self._mock_logcat = ''
        self._mock_devices_output = ''
        self._mock_devices = []
        self._mock_ls_tombstones = ''

    def mock_run_command_fn(self, args):
        if args[0] != 'adb':
            return ''
        if args[1] == 'devices':
            return self._mock_devices_output

        assert len(args) > 3
        assert args[1] == '-s'
        assert args[2] in self._mock_devices
        if args[3] == 'shell':
            if args[4:] == ['ls', '-n', '/data/tombstones']:
                return self._mock_ls_tombstones
            elif args[4] == 'cat':
                return args[5] + '\nmock_contents\n'
        elif args[3] == 'logcat':
            return self._mock_logcat
        return ''

    def mock_no_device(self):
        self._mock_devices = []
        self._mock_devices_output = 'List of devices attached'

    def mock_one_device(self):
        self._mock_devices = ['123456789ABCDEF0']
        self._mock_devices_output = ('List of devices attached\n'
                                     '%s\tdevice\n' % self._mock_devices[0])

    def mock_two_devices(self):
        self._mock_devices = ['123456789ABCDEF0', '23456789ABCDEF01']
        self._mock_devices_output = ('* daemon not running. starting it now on port 5037 *'
                                     '* daemon started successfully *'
                                     'List of devices attached\n'
                                     '%s\tdevice\n'
                                     '%s\tdevice\n' % (self._mock_devices[0], self._mock_devices[1]))

    def mock_no_tombstone_dir(self):
        self._mock_ls_tombstones = '/data/tombstones: No such file or directory'

    def mock_no_tombstone_file(self):
        self._mock_ls_tombstones = ''

    def mock_ten_tombstones(self):
        self._mock_ls_tombstones = ('-rw------- 1000     1000       218643 2012-04-26 18:15 tombstone_00\n'
                                    '-rw------- 1000     1000       241695 2012-04-26 18:15 tombstone_01\n'
                                    '-rw------- 1000     1000       219472 2012-04-26 18:16 tombstone_02\n'
                                    '-rw------- 1000     1000        45316 2012-04-27 16:33 tombstone_03\n'
                                    '-rw------- 1000     1000        82022 2012-04-23 16:57 tombstone_04\n'
                                    '-rw------- 1000     1000        82015 2012-04-23 16:57 tombstone_05\n'
                                    '-rw------- 1000     1000        81974 2012-04-23 16:58 tombstone_06\n'
                                    '-rw------- 1000     1000       237409 2012-04-26 17:41 tombstone_07\n'
                                    '-rw------- 1000     1000       276089 2012-04-26 18:15 tombstone_08\n'
                                    '-rw------- 1000     1000       219618 2012-04-26 18:15 tombstone_09\n')

    def mock_logcat(self, content):
        self._mock_logcat = content


class ChromiumAndroidPortTest(chromium_port_testcase.ChromiumPortTestCase):
    port_name = 'chromium-android'
    port_maker = chromium_android.ChromiumAndroidPort

    def make_port(self, **kwargs):
        port = super(ChromiumAndroidPortTest, self).make_port(**kwargs)
        self.mock_run_command = MockRunCommand()
        self.mock_run_command.mock_one_device()
        port._executive = MockExecutive2(run_command_fn=self.mock_run_command.mock_run_command_fn)
        return port

    def test_attributes(self):
        port = self.make_port()
        self.assertEquals(port.baseline_path(), port._webkit_baseline_path('chromium-android'))

    def test_default_timeout_ms(self):
        self.assertEquals(self.make_port(options=optparse.Values({'configuration': 'Release'})).default_timeout_ms(), 10000)
        self.assertEquals(self.make_port(options=optparse.Values({'configuration': 'Debug'})).default_timeout_ms(), 10000)

    def test_expectations_files(self):
        # FIXME: override this test temporarily while we're still upstreaming the android port and
        # using a custom expectations file.
        pass

    def test_get_devices_no_device(self):
        port = self.make_port()
        self.mock_run_command.mock_no_device()
        self.assertRaises(AssertionError, port._get_devices)

    def test_get_devices_one_device(self):
        port = self.make_port()
        self.mock_run_command.mock_one_device()
        self.assertEquals(self.mock_run_command._mock_devices, port._get_devices())
        self.assertEquals(1, port.default_child_processes())

    def test_get_devices_two_devices(self):
        port = self.make_port()
        self.mock_run_command.mock_two_devices()
        self.assertEquals(self.mock_run_command._mock_devices, port._get_devices())
        self.assertEquals(2, port.default_child_processes())

    def test_get_device_serial_no_device(self):
        port = self.make_port()
        self.mock_run_command.mock_no_device()
        self.assertRaises(AssertionError, port._get_device_serial, 0)

    def test_get_device_serial_one_device(self):
        port = self.make_port()
        self.mock_run_command.mock_one_device()
        self.assertEquals(self.mock_run_command._mock_devices[0], port._get_device_serial(0))
        self.assertRaises(AssertionError, port._get_device_serial, 1)

    def test_get_device_serial_two_devices(self):
        port = self.make_port()
        self.mock_run_command.mock_two_devices()
        self.assertEquals(self.mock_run_command._mock_devices[0], port._get_device_serial(0))
        self.assertEquals(self.mock_run_command._mock_devices[1], port._get_device_serial(1))
        self.assertRaises(AssertionError, port._get_device_serial, 2)


class ChromiumAndroidDriverTest(unittest.TestCase):
    def setUp(self):
        self.mock_run_command = MockRunCommand()
        self.mock_run_command.mock_one_device()
        self.port = chromium_android.ChromiumAndroidPort(
                MockSystemHost(executive=MockExecutive2(run_command_fn=self.mock_run_command.mock_run_command_fn)),
                'chromium-android')
        self.driver = chromium_android.ChromiumAndroidDriver(self.port, worker_number=0, pixel_tests=True)

    def test_get_last_stacktrace(self):
        self.mock_run_command.mock_no_tombstone_dir()
        self.assertEquals(self.driver._get_last_stacktrace(), '')

        self.mock_run_command.mock_no_tombstone_file()
        self.assertEquals(self.driver._get_last_stacktrace(), '')

        self.mock_run_command.mock_ten_tombstones()
        self.assertEquals(self.driver._get_last_stacktrace(),
                          '-rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
                          '/data/tombstones/tombstone_03\nmock_contents\n')

    def test_get_crash_log(self):
        self.mock_run_command.mock_logcat('logcat contents\n')
        self.mock_run_command.mock_ten_tombstones()
        self.driver._crashed_process_name = 'foo'
        self.driver._crashed_pid = 1234
        self.assertEquals(self.driver._get_crash_log('out bar\nout baz\n', 'err bar\nerr baz\n', newer_than=None),
            ('err bar\n'
             'err baz\n'
             '********* Tombstone file:\n'
             '-rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             '/data/tombstones/tombstone_03\n'
             'mock_contents\n',
             u'crash log for foo (pid 1234):\n'
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

        self.driver._crashed_process_name = None
        self.driver._crashed_pid = None
        self.assertEquals(self.driver._get_crash_log(None, None, newer_than=None),
            ('********* Tombstone file:\n'
             '-rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             '/data/tombstones/tombstone_03\n'
             'mock_contents\n',
             u'crash log for <unknown process name> (pid <unknown>):\n'
             u'STDOUT: ********* Logcat:\n'
             u'STDOUT: logcat contents\n'
             u'STDERR: ********* Tombstone file:\n'
             u'STDERR: -rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             u'STDERR: /data/tombstones/tombstone_03\n'
             u'STDERR: mock_contents\n'))

    def test_cmd_line(self):
        cmd_line = self.driver.cmd_line(True, ['anything'])
        self.assertEquals(['adb', '-s', self.mock_run_command._mock_devices[0], 'shell'], cmd_line)

    def test_drt_cmd_line(self):
        cmd_line = self.driver._drt_cmd_line(True, ['--a'])
        self.assertTrue('--a' in cmd_line)
        self.assertTrue('--in-fifo=' + chromium_android.DEVICE_DRT_DIR + 'DumpRenderTree.in' in cmd_line)
        self.assertTrue('--out-fifo=' + chromium_android.DEVICE_DRT_DIR + 'DumpRenderTree.out' in cmd_line)
        self.assertTrue('--err-fifo=' + chromium_android.DEVICE_DRT_DIR + 'DumpRenderTree.err' in cmd_line)

    def test_read_prompt(self):
        self.driver._server_process = driver_unittest.MockServerProcess(lines=['root@android:/ # '])
        self.assertEquals(self.driver._read_prompt(time.time() + 1), None)
        self.driver._server_process = driver_unittest.MockServerProcess(lines=['$ '])
        self.assertRaises(AssertionError, self.driver._read_prompt, time.time() + 1)

    def test_command_from_driver_input(self):
        driver_input = driver.DriverInput('foo/bar/test.html', 10, 'checksum', True)
        expected_command = "/data/local/tmp/third_party/WebKit/LayoutTests/foo/bar/test.html'--pixel-test'checksum\n"
        self.assertEquals(self.driver._command_from_driver_input(driver_input), expected_command)

        driver_input = driver.DriverInput('http/tests/foo/bar/test.html', 10, 'checksum', True)
        expected_command = "http://127.0.0.1:8000/foo/bar/test.html'--pixel-test'checksum\n"
        self.assertEquals(self.driver._command_from_driver_input(driver_input), expected_command)


class ChromiumAndroidDriverTwoDriversTest(unittest.TestCase):
    def test_two_drivers(self):
        mock_run_command = MockRunCommand()
        mock_run_command.mock_two_devices()
        port = chromium_android.ChromiumAndroidPort(
                MockSystemHost(executive=MockExecutive2(run_command_fn=mock_run_command.mock_run_command_fn)),
                'chromium-android')
        driver0 = chromium_android.ChromiumAndroidDriver(port, worker_number=0, pixel_tests=True)
        driver1 = chromium_android.ChromiumAndroidDriver(port, worker_number=1, pixel_tests=True)

        cmd_line0 = driver0.cmd_line(True, ['anything'])
        self.assertEquals(['adb', '-s', mock_run_command._mock_devices[0], 'shell'], cmd_line0)

        cmd_line1 = driver1.cmd_line(True, ['anything'])
        self.assertEquals(['adb', '-s', mock_run_command._mock_devices[1], 'shell'], cmd_line1)


if __name__ == '__main__':
    unittest.main()
