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

import StringIO
import sys
import unittest

from webkitpy.layout_tests.port.mac import MacPort, LeakDetector
from webkitpy.layout_tests.port import port_testcase
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockOptions, MockUser, MockExecutive


class LeakDetectorTest(unittest.TestCase):
    def _mock_port(self):
        class MockPort(object):
            def __init__(self):
                self._filesystem = MockFileSystem()
                self._executive = MockExecutive()

        return MockPort()

    def _make_detector(self):
        return LeakDetector(self._mock_port())

    def test_leaks_args(self):
        detector = self._make_detector()
        detector._callstacks_to_exclude_from_leaks = lambda: ['foo bar', 'BAZ']
        detector._types_to_exlude_from_leaks = lambda: ['abcdefg', 'hi jklmno']
        expected_args = ['--exclude-callstack="foo bar"', '--exclude-callstack="BAZ"', '--exclude-type="abcdefg"', '--exclude-type="hi jklmno"', 1234]
        self.assertEquals(detector._leaks_args(1234), expected_args)

    example_leaks_output = """Process 5122: 663744 nodes malloced for 78683 KB
Process 5122: 337301 leaks for 6525216 total leaked bytes.
Leak: 0x38cb600  size=3072  zone: DefaultMallocZone_0x1d94000   instance of 'NSCFData', type ObjC, implemented in Foundation
        0xa033f0b8 0x01001384 0x00000b3a 0x00000b3a     ..3.....:...:...
        0x00000000 0x038cb620 0x00000000 0x00000000     .... ...........
        0x00000000 0x21000000 0x726c6468 0x00000000     .......!hdlr....
        0x00000000 0x7269646d 0x6c707061 0x00000000     ....mdirappl....
        0x00000000 0x04000000 0x736c69c1 0x00000074     .........ilst...
        0x6f74a923 0x0000006f 0x7461641b 0x00000061     #.too....data...
        0x00000001 0x76614c00 0x2e323566 0x302e3236     .....Lavf52.62.0
        0x37000000 0x6d616ea9 0x2f000000 0x61746164     ...7.nam.../data
        ...
Leak: 0x2a9c960  size=288  zone: DefaultMallocZone_0x1d94000
        0x09a1cc47 0x1bda8560 0x3d472cd1 0xfbe9bccd     G...`....,G=....
        0x8bcda008 0x9e972a91 0xa892cf63 0x2448bdb0     .....*..c.....H$
        0x4736fc34 0xdbe2d94e 0x25f56688 0x839402a4     4.6GN....f.%....
        0xd12496b3 0x59c40c12 0x8cfcab2a 0xd20ef9c4     ..$....Y*.......
        0xe7c56b1b 0x5835af45 0xc69115de 0x6923e4bb     .k..E.5X......#i
        0x86f15553 0x15d40fa9 0x681288a4 0xc33298a9     SU.........h..2.
        0x439bb535 0xc4fc743d 0x7dfaaff8 0x2cc49a4a     5..C=t.....}J..,
        0xdd119df8 0x7e086821 0x3d7d129e 0x2e1b1547     ....!h.~..}=G...
        ...
Leak: 0x25102fe0  size=176  zone: DefaultMallocZone_0x1d94000   string 'NSException Data'
"""

    def test_parse_leaks_output(self):
        self.assertEquals(self._make_detector()._parse_leaks_output(self.example_leaks_output, 5122), (337301, 0, 6525216))

    def test_leaks_files_in_directory(self):
        detector = self._make_detector()
        self.assertEquals(detector.leaks_files_in_directory('/bogus-directory'), [])
        detector._filesystem = MockFileSystem({
            '/mock-results/leaks-DumpRenderTree-0-1.txt': '',
            '/mock-results/leaks-DumpRenderTree-1-1.txt': '',
            '/mock-results/leaks-DumpRenderTree-0-2.txt': '',
        })
        self.assertEquals(len(detector.leaks_files_in_directory('/mock-results')), 3)

    def test_parse_leak_files(self):
        detector = self._make_detector()

        def mock_run_script(name, args, include_configuration_arguments=False):
            print "MOCK _run_script: %s %s" % (name, args)
            return """1 calls for 16 bytes: -[NSURLRequest mutableCopyWithZone:] | +[NSObject(NSObject) allocWithZone:] | _internal_class_createInstanceFromZone | calloc | malloc_zone_calloc

total: 5,888 bytes (0 bytes excluded)."""
        detector._port._run_script = mock_run_script

        leak_files = ['/mock-results/leaks-DumpRenderTree-1234.txt', '/mock-results/leaks-DumpRenderTree-1235.txt']
        expected_stdout = "MOCK _run_script: parse-malloc-history ['--merge-depth', 5, '/mock-results/leaks-DumpRenderTree-1234.txt', '/mock-results/leaks-DumpRenderTree-1235.txt']\n"
        results_tuple = OutputCapture().assert_outputs(self, detector.parse_leak_files, [leak_files], expected_stdout=expected_stdout)
        self.assertEquals(results_tuple, ("5,888 bytes", 1))


class MacTest(port_testcase.PortTestCase):
    def port_maker(self, platform):
        # FIXME: This platform check should no longer be necessary and should be removed as soon as possible.
        if platform != 'darwin':
            return None
        return MacPort

    def assert_skipped_file_search_paths(self, port_name, expected_paths):
        port = MacPort(port_name=port_name, filesystem=MockFileSystem(), user=MockUser(), executive=MockExecutive())
        self.assertEqual(port._skipped_file_search_paths(), expected_paths)

    def test_skipped_file_search_paths(self):
        # FIXME: This should no longer be necessary, but I'm not brave enough to remove it.
        if sys.platform == 'win32':
            return None

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
"""
    example_skipped_tests = [
        "fast/events/mouseout-on-window.html",
        "fast/events/attempt-scroll-with-no-scrollbars.html",
        "svg/batik/text/smallFonts.svg",
    ]

    def test_tests_from_skipped_file_contents(self):
        port = MacPort(filesystem=MockFileSystem(), user=MockUser(), executive=MockExecutive())
        self.assertEqual(port._tests_from_skipped_file_contents(self.example_skipped_file), self.example_skipped_tests)

    def assert_name(self, port_name, os_version_string, expected):
        port = MacPort(port_name=port_name, os_version_string=os_version_string, filesystem=MockFileSystem(), user=MockUser(), executive=MockExecutive())
        self.assertEquals(expected, port.name())

    def test_tests_for_other_platforms(self):
        platforms = ['mac', 'chromium-linux', 'mac-snowleopard']
        port = MacPort(port_name='mac-snowleopard', filesystem=MockFileSystem(), user=MockUser(), executive=MockExecutive())
        platform_dir_paths = map(port._webkit_baseline_path, platforms)
        # Replace our empty mock file system with one which has our expected platform directories.
        port._filesystem = MockFileSystem(dirs=platform_dir_paths)

        dirs_to_skip = port._tests_for_other_platforms()
        self.assertTrue('platform/chromium-linux' in dirs_to_skip)
        self.assertFalse('platform/mac' in dirs_to_skip)
        self.assertFalse('platform/mac-snowleopard' in dirs_to_skip)

    def test_version(self):
        port = MacPort(filesystem=MockFileSystem(), user=MockUser(), executive=MockExecutive())
        self.assertTrue(port.version())

    def test_versions(self):
        port = self.make_port()
        if port:
            self.assertTrue(port.name() in ('mac-leopard', 'mac-snowleopard', 'mac-lion'))

        self.assert_name(None, '10.5.3', 'mac-leopard')
        self.assert_name('mac', '10.5.3', 'mac-leopard')
        self.assert_name('mac-leopard', '10.4.8', 'mac-leopard')
        self.assert_name('mac-leopard', '10.5.3', 'mac-leopard')
        self.assert_name('mac-leopard', '10.6.3', 'mac-leopard')

        self.assert_name(None, '10.6.3', 'mac-snowleopard')
        self.assert_name('mac', '10.6.3', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', '10.4.3', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', '10.5.3', 'mac-snowleopard')
        self.assert_name('mac-snowleopard', '10.6.3', 'mac-snowleopard')

        self.assert_name(None, '10.7', 'mac-lion')
        self.assert_name(None, '10.7.3', 'mac-lion')
        self.assert_name(None, '10.8', 'mac-future')
        self.assert_name('mac', '10.7.3', 'mac-lion')

        self.assertRaises(AssertionError, self.assert_name, None, '10.3.1', 'should-raise-assertion-so-this-value-does-not-matter')

    def _assert_search_path(self, search_paths, version, use_webkit2=False):
        # FIXME: Port constructors should not "parse" the port name, but
        # rather be passed components (directly or via setters).  Once
        # we fix that, this method will need a re-write.
        port = MacPort('mac-%s' % version, options=MockOptions(webkit_test_runner=use_webkit2), filesystem=MockFileSystem(), user=MockUser(), executive=MockExecutive())
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
        port = MacPort(filesystem=MockFileSystem(), user=MockUser(), executive=MockExecutive())
        # Delay setting a should_log executive to avoid logging from MacPort.__init__.
        port._executive = MockExecutive(should_log=True)
        expected_stderr = "MOCK run_command: ['Tools/Scripts/run-safari', '--release', '-NSOpen', 'test.html'], cwd=/mock-checkout\n"
        OutputCapture().assert_outputs(self, port.show_results_html_file, ["test.html"], expected_stderr=expected_stderr)


if __name__ == '__main__':
    port_testcase.main()
