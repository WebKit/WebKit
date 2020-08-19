# Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

from webkitcorepy import Version

from webkitpy.port.ios_simulator import IOSSimulatorPort
from webkitpy.port import ios_testcase
from webkitpy.port import port_testcase
from webkitpy.port.config import clear_cached_configuration
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.xcode.device_type import DeviceType

from webkitcorepy import OutputCapture


class IOSSimulatorTest(ios_testcase.IOSTest):
    # FIXME: https://bugs.webkit.org/show_bug.cgi?id=173107
    os_name = 'mac'
    os_version = None
    port_name = 'ios-simulator'
    port_maker = IOSSimulatorPort

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=Version(14), **kwargs):
        port = super(IOSSimulatorTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, os_version=os_version, kwargs=kwargs)
        port.set_option('child_processes', 1)
        return port

    def test_setup_environ_for_server(self):
        port = self.make_port(options=MockOptions(leaks=True, guard_malloc=True))
        env = port.setup_environ_for_server(port.driver_name())
        self.assertEqual(env['MallocStackLogging'], '1')
        self.assertEqual(env['MallocScribble'], '1')
        self.assertEqual(env['DYLD_INSERT_LIBRARIES'], '/usr/lib/libgmalloc.dylib')

    def test_operating_system(self):
        self.assertEqual('ios-simulator', self.make_port().operating_system())

    def test_32bit(self):
        port = self.make_port(options=MockOptions(architecture='i386'))

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        self.assertEqual(port.architecture(), 'i386')
        port._build_driver()
        self.assertEqual(self.args, ['--sdk', 'iphonesimulator', 'ARCHS=i386'])

    def test_64bit(self):
        # Apple Mac port is 64-bit by default
        port = self.make_port()
        self.assertEqual(port.architecture(), 'x86_64')

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        port._build_driver()
        self.assertEqual(self.args, ['--sdk', 'iphonesimulator', 'ARCHS=x86_64'])

    def test_sdk_name(self):
        port = self.make_port()
        self.assertEqual(port.SDK, 'iphonesimulator')

    def test_xcrun(self):
        def throwing_run_command(args):
            print(args)
            raise ScriptError("MOCK script error")

        port = self.make_port()
        port._executive = MockExecutive2(run_command_fn=throwing_run_command)
        with OutputCapture() as captured:
            port.xcrun_find('test', 'falling')
        self.assertEqual(
            captured.stdout.getvalue(),
            "['xcrun', '--sdk', 'iphonesimulator', '-find', 'test']\n",
        )

    def test_layout_test_searchpath_with_apple_additions(self):
        with port_testcase.bind_mock_apple_additions():
            search_path = self.make_port().default_baseline_search_path()

        self.assertEqual(search_path, [
            '/additional_testing_path/ios-simulator-add-ios14-wk1',
            '/mock-checkout/LayoutTests/platform/ios-simulator-14-wk1',
            '/additional_testing_path/ios-simulator-add-ios14',
            '/mock-checkout/LayoutTests/platform/ios-simulator-14',
            '/additional_testing_path/ios-simulator-wk1',
            '/mock-checkout/LayoutTests/platform/ios-simulator-wk1',
            '/additional_testing_path/ios-simulator',
            '/mock-checkout/LayoutTests/platform/ios-simulator',
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
        search_path = self.make_port(port_name='ios-simulator-wk2', os_version=Version(14)).default_baseline_search_path()

        self.assertEqual(search_path, [
            '/mock-checkout/LayoutTests/platform/ios-simulator-14-wk2',
            '/mock-checkout/LayoutTests/platform/ios-simulator-14',
            '/mock-checkout/LayoutTests/platform/ios-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/ios-simulator',
            '/mock-checkout/LayoutTests/platform/ios-14-wk2',
            '/mock-checkout/LayoutTests/platform/ios-14',
            '/mock-checkout/LayoutTests/platform/ios-wk2',
            '/mock-checkout/LayoutTests/platform/ios',
            '/mock-checkout/LayoutTests/platform/wk2',
        ])

    def test_layout_searchpath_wih_device_type(self):
        search_path = self.make_port(port_name='ios-simulator-wk2', os_version=Version(14)).default_baseline_search_path(DeviceType.from_string('iPhone SE'))

        self.assertEqual(search_path, [
            '/mock-checkout/LayoutTests/platform/iphone-se-simulator-14-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se-simulator-14',
            '/mock-checkout/LayoutTests/platform/iphone-se-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-se-simulator',
            '/mock-checkout/LayoutTests/platform/iphone-simulator-14-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-simulator-14',
            '/mock-checkout/LayoutTests/platform/iphone-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/iphone-simulator',
            '/mock-checkout/LayoutTests/platform/ios-simulator-14-wk2',
            '/mock-checkout/LayoutTests/platform/ios-simulator-14',
            '/mock-checkout/LayoutTests/platform/ios-simulator-wk2',
            '/mock-checkout/LayoutTests/platform/ios-simulator',
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
        port = self.make_port()
        self.assertEqual(port.max_child_processes(DeviceType.from_string('Apple Watch')), 0)

    def test_default_upload_configuration(self):
        clear_cached_configuration()
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], True)
        self.assertEqual(configuration['platform'], 'ios')
        self.assertEqual(configuration['style'], 'release')
        self.assertEqual(configuration['version_name'], 'iOS {}'.format(port.device_version()))
