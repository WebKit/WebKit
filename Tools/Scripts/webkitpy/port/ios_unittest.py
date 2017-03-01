# Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

from webkitpy.port.ios_simulator import IOSSimulatorPort
from webkitpy.port import darwin_testcase
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2, MockProcess, ScriptError
from webkitpy.common.system.systemhost_mock import MockSystemHost


class iosTest(darwin_testcase.DarwinTest):
    os_name = 'ios-simulator'
    os_version = ''
    port_name = 'ios-simulator'
    port_maker = IOSSimulatorPort
    is_simulator = True

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=None, **kwargs):
        port = super(iosTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, s_version=os_version, kwargs=kwargs)
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

    def test_get_crash_log(self):
        # Mac crash logs are tested elsewhere, so here we just make sure we don't crash.
        def fake_time_cb():
            times = [0, 20, 40]
            return lambda: times.pop(0)
        port = self.make_port(port_name='ios-simulator')
        port._get_crash_log('DumpRenderTree', 1234, None, None, time.time(), wait_for_log=False)

    def test_32bit(self):
        port = self.make_port(options=MockOptions(architecture='x86'))

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        self.assertEqual(port.architecture(), 'x86')
        port._build_driver()
        self.assertEqual(self.args, ['ARCHS=i386', '--sdk', 'iphonesimulator'])

    def test_64bit(self):
        # Apple Mac port is 64-bit by default
        port = self.make_port()
        self.assertEqual(port.architecture(), 'x86_64')

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        port._build_driver()
        self.assertEqual(self.args, ['--sdk', 'iphonesimulator'])

    def test_sdk_name(self):
        port = self.make_port()
        self.assertEqual(port.SDK, 'iphonesimulator')

    def test_xcrun(self):
        def throwing_run_command(args):
            print args
            raise ScriptError("MOCK script error")

        port = self.make_port()
        port._executive = MockExecutive2(run_command_fn=throwing_run_command)
        expected_stdout = "['xcrun', '--sdk', 'iphonesimulator', '-find', 'test']\n"
        OutputCapture().assert_outputs(self, port.xcrun_find, args=['test', 'falling'], expected_stdout=expected_stdout)
