# Copyright (C) 2010 Google Inc. All rights reserved.
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

from webkitpy.layout_tests.port.mac import MacPort
from webkitpy.layout_tests.port import port_testcase
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.systemhost_mock import MockSystemHost


class MacTest(port_testcase.PortTestCase):
    os_name = 'mac'
    os_version = 'leopard'
    port_name = 'mac-leopard'
    port_maker = MacPort

    def assert_skipped_file_search_paths(self, port_name, expected_paths):
        port = self.make_port(port_name=port_name)
        self.assertEqual(port._skipped_file_search_paths(), expected_paths)

    def test_skipped_file_search_paths(self):
        self.assert_skipped_file_search_paths('mac-snowleopard', set(['mac-snowleopard', 'mac']))
        self.assert_skipped_file_search_paths('mac-leopard', set(['mac-leopard', 'mac']))
        # We cannot test just "mac" here as the MacPort constructor automatically fills in the version from the running OS.
        # self.assert_skipped_file_search_paths('mac', ['mac'])


    example_skipped_file = u"""
# <rdar://problem/5647952> fast/events/mouseout-on-window.html needs mac DRT to issue mouse out events
fast/events/mouseout-on-window.html

# <rdar://problem/5643675> window.scrollTo scrolls a window with no scrollbars
fast/events/attempt-scroll-with-no-scrollbars.html

# see bug <rdar://problem/5646437> REGRESSION (r28015): svg/batik/text/smallFonts fails
svg/batik/text/smallFonts.svg

# Java tests don't work on WK2
java/
"""
    example_skipped_tests = [
        "fast/events/mouseout-on-window.html",
        "fast/events/attempt-scroll-with-no-scrollbars.html",
        "svg/batik/text/smallFonts.svg",
        "java",
    ]

    def test_tests_from_skipped_file_contents(self):
        port = self.make_port()
        self.assertEqual(port._tests_from_skipped_file_contents(self.example_skipped_file), self.example_skipped_tests)

    def assert_name(self, port_name, os_version_string, expected):
        host = MockSystemHost(os_name='mac', os_version=os_version_string)
        port = self.make_port(host=host, port_name=port_name)
        self.assertEquals(expected, port.name())

    def test_tests_for_other_platforms(self):
        platforms = ['mac', 'chromium-linux', 'mac-snowleopard']
        port = self.make_port(port_name='mac-snowleopard')
        platform_dir_paths = map(port._webkit_baseline_path, platforms)
        # Replace our empty mock file system with one which has our expected platform directories.
        port._filesystem = MockFileSystem(dirs=platform_dir_paths)

        dirs_to_skip = port._tests_for_other_platforms()
        self.assertTrue('platform/chromium-linux' in dirs_to_skip)
        self.assertFalse('platform/mac' in dirs_to_skip)
        self.assertFalse('platform/mac-snowleopard' in dirs_to_skip)

    def test_version(self):
        port = self.make_port()
        self.assertTrue(port.version())

    def test_versions(self):
        self.assert_name(None, 'leopard', 'mac-leopard')
        self.assert_name('mac', 'leopard', 'mac-leopard')
        self.assert_name('mac-leopard', 'tiger', 'mac-leopard')
        self.assert_name('mac-leopard', 'leopard', 'mac-leopard')
        self.assert_name('mac-leopard', 'snowleopard', 'mac-leopard')

        self.assert_name(None, 'snowleopard', 'mac-snowleopard')
        self.assert_name('mac', 'snowleopard', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', 'tiger', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', 'leopard', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', 'snowleopard', 'mac-snowleopard')

        self.assert_name(None, 'lion', 'mac-lion')
        self.assert_name('mac', 'lion', 'mac-lion')
        self.assert_name('mac-lion', 'lion', 'mac-lion')

        self.assert_name(None, 'future', 'mac-future')
        self.assert_name('mac', 'future', 'mac-future')
        self.assert_name('mac-future', 'future', 'mac-future')


    def test_is_version_methods(self):
        leopard_port = self.make_port(port_name='mac-leopard')
        self.assertTrue(leopard_port.is_leopard())
        self.assertFalse(leopard_port.is_snowleopard())
        self.assertFalse(leopard_port.is_lion())

        snowleopard_port = self.make_port(port_name='mac-snowleopard')
        self.assertFalse(snowleopard_port.is_leopard())
        self.assertTrue(snowleopard_port.is_snowleopard())
        self.assertFalse(snowleopard_port.is_lion())

        lion_port = self.make_port(port_name='mac-lion')
        self.assertFalse(lion_port.is_leopard())
        self.assertFalse(lion_port.is_snowleopard())
        self.assertTrue(lion_port.is_lion())

    def test_setup_environ_for_server(self):
        port = self.make_port(options=MockOptions(leaks=True, guard_malloc=True))
        env = port.setup_environ_for_server(port.driver_name())
        self.assertEquals(env['MallocStackLogging'], '1')
        self.assertEquals(env['DYLD_INSERT_LIBRARIES'], '/usr/lib/libgmalloc.dylib')

    def _assert_search_path(self, search_paths, version, use_webkit2=False):
        # FIXME: Port constructors should not "parse" the port name, but
        # rather be passed components (directly or via setters).  Once
        # we fix that, this method will need a re-write.
        port = self.make_port(port_name='mac-%s' % version, options=MockOptions(webkit_test_runner=use_webkit2))
        absolute_search_paths = map(port._webkit_baseline_path, search_paths)
        self.assertEquals(port.baseline_search_path(), absolute_search_paths)

    def test_baseline_search_path(self):
        # FIXME: Is this really right?  Should mac-leopard fallback to mac-snowleopard?
        self._assert_search_path(['mac-leopard', 'mac-snowleopard', 'mac-lion', 'mac'], 'leopard')
        self._assert_search_path(['mac-snowleopard', 'mac-lion', 'mac'], 'snowleopard')
        self._assert_search_path(['mac-lion', 'mac'], 'lion')

        self._assert_search_path(['mac-wk2', 'mac-leopard', 'mac-snowleopard', 'mac-lion', 'mac'], 'leopard', use_webkit2=True)
        self._assert_search_path(['mac-wk2', 'mac-snowleopard', 'mac-lion', 'mac'], 'snowleopard', use_webkit2=True)
        self._assert_search_path(['mac-wk2', 'mac-lion', 'mac'], 'lion', use_webkit2=True)

    def test_show_results_html_file(self):
        port = self.make_port()
        # Delay setting a should_log executive to avoid logging from MacPort.__init__.
        port._executive = MockExecutive(should_log=True)
        expected_stderr = "MOCK run_command: ['Tools/Scripts/run-safari', '--release', '-NSOpen', 'test.html'], cwd=/mock-checkout\n"
        OutputCapture().assert_outputs(self, port.show_results_html_file, ["test.html"], expected_stderr=expected_stderr)

    def test_operating_system(self):
        self.assertEqual('mac', self.make_port().operating_system())
