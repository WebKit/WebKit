# Copyright (C) 2018 Apple Inc. All rights reserved.
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

from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.version import Version
from webkitpy.port.config import clear_cached_configuration
from webkitpy.port.watch_simulator import WatchSimulatorPort
from webkitpy.port import watch_testcase
from webkitpy.tool.mocktool import MockOptions
from webkitpy.xcode.device_type import DeviceType


class WatchSimulatorTest(watch_testcase.WatchTest):
    # FIXME: https://bugs.webkit.org/show_bug.cgi?id=173107
    os_name = 'mac'
    os_version = None
    port_name = 'watch-simulator'
    port_maker = WatchSimulatorPort

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=Version(5), **kwargs):
        port = super(WatchSimulatorTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, os_version=os_version, kwargs=kwargs)
        port.set_option('child_processes', 1)
        return port

    def test_setup_environ_for_server(self):
        port = self.make_port(options=MockOptions(leaks=True, guard_malloc=True))
        env = port.setup_environ_for_server(port.driver_name())
        self.assertEqual(env['MallocStackLogging'], '1')
        self.assertEqual(env['MallocScribble'], '1')
        self.assertEqual(env['DYLD_INSERT_LIBRARIES'], '/usr/lib/libgmalloc.dylib')

    def test_operating_system(self):
        self.assertEqual('watchos-simulator', self.make_port().operating_system())

    def test_32bit(self):
        port = self.make_port(options=MockOptions(architecture='i386'))

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        self.assertEqual(port.architecture(), 'i386')
        port._build_driver()
        self.assertEqual(self.args, ['--sdk', 'watchsimulator', 'ARCHS=i386'])

    def test_sdk_name(self):
        port = self.make_port()
        self.assertEqual(port.SDK, 'watchsimulator')

    def test_xcrun(self):
        def throwing_run_command(args):
            print(args)
            raise ScriptError("MOCK script error")

        port = self.make_port(options=MockOptions(architecture='i386'))
        port._executive = MockExecutive2(run_command_fn=throwing_run_command)
        expected_stdout = "['xcrun', '--sdk', 'watchsimulator', '-find', 'test']\n"
        OutputCapture().assert_outputs(self, port.xcrun_find, args=['test', 'falling'], expected_stdout=expected_stdout)

    def test_max_child_processes(self):
        port = self.make_port()
        self.assertEqual(port.max_child_processes(DeviceType.from_string('iPhone')), 0)

    def test_default_upload_configuration(self):
        clear_cached_configuration()
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], True)
        self.assertEqual(configuration['platform'], 'watchos')
        self.assertEqual(configuration['style'], 'release')
        self.assertEqual(configuration['version_name'], 'watchOS {}'.format(port.device_version()))
