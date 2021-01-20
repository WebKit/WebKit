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

import logging
import time

from webkitpy.port import port_testcase
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive, MockExecutive2, MockProcess, ScriptError
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.common.version_name_map import VersionNameMap

from webkitcorepy import OutputCapture


class DarwinTest(port_testcase.PortTestCase):

    def assert_skipped_file_search_paths(self, port_name, expected_paths, use_webkit2=False):
        port = self.make_port(port_name=port_name, options=MockOptions(webkit_test_runner=use_webkit2))
        self.assertEqual(port._skipped_file_search_paths(), expected_paths)

    def test_default_timeout_ms(self):
        super(DarwinTest, self).test_default_timeout_ms()
        self.assertEqual(self.make_port(options=MockOptions(guard_malloc=True)).default_timeout_ms(), 350000)

    def assert_name(self, port_name, os_version_string, expected):
        host = MockSystemHost(os_name=self.os_name, os_version=VersionNameMap.map().from_name(os_version_string)[1])
        port = self.make_port(host=host, port_name=port_name)
        self.assertEqual(expected, port.name())

    def test_show_results_html_file(self):
        port = self.make_port()
        # Delay setting a should_log executive to avoid logging from MacPort.__init__.
        port._executive = MockExecutive(should_log=True)
        with OutputCapture(level=logging.INFO) as captured:
            port.show_results_html_file('test.html')
        self.assertEqual(
            captured.root.log.getvalue(),
            "MOCK popen: ['Tools/Scripts/run-safari', '--release', '--no-saved-state', '-NSOpen', 'test.html'], cwd=/mock-checkout\n",
        )

    def test_helper_starts(self):
        host = MockSystemHost(MockExecutive())
        port = self.make_port(host)
        with OutputCapture():
            host.executive._proc = MockProcess('ready\n')
            port.start_helper()
            port.stop_helper()

        # make sure trying to stop the helper twice is safe.
        port.stop_helper()

    def test_helper_fails_to_start(self):
        host = MockSystemHost(MockExecutive())
        port = self.make_port(host)
        with OutputCapture():
            port.start_helper()
            port.stop_helper()

    def test_helper_fails_to_stop(self):
        host = MockSystemHost(MockExecutive())
        host.executive._proc = MockProcess()

        def bad_waiter():
            raise IOError('failed to wait')
        host.executive._proc.wait = bad_waiter

        port = self.make_port(host)
        with OutputCapture():
            port.start_helper()
            port.stop_helper()

    def test_crashlog_path(self):
        port = self.make_port()
        self.assertEqual(port.path_to_crash_logs(), '/Users/mock/Library/Logs/CrashReporter')

        host = MockSystemHost(filesystem=MockFileSystem(files={'/Users/mock/Library/Logs/DiagnosticReports/crashlog.crash': None}))
        port = self.make_port(host)
        self.assertEqual(port.path_to_crash_logs(), '/Users/mock/Library/Logs/DiagnosticReports')

    def test_tailspin(self):

        def logging_run_command(args):
            print(args)

        port = self.make_port()
        port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin-temp.txt'] = 'Temporary tailspin output file'
        port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin.txt'] = 'Symbolocated tailspin file'
        port.host.executive = MockExecutive2(run_command_fn=logging_run_command)
        with OutputCapture() as captured:
            port.sample_process('test', 42)
        self.assertEqual(
            captured.stdout.getvalue(),
            """['/usr/bin/sudo', '-n', '/usr/bin/tailspin', 'save', '-n', '/__im_tmp/tmp_0_/test-42-tailspin-temp.txt']
['/usr/sbin/spindump', '-i', '/__im_tmp/tmp_0_/test-42-tailspin-temp.txt', '-file', '/__im_tmp/tmp_0_/test-42-tailspin.txt', '-noBulkSymbolication']
""",
        )
        self.assertEqual(port.host.filesystem.files['/mock-build/layout-test-results/test-42-tailspin.txt'], 'Symbolocated tailspin file')
        self.assertIsNone(port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin-temp.txt'])
        self.assertIsNone(port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-tailspin.txt'])

    def test_sample_process(self):

        def logging_run_command(args):
            if args[0] == '/usr/bin/sudo':
                return 1
            print(args)
            return 0

        port = self.make_port()
        port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-sample.txt'] = 'Sample file'
        port.host.executive = MockExecutive2(run_command_fn=logging_run_command)
        with OutputCapture() as captured:
            port.sample_process('test', 42)
        self.assertEqual(
            captured.stdout.getvalue(),
            "['/usr/bin/sample', 42, 10, 10, '-file', '/__im_tmp/tmp_0_/test-42-sample.txt']\n",
        )
        self.assertEqual(port.host.filesystem.files['/mock-build/layout-test-results/test-42-sample.txt'], 'Sample file')
        self.assertIsNone(port.host.filesystem.files['/__im_tmp/tmp_0_/test-42-sample.txt'])

    def test_sample_process_exception(self):
        def throwing_run_command(args):
            if args[0] == '/usr/bin/sudo':
                return 1
            raise ScriptError("MOCK script error")

        port = self.make_port()
        port.host.executive = MockExecutive2(run_command_fn=throwing_run_command)
        with OutputCapture() as captured:
            port.sample_process('test', 42)
        self.assertEqual(captured.stdout.getvalue(), '')

    def test_get_crash_log(self):
        port = self.make_port(port_name=self.port_name)
        port._get_crash_log('DumpRenderTree', 1234, None, None, time.time(), wait_for_log=False)
