# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

from webkitpy.port.mac import MacPort
from webkitpy.port import darwin_testcase
from webkitpy.port import port_testcase
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2, ScriptError
from webkitpy.common.version import Version
from webkitpy.common.version_name_map import VersionNameMap


class MacTest(darwin_testcase.DarwinTest):
    os_name = 'mac'
    os_version = Version.from_name('Lion')
    port_name = 'mac-lion'
    port_maker = MacPort

    # 2 minor versions from the current version should always be a future version.
    FUTURE_VERSION = Version.from_iterable(MacPort.CURRENT_VERSION)
    FUTURE_VERSION.minor += 2


    def test_version(self):
        port = self.make_port()
        self.assertIsNotNone(port.version_name())

    def test_versions(self):
        # Note: these tests don't need to be exhaustive as long as we get path coverage.
        self.assert_name('mac', 'snowleopard', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', 'leopard', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', 'lion', 'mac-snowleopard')
        self.assert_name('mac', 'lion', 'mac-lion')
        self.assert_name('mac-lion', 'lion', 'mac-lion')
        self.assert_name('mac', 'mountainlion', 'mac-mountainlion')
        self.assert_name('mac-mountainlion', 'lion', 'mac-mountainlion')
        self.assert_name('mac', 'mavericks', 'mac-mavericks')
        self.assert_name('mac-mavericks', 'mountainlion', 'mac-mavericks')
        self.assert_name('mac', 'yosemite', 'mac-yosemite')
        self.assert_name('mac-yosemite', 'mavericks', 'mac-yosemite')
        self.assert_name('mac', 'elcapitan', 'mac-elcapitan')
        self.assert_name('mac-elcapitan', 'mavericks', 'mac-elcapitan')
        self.assert_name('mac-elcapitan', 'yosemite', 'mac-elcapitan')
        self.assert_name('mac', 'sierra', 'mac-sierra')
        self.assert_name('mac-sierra', 'yosemite', 'mac-sierra')
        self.assert_name('mac-sierra', 'elcapitan', 'mac-sierra')
        self.assert_name('mac', 'highsierra', 'mac-highsierra')
        self.assert_name('mac-highsierra', 'elcapitan', 'mac-highsierra')
        self.assert_name('mac-highsierra', 'sierra', 'mac-highsierra')
        self.assert_name('mac', 'mojave', 'mac-mojave')
        self.assert_name('mac-mojave', 'sierra', 'mac-mojave')
        self.assert_name('mac-mojave', 'highsierra', 'mac-mojave')
        self.assertRaises(AssertionError, self.assert_name, 'mac-tiger', 'leopard', 'mac-leopard')

    def test_setup_environ_for_server(self):
        port = self.make_port(options=MockOptions(leaks=True, guard_malloc=True))
        env = port.setup_environ_for_server(port.driver_name())
        self.assertEqual(env['MallocStackLogging'], '1')
        self.assertEqual(env['MallocScribble'], '1')
        self.assertEqual(env['DYLD_INSERT_LIBRARIES'], '/usr/lib/libgmalloc.dylib:/mock-build/libWebCoreTestShim.dylib')

    def test_show_results_html_file(self):
        port = self.make_port()
        # Delay setting a should_log executive to avoid logging from MacPort.__init__.
        port._executive = MockExecutive(should_log=True)
        expected_logs = "MOCK popen: ['Tools/Scripts/run-safari', '--release', '--no-saved-state', '-NSOpen', 'test.html'], cwd=/mock-checkout\n"
        OutputCapture().assert_outputs(self, port.show_results_html_file, ["test.html"], expected_logs=expected_logs)

    def test_operating_system(self):
        self.assertEqual('mac', self.make_port().operating_system())

    def test_default_child_processes(self):
        port = self.make_port(port_name='mac-lion')
        # MockPlatformInfo only has 2 mock cores.  The important part is that 2 > 1.
        self.assertEqual(port.default_child_processes(), 2)

        bytes_for_drt = 200 * 1024 * 1024
        port.host.platform.total_bytes_memory = lambda: bytes_for_drt
        expected_logs = "This machine could support 2 child processes, but only has enough memory for 1.\n"
        child_processes = OutputCapture().assert_outputs(self, port.default_child_processes, (), expected_logs=expected_logs)
        self.assertEqual(child_processes, 1)

        # Make sure that we always use one process, even if we don't have the memory for it.
        port.host.platform.total_bytes_memory = lambda: bytes_for_drt - 1
        expected_logs = "This machine could support 2 child processes, but only has enough memory for 1.\n"
        child_processes = OutputCapture().assert_outputs(self, port.default_child_processes, (), expected_logs=expected_logs)
        self.assertEqual(child_processes, 1)

    def test_32bit(self):
        port = self.make_port(options=MockOptions(architecture='x86'))

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        self.assertEqual(port.architecture(), 'x86')
        port._build_driver()
        self.assertEqual(self.args, ['ARCHS=i386'])

    def test_64bit(self):
        port = self.make_port(options=MockOptions(architecture='x86_64'))
        self.assertEqual(port.architecture(), 'x86_64')

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        port._build_driver()
        self.assertEqual(self.args, ['ARCHS=x86_64'])

    def test_arm(self):
        port = self.make_port(options=MockOptions(architecture='arm64e'))
        self.assertEqual(port.architecture(), 'arm64')

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        port._build_driver()
        self.assertEqual(self.args, ['ARCHS=arm64'])

    def test_default(self):
        port = self.make_port()
        self.assertEqual(port.architecture(), port.host.platform.architecture())

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        port._build_driver()
        self.assertEqual(self.args, ['ARCHS={}'.format(port.host.platform.architecture())])

    def test_sdk_name(self):
        port = self.make_port()
        self.assertEqual(port.SDK, 'macosx')

    def test_xcrun(self):
        def throwing_run_command(args):
            print(args)
            raise ScriptError("MOCK script error")

        port = self.make_port()
        port._executive = MockExecutive2(run_command_fn=throwing_run_command)
        expected_stdout = "['xcrun', '--sdk', 'macosx', '-find', 'test']\n"
        OutputCapture().assert_outputs(self, port.xcrun_find, args=['test', 'falling'], expected_stdout=expected_stdout)

    def test_layout_test_searchpath_with_apple_additions(self):
        with port_testcase.bind_mock_apple_additions():
            search_path = self.make_port().default_baseline_search_path()
        self.assertEqual(search_path[0], '/additional_testing_path/mac-add-lion-wk1')
        self.assertEqual(search_path[1], '/mock-checkout/LayoutTests/platform/mac-lion-wk1')
        self.assertEqual(search_path[2], '/additional_testing_path/mac-add-lion')
        self.assertEqual(search_path[3], '/mock-checkout/LayoutTests/platform/mac-lion')
        self.assertEqual(search_path[4], '/additional_testing_path/mac-add-mountainlion-wk1')
        self.assertEqual(search_path[5], '/mock-checkout/LayoutTests/platform/mac-mountainlion-wk1')

    def test_big_sur_baseline_search_path(self):
        search_path = self.make_port(port_name='macos-big-sur').default_baseline_search_path()
        self.assertEqual(search_path[0], '/mock-checkout/LayoutTests/platform/mac-catalina-wk1')
        self.assertEqual(search_path[1], '/mock-checkout/LayoutTests/platform/mac-catalina')
        self.assertEqual(search_path[2], '/mock-checkout/LayoutTests/platform/mac-wk1')
        self.assertEqual(search_path[3], '/mock-checkout/LayoutTests/platform/mac')

    def test_factory_with_future_version(self):
        port = self.make_port(options=MockOptions(webkit_test_runner=True), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')
        self.assertEqual(port.version_name(), VersionNameMap().to_name(MacPort.CURRENT_VERSION, platform=MacPort.port_name))

        port = self.make_port(options=MockOptions(webkit_test_runner=False), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac')
        self.assertEqual(port.driver_name(), 'DumpRenderTree')
        self.assertEqual(port.version_name(), VersionNameMap().to_name(MacPort.CURRENT_VERSION, platform=MacPort.port_name))

        port = self.make_port(options=MockOptions(webkit_test_runner=False), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac-wk2')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')
        self.assertEqual(port.version_name(), VersionNameMap().to_name(MacPort.CURRENT_VERSION, platform=MacPort.port_name))

        port = self.make_port(options=MockOptions(webkit_test_runner=True), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac-wk2')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')
        self.assertEqual(port.version_name(), VersionNameMap().to_name(MacPort.CURRENT_VERSION, platform=MacPort.port_name))

    def test_factory_with_future_version_and_apple_additions(self):
        with port_testcase.bind_mock_apple_additions():
            port = self.make_port(options=MockOptions(webkit_test_runner=True), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac')
            self.assertEqual(port.driver_name(), 'WebKitTestRunner')
            self.assertEqual(port.version_name(), None)

            port = self.make_port(options=MockOptions(webkit_test_runner=False), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac')
            self.assertEqual(port.driver_name(), 'DumpRenderTree')
            self.assertEqual(port.version_name(), None)

            port = self.make_port(options=MockOptions(webkit_test_runner=False), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac-wk2')
            self.assertEqual(port.driver_name(), 'WebKitTestRunner')
            self.assertEqual(port.version_name(), None)

            port = self.make_port(options=MockOptions(webkit_test_runner=True), os_version=MacTest.FUTURE_VERSION, os_name='mac', port_name='mac-wk2')
            self.assertEqual(port.driver_name(), 'WebKitTestRunner')
            self.assertEqual(port.version_name(), None)

    def test_factory_with_portname_version(self):
        port = self.make_port(options=MockOptions(webkit_test_runner=False), port_name='mac-mountainlion')
        self.assertEqual(port.driver_name(), 'DumpRenderTree')
        self.assertEqual(port.version_name(), 'Mountain Lion')

        port = self.make_port(options=MockOptions(webkit_test_runner=True), port_name='mac-mountainlion')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')
        self.assertEqual(port.version_name(), 'Mountain Lion')

        port = self.make_port(options=MockOptions(webkit_test_runner=True), port_name='mac-mountainlion-wk2')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')
        self.assertEqual(port.version_name(), 'Mountain Lion')

        port = self.make_port(options=MockOptions(webkit_test_runner=False), port_name='mac-mountainlion-wk2')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')
        self.assertEqual(port.version_name(), 'Mountain Lion')

    def test_factory_with_portname_wk2(self):
        port = self.make_port(options=MockOptions(webkit_test_runner=False), port_name='mac')
        self.assertEqual(port.driver_name(), 'DumpRenderTree')

        port = self.make_port(options=MockOptions(webkit_test_runner=True), port_name='mac')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')

        port = self.make_port(options=MockOptions(webkit_test_runner=True), port_name='mac-wk2')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')

        port = self.make_port(options=MockOptions(webkit_test_runner=False), port_name='mac-wk2')
        self.assertEqual(port.driver_name(), 'WebKitTestRunner')

    def test_configuration_for_upload(self):
        port = self.make_port()
        self.assertEqual(
            dict(
                platform='mac',
                is_simulator=False,
                architecture='x86_64',
                version='10.7',
                version_name='Lion',
                sdk='17A405',
                style='release',
            ),
            port.configuration_for_upload(),
        )
