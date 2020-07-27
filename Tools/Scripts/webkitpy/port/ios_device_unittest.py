# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import time

from webkitcorepy import Version

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.port.config import clear_cached_configuration
from webkitpy.port.ios_device import IOSDevicePort
from webkitpy.port import ios_testcase
from webkitpy.port import port_testcase
from webkitpy.xcode.device_type import DeviceType


class IOSDeviceTest(ios_testcase.IOSTest):
    os_name = 'ios'
    os_version = None
    port_name = 'ios-device'
    port_maker = IOSDevicePort

    def test_operating_system(self):
        self.assertEqual('ios-device', self.make_port().operating_system())

    def test_crashlog_path(self):
        port = self.make_port()
        with self.assertRaises(RuntimeError):
            port.path_to_crash_logs()

    def test_tailspin(self):
        def logging_run_command(args):
            print(args)

        port = self.make_port()
        port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin-temp.txt'] = 'Temporary tailspin output file'
        port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin.txt'] = 'Symbolocated tailspin file'
        port.host.executive = MockExecutive2(run_command_fn=logging_run_command)
        expected_stdout = """['/usr/bin/tailspin', 'save', '-n', '/__im_tmp/tmp_0_/test-42-tailspin-temp.txt']
['/usr/sbin/spindump', '-i', '/__im_tmp/tmp_0_/test-42-tailspin-temp.txt', '-file', '/__im_tmp/tmp_0_/test-42-tailspin.txt', '-noBulkSymbolication']
"""
        OutputCapture().assert_outputs(self, port.sample_process, args=['test', 42], expected_stdout=expected_stdout)
        self.assertEqual(port.host.filesystem.files['/mock-build/layout-test-results/test-42-tailspin.txt'], 'Symbolocated tailspin file')
        self.assertIsNone(port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin-temp.txt'])
        self.assertIsNone(port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin.txt'])

    def test_sample_process(self):
        def logging_run_command(args):
            if args[0] == '/usr/bin/tailspin':
                return 1
            print(args)
            return 0

        port = self.make_port()
        port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-sample.txt'] = 'Sample file'
        port.host.executive = MockExecutive2(run_command_fn=logging_run_command)
        expected_stdout = "['/usr/bin/sample', 42, 10, 10, '-file', '/__im_tmp/tmp_0_/test-42-sample.txt']\n"
        OutputCapture().assert_outputs(self, port.sample_process, args=['test', 42], expected_stdout=expected_stdout)
        self.assertEqual(port.host.filesystem.files['/mock-build/layout-test-results/test-42-sample.txt'], 'Sample file')
        self.assertIsNone(port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-sample.txt'])

    def test_sample_process_exception(self):
        def throwing_run_command(args):
            if args[0] == '/usr/bin/tailspin':
                return 1
            raise ScriptError('MOCK script error')

        port = self.make_port()
        port.host.executive = MockExecutive2(run_command_fn=throwing_run_command)
        OutputCapture().assert_outputs(self, port.sample_process, args=['test', 42])

    def test_get_crash_log(self):
        port = self.make_port(port_name=self.port_name)
        self.assertEqual((None, None), port._get_crash_log('DumpRenderTree', 1234, None, None, time.time(), wait_for_log=False))

    def test_layout_test_searchpath_with_apple_additions(self):
        with port_testcase.bind_mock_apple_additions():
            search_path = self.make_port().default_baseline_search_path()

        self.assertEqual(search_path, [
            '/additional_testing_path/ios-device-add-ios14-wk1',
            '/mock-checkout/LayoutTests/platform/ios-device-14-wk1',
            '/additional_testing_path/ios-device-add-ios14',
            '/mock-checkout/LayoutTests/platform/ios-device-14',
            '/additional_testing_path/ios-device-wk1',
            '/mock-checkout/LayoutTests/platform/ios-device-wk1',
            '/additional_testing_path/ios-device',
            '/mock-checkout/LayoutTests/platform/ios-device',
            '/additional_testing_path/ios-add-ios14-wk1',
            '/mock-checkout/LayoutTests/platform/ios-14-wk1',
            '/additional_testing_path/ios-add-ios14',
            '/mock-checkout/LayoutTests/platform/ios-14',
            '/additional_testing_path/ios-wk1',
            '/mock-checkout/LayoutTests/platform/ios-wk1',
            '/additional_testing_path/ios',
            '/mock-checkout/LayoutTests/platform/ios',
        ])

    def test_layout_test_searchpath_without_apple_additions(self):
        search_path = self.make_port(port_name='ios-device-wk2', os_version=Version(14)).default_baseline_search_path()

        self.assertEqual(search_path, [
            '/mock-checkout/LayoutTests/platform/ios-device-14-wk2',
            '/mock-checkout/LayoutTests/platform/ios-device-14',
            '/mock-checkout/LayoutTests/platform/ios-device-wk2',
            '/mock-checkout/LayoutTests/platform/ios-device',
            '/mock-checkout/LayoutTests/platform/ios-14-wk2',
            '/mock-checkout/LayoutTests/platform/ios-14',
            '/mock-checkout/LayoutTests/platform/ios-wk2',
            '/mock-checkout/LayoutTests/platform/ios',
            '/mock-checkout/LayoutTests/platform/wk2',
        ])

    def test_layout_searchpath_wih_device_type(self):
        search_path = self.make_port(port_name='ios-device-wk2', os_version=Version(14)).default_baseline_search_path(DeviceType.from_string('iPhone SE'))

        self.assertEqual(search_path, [
            '/mock-checkout/LayoutTests/platform/iphone-se-device-14-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se-device-14',
            '/mock-checkout/LayoutTests/platform/iphone-se-device-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se-device',
            '/mock-checkout/LayoutTests/platform/iphone-device-14-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-device-14',
            '/mock-checkout/LayoutTests/platform/iphone-device-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-device',
            '/mock-checkout/LayoutTests/platform/ios-device-14-wk2',
            '/mock-checkout/LayoutTests/platform/ios-device-14',
            '/mock-checkout/LayoutTests/platform/ios-device-wk2',
            '/mock-checkout/LayoutTests/platform/ios-device',
            '/mock-checkout/LayoutTests/platform/iphone-se-14-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se-14',
            '/mock-checkout/LayoutTests/platform/iphone-se-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se',
            '/mock-checkout/LayoutTests/platform/iphone-14-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-14',
            '/mock-checkout/LayoutTests/platform/iphone-wk2',
            '/mock-checkout/LayoutTests/platform/iphone',
            '/mock-checkout/LayoutTests/platform/ios-14-wk2',
            '/mock-checkout/LayoutTests/platform/ios-14',
            '/mock-checkout/LayoutTests/platform/ios-wk2',
            '/mock-checkout/LayoutTests/platform/ios',
            '/mock-checkout/LayoutTests/platform/wk2',
        ])

    def test_max_child_processes(self):
        pass

    def test_default_upload_configuration(self):
        clear_cached_configuration()
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], False)
        self.assertEqual(configuration['platform'], port.host.platform.os_name)
        self.assertEqual(configuration['style'], 'release')
        self.assertEqual(configuration['version_name'], 'iOS {}'.format(port.device_version()))
