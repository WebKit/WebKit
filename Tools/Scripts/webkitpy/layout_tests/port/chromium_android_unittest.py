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
import sys

from webkitpy.common.system import executive_mock
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost

from webkitpy.layout_tests.port import chromium_android
from webkitpy.layout_tests.port import chromium_port_testcase
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import driver_unittest
from webkitpy.tool.mocktool import MockOptions


class MockRunCommand(object):
    def __init__(self):
        self._mock_logcat = ''
        self._mock_devices_output = ''
        self._mock_devices = []
        self._mock_ls_tombstones = ''

    def mock_run_command_fn(self, args):
        if not args[0].endswith('adb'):
            return ''
        if args[1] == 'devices':
            return self._mock_devices_output
        if args[1] == 'version':
            return 'version 1.0'

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
        self.assertEqual(port.baseline_path(), port._webkit_baseline_path('chromium-android'))

    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port(options=optparse.Values({'configuration': 'Release'})).default_timeout_ms(), 10000)
        self.assertEqual(self.make_port(options=optparse.Values({'configuration': 'Debug'})).default_timeout_ms(), 10000)

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
        self.assertEqual(self.mock_run_command._mock_devices, port._get_devices())
        self.assertEqual(1, port.default_child_processes())

    def test_get_devices_two_devices(self):
        port = self.make_port()
        self.mock_run_command.mock_two_devices()
        self.assertEqual(self.mock_run_command._mock_devices, port._get_devices())
        self.assertEqual(2, port.default_child_processes())

    def test_get_device_serial_no_device(self):
        port = self.make_port()
        self.mock_run_command.mock_no_device()
        self.assertRaises(AssertionError, port._get_device_serial, 0)

    def test_get_device_serial_one_device(self):
        port = self.make_port()
        self.mock_run_command.mock_one_device()
        self.assertEqual(self.mock_run_command._mock_devices[0], port._get_device_serial(0))
        self.assertRaises(AssertionError, port._get_device_serial, 1)

    def test_get_device_serial_two_devices(self):
        port = self.make_port()
        self.mock_run_command.mock_two_devices()
        self.assertEqual(self.mock_run_command._mock_devices[0], port._get_device_serial(0))
        self.assertEqual(self.mock_run_command._mock_devices[1], port._get_device_serial(1))
        self.assertRaises(AssertionError, port._get_device_serial, 2)

    def test_must_require_http_server(self):
        port = self.make_port()
        self.assertEqual(port.requires_http_server(), True)


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
        self.assertEqual(self.driver._get_last_stacktrace(), '')

        self.mock_run_command.mock_no_tombstone_file()
        self.assertEqual(self.driver._get_last_stacktrace(), '')

        self.mock_run_command.mock_ten_tombstones()
        self.assertEqual(self.driver._get_last_stacktrace(),
                          '-rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
                          '/data/tombstones/tombstone_03\nmock_contents\n')

    def test_get_crash_log(self):
        self.mock_run_command.mock_logcat('logcat contents\n')
        self.mock_run_command.mock_ten_tombstones()
        self.driver._crashed_process_name = 'foo'
        self.driver._crashed_pid = 1234
        self.assertEqual(self.driver._get_crash_log('out bar\nout baz\n', 'err bar\nerr baz\n', newer_than=None),
            ('err bar\n'
             'err baz\n'
             '********* [123456789ABCDEF0] Tombstone file:\n'
             '-rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             '/data/tombstones/tombstone_03\n'
             'mock_contents\n',
             u'crash log for foo (pid 1234):\n'
             u'STDOUT: out bar\n'
             u'STDOUT: out baz\n'
             u'STDOUT: ********* [123456789ABCDEF0] Logcat:\n'
             u'STDOUT: logcat contents\n'
             u'STDERR: err bar\n'
             u'STDERR: err baz\n'
             u'STDERR: ********* [123456789ABCDEF0] Tombstone file:\n'
             u'STDERR: -rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             u'STDERR: /data/tombstones/tombstone_03\n'
             u'STDERR: mock_contents\n'))

        self.driver._crashed_process_name = None
        self.driver._crashed_pid = None
        self.assertEqual(self.driver._get_crash_log(None, None, newer_than=None),
            ('********* [123456789ABCDEF0] Tombstone file:\n'
             '-rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             '/data/tombstones/tombstone_03\n'
             'mock_contents\n',
             u'crash log for <unknown process name> (pid <unknown>):\n'
             u'STDOUT: ********* [123456789ABCDEF0] Logcat:\n'
             u'STDOUT: logcat contents\n'
             u'STDERR: ********* [123456789ABCDEF0] Tombstone file:\n'
             u'STDERR: -rw------- 1000 1000 45316 2012-04-27 16:33 tombstone_03\n'
             u'STDERR: /data/tombstones/tombstone_03\n'
             u'STDERR: mock_contents\n'))

    def test_cmd_line(self):
        cmd_line = self.driver.cmd_line(True, ['anything'])
        self.assertEqual(['adb', '-s', self.mock_run_command._mock_devices[0], 'shell'], cmd_line)

    def test_drt_cmd_line(self):
        cmd_line = self.driver._drt_cmd_line(True, ['--a'])
        self.assertTrue('--a' in cmd_line)
        self.assertTrue('--create-stdin-fifo' in cmd_line)
        self.assertTrue('--separate-stderr-fifo' in cmd_line)

    def test_read_prompt(self):
        self.driver._server_process = driver_unittest.MockServerProcess(lines=['root@android:/ # '])
        self.assertEqual(self.driver._read_prompt(time.time() + 1), None)
        self.driver._server_process = driver_unittest.MockServerProcess(lines=['$ '])
        self.assertEqual(self.driver._read_prompt(time.time() + 1), None)

    def test_command_from_driver_input(self):
        driver_input = driver.DriverInput('foo/bar/test.html', 10, 'checksum', True)
        expected_command = "/data/local/tmp/third_party/WebKit/LayoutTests/foo/bar/test.html'--pixel-test'checksum\n"
        if (sys.platform != "cygwin"):
            self.assertEqual(self.driver._command_from_driver_input(driver_input), expected_command)

        driver_input = driver.DriverInput('http/tests/foo/bar/test.html', 10, 'checksum', True)
        expected_command = "http://127.0.0.1:8000/foo/bar/test.html'--pixel-test'checksum\n"
        self.assertEqual(self.driver._command_from_driver_input(driver_input), expected_command)

    def test_pid_from_android_ps_output(self):
        # FIXME: Use a larger blob of ps output.
        ps_output = """u0_a72    21630 125   947920 59364 ffffffff 400beee4 S org.chromium.native_test"""
        pid = self.driver._pid_from_android_ps_output(ps_output, "org.chromium.native_test")
        self.assertEqual(pid, 21630)


class AndroidPerfTest(unittest.TestCase):
    def test_perf_output_regexp(self):
        perf_output = """[kernel.kallsyms] with build id 5a20f6299bdb955a2f07711bb7f65cd706fe7469 not found, continuing without symbols
Failed to open /tmp/perf-14168.map, continuing without symbols
Kernel address maps (/proc/{kallsyms,modules}) were restricted.

Check /proc/sys/kernel/kptr_restrict before running 'perf record'.

As no suitable kallsyms nor vmlinux was found, kernel samples
can't be resolved.

Samples in kernel modules can't be resolved as well.

# Events: 31K cycles
#
# Overhead          Command                Shared Object
# ........  ...............  ...........................  .....................................................................................................................................................................
#
    16.18%   DumpRenderTree  perf-14168.map               [.] 0x21270ac0cf43
    12.72%   DumpRenderTree  DumpRenderTree               [.] v8::internal::JSObject::GetElementWithInterceptor(v8::internal::Object*, unsigned int)
     8.28%   DumpRenderTree  DumpRenderTree               [.] v8::internal::LoadPropertyWithInterceptorOnly(v8::internal::Arguments, v8::internal::Isolate*)
     5.60%   DumpRenderTree  DumpRenderTree               [.] WTF::AtomicString WebCore::v8StringToWebCoreString<WTF::AtomicString>(v8::Handle<v8::String>, WebCore::ExternalMode)
     4.60%   DumpRenderTree  DumpRenderTree               [.] WebCore::WeakReferenceMap<void, v8::Object>::get(void*)
     3.99%   DumpRenderTree  DumpRenderTree               [.] _ZNK3WTF7HashMapIPNS_16AtomicStringImplEPN7WebCore7ElementENS_7PtrHashIS2_EENS_10HashTraitsIS2_EENS8_IS5_EEE3getERKS2_.isra.98
     3.69%   DumpRenderTree  DumpRenderTree               [.] WebCore::DocumentV8Internal::getElementByIdCallback(v8::Arguments const&)
     3.23%   DumpRenderTree  DumpRenderTree               [.] WebCore::V8ParameterBase::prepareBase()
     2.83%   DumpRenderTree  DumpRenderTree               [.] WTF::AtomicString::add(unsigned short const*, unsigned int)
     2.73%   DumpRenderTree  DumpRenderTree               [.] WebCore::DocumentV8Internal::getElementsByTagNameCallback(v8::Arguments const&)
     2.47%   DumpRenderTree  DumpRenderTree               [.] _ZN2v86Object27GetPointerFromInternalFieldEi.constprop.439
     2.43%   DumpRenderTree  DumpRenderTree               [.] v8::internal::Isolate::SetCurrentVMState(v8::internal::StateTag)
"""
        expected_first_ten_lines = """    16.18%   DumpRenderTree  perf-14168.map               [.] 0x21270ac0cf43
    12.72%   DumpRenderTree  DumpRenderTree               [.] v8::internal::JSObject::GetElementWithInterceptor(v8::internal::Object*, unsigned int)
     8.28%   DumpRenderTree  DumpRenderTree               [.] v8::internal::LoadPropertyWithInterceptorOnly(v8::internal::Arguments, v8::internal::Isolate*)
     5.60%   DumpRenderTree  DumpRenderTree               [.] WTF::AtomicString WebCore::v8StringToWebCoreString<WTF::AtomicString>(v8::Handle<v8::String>, WebCore::ExternalMode)
     4.60%   DumpRenderTree  DumpRenderTree               [.] WebCore::WeakReferenceMap<void, v8::Object>::get(void*)
     3.99%   DumpRenderTree  DumpRenderTree               [.] _ZNK3WTF7HashMapIPNS_16AtomicStringImplEPN7WebCore7ElementENS_7PtrHashIS2_EENS_10HashTraitsIS2_EENS8_IS5_EEE3getERKS2_.isra.98
     3.69%   DumpRenderTree  DumpRenderTree               [.] WebCore::DocumentV8Internal::getElementByIdCallback(v8::Arguments const&)
     3.23%   DumpRenderTree  DumpRenderTree               [.] WebCore::V8ParameterBase::prepareBase()
     2.83%   DumpRenderTree  DumpRenderTree               [.] WTF::AtomicString::add(unsigned short const*, unsigned int)
     2.73%   DumpRenderTree  DumpRenderTree               [.] WebCore::DocumentV8Internal::getElementsByTagNameCallback(v8::Arguments const&)
"""
        host = MockSystemHost()
        profiler = chromium_android.AndroidPerf(host, '/bin/executable', '/tmp/output', 'adb-path', 'device-serial', '/tmp/symfs', '/tmp/kallsyms', 'foo')
        self.assertEqual(profiler._first_ten_lines_of_profile(perf_output), expected_first_ten_lines)


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
        self.assertEqual(['adb', '-s', mock_run_command._mock_devices[0], 'shell'], cmd_line0)

        cmd_line1 = driver1.cmd_line(True, ['anything'])
        self.assertEqual(['adb', '-s', mock_run_command._mock_devices[1], 'shell'], cmd_line1)


class ChromiumAndroidTwoPortsTest(unittest.TestCase):
    def test_options_with_two_ports(self):
        options = MockOptions(additional_drt_flag=['--foo=bar', '--foo=baz'])
        mock_run_command = MockRunCommand()
        mock_run_command.mock_two_devices()
        port0 = chromium_android.ChromiumAndroidPort(
                MockSystemHost(executive=MockExecutive2(run_command_fn=mock_run_command.mock_run_command_fn)),
                'chromium-android', options=options)
        port1 = chromium_android.ChromiumAndroidPort(
                MockSystemHost(executive=MockExecutive2(run_command_fn=mock_run_command.mock_run_command_fn)),
                'chromium-android', options=options)
        cmd_line = port1.driver_cmd_line()
        self.assertEqual(cmd_line.count('--encode-binary'), 1)
        self.assertEqual(cmd_line.count('--enable-hardware-gpu'), 1)
