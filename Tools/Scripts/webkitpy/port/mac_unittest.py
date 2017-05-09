# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

import time

from webkitpy.port.mac import MacPort
from webkitpy.port import darwin_testcase
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2, MockProcess, ScriptError
from webkitpy.common.system.systemhost_mock import MockSystemHost


class MacTest(darwin_testcase.DarwinTest):
    os_name = 'mac'
    os_version = 'lion'
    port_name = 'mac-lion'
    port_maker = MacPort

    def test_tests_for_other_platforms(self):
        platforms = ['mac', 'mac-snowleopard']
        port = self.make_port(port_name='mac-snowleopard')
        platform_dir_paths = map(port._webkit_baseline_path, platforms)
        # Replace our empty mock file system with one which has our expected platform directories.
        port._filesystem = MockFileSystem(dirs=platform_dir_paths)

        dirs_to_skip = port._tests_for_other_platforms()
        self.assertNotIn('platform/mac', dirs_to_skip)
        self.assertNotIn('platform/mac-snowleopard', dirs_to_skip)

    def test_version(self):
        port = self.make_port()
        self.assertTrue(port.version())

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
        self.assert_name('mac', 'future', 'mac-future')
        self.assert_name('mac-future', 'future', 'mac-future')
        self.assertRaises(AssertionError, self.assert_name, 'mac-tiger', 'leopard', 'mac-leopard')

    def test_setup_environ_for_server(self):
        port = self.make_port(options=MockOptions(leaks=True, guard_malloc=True))
        env = port.setup_environ_for_server(port.driver_name())
        self.assertEqual(env['MallocStackLogging'], '1')
        self.assertEqual(env['MallocScribble'], '1')
        self.assertEqual(env['DYLD_INSERT_LIBRARIES'], '/usr/lib/libgmalloc.dylib:/mock-build/libWebCoreTestShim.dylib')

    def _assert_search_path(self, port_name, baseline_path, search_paths, use_webkit2=False):
        port = self.make_port(port_name=port_name, options=MockOptions(webkit_test_runner=use_webkit2))
        absolute_search_paths = map(port._webkit_baseline_path, search_paths)
        self.assertEqual(port.baseline_path(), port._webkit_baseline_path(baseline_path))
        self.assertEqual(port.baseline_search_path(), absolute_search_paths)

    def test_baseline_search_path(self):
        # Note that we don't need total coverage here, just path coverage, since this is all data driven.
        self._assert_search_path('mac-snowleopard', 'mac-snowleopard', ['mac-snowleopard', 'mac-lion', 'mac-mountainlion', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac-wk1', 'mac'])
        self._assert_search_path('mac-lion', 'mac-lion', ['mac-lion', 'mac-mountainlion', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac-wk1', 'mac'])
        self._assert_search_path('mac-mountainlion', 'mac-mountainlion', ['mac-mountainlion', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac-wk1', 'mac'])
        self._assert_search_path('mac-mavericks', 'mac-mavericks', ['mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac-wk1', 'mac'])
        self._assert_search_path('mac-yosemite', 'mac-yosemite', ['mac-yosemite', 'mac-elcapitan', 'mac-wk1', 'mac'])
        self._assert_search_path('mac-elcapitan', 'mac-elcapitan', ['mac-elcapitan', 'mac-wk1', 'mac'])
        self._assert_search_path('mac-sierra', 'mac-wk1', ['mac-wk1', 'mac'])
        self._assert_search_path('mac-future', 'mac-wk1', ['mac-wk1', 'mac'])
        self._assert_search_path('mac-snowleopard', 'mac-wk2', ['mac-wk2', 'wk2', 'mac-snowleopard', 'mac-lion', 'mac-mountainlion', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac'], use_webkit2=True)
        self._assert_search_path('mac-lion', 'mac-wk2', ['mac-wk2', 'wk2', 'mac-lion', 'mac-mountainlion', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac'], use_webkit2=True)
        self._assert_search_path('mac-mountainlion', 'mac-wk2', ['mac-wk2', 'wk2', 'mac-mountainlion', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac'], use_webkit2=True)
        self._assert_search_path('mac-mavericks', 'mac-wk2', ['mac-wk2', 'wk2', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac'], use_webkit2=True)
        self._assert_search_path('mac-yosemite', 'mac-wk2', ['mac-wk2', 'wk2', 'mac-yosemite', 'mac-elcapitan', 'mac'], use_webkit2=True)
        self._assert_search_path('mac-elcapitan', 'mac-wk2', ['mac-wk2', 'wk2', 'mac-elcapitan', 'mac'], use_webkit2=True)
        self._assert_search_path('mac-sierra', 'mac-wk2', ['mac-wk2', 'wk2', 'mac'], use_webkit2=True)
        self._assert_search_path('mac-future', 'mac-wk2', ['mac-wk2', 'wk2', 'mac'], use_webkit2=True)

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
        # Apple Mac port is 64-bit by default
        port = self.make_port()
        self.assertEqual(port.architecture(), 'x86_64')

        def run_script(script, args=None, env=None):
            self.args = args

        port._run_script = run_script
        port._build_driver()
        self.assertEqual(self.args, [])

    def test_sdk_name(self):
        port = self.make_port()
        self.assertEqual(port.SDK, 'macosx')

    def test_xcrun(self):
        def throwing_run_command(args):
            print args
            raise ScriptError("MOCK script error")

        port = self.make_port()
        port._executive = MockExecutive2(run_command_fn=throwing_run_command)
        expected_stdout = "['xcrun', '--sdk', 'macosx', '-find', 'test']\n"
        OutputCapture().assert_outputs(self, port.xcrun_find, args=['test', 'falling'], expected_stdout=expected_stdout)
