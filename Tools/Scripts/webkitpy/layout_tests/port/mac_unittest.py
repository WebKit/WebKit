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

from webkitpy.layout_tests.port import mac
from webkitpy.layout_tests.port import port_testcase


class MacTest(port_testcase.PortTestCase):
    def port_maker(self, platform):
        if platform != 'darwin':
            return None
        return mac.MacPort

    def assert_skipped_files_for_version(self, port_name, expected_paths):
        port = mac.MacPort(port_name=port_name)
        skipped_paths = port._skipped_file_paths()
        # FIXME: _skipped_file_paths should return WebKit-relative paths.
        # So to make it unit testable, we strip the WebKit directory from the path.
        relative_paths = [path[len(port.path_from_webkit_base()):] for path in skipped_paths]
        self.assertEqual(relative_paths, expected_paths)

    def test_skipped_file_paths(self):
        # We skip this on win32 because we use '/' as the dir separator and it's
        # not worth making platform-independent.
        if sys.platform == 'win32':
            return None

        self.assert_skipped_files_for_version('mac-snowleopard',
            ['/LayoutTests/platform/mac-snowleopard/Skipped', '/LayoutTests/platform/mac/Skipped'])
        self.assert_skipped_files_for_version('mac-leopard',
            ['/LayoutTests/platform/mac-leopard/Skipped', '/LayoutTests/platform/mac/Skipped'])

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
        port = mac.MacPort()
        self.assertEqual(port._tests_from_skipped_file_contents(self.example_skipped_file), self.example_skipped_tests)

    def assert_name(self, port_name, os_version_string, expected):
        port = mac.MacPort(port_name=port_name,
                           os_version_string=os_version_string)
        self.assertEquals(expected, port.name())

    def test_tests_for_other_platforms(self):
        port = mac.MacPort(port_name='mac-snowleopard')
        dirs_to_skip = port._tests_for_other_platforms()
        self.assertTrue('platform/chromium-linux' in dirs_to_skip)
        self.assertTrue('platform/mac-tiger' in dirs_to_skip)
        self.assertFalse('platform/mac' in dirs_to_skip)
        self.assertFalse('platform/mac-snowleopard' in dirs_to_skip)

    def test_version(self):
        port = mac.MacPort()
        self.assertTrue(port.version())

    def test_versions(self):
        port = self.make_port()
        if port:
            self.assertTrue(port.name() in ('mac-tiger', 'mac-leopard', 'mac-snowleopard', 'mac-future'))

        self.assert_name(None, '10.4.8', 'mac-tiger')
        self.assert_name('mac', '10.4.8', 'mac-tiger')
        self.assert_name('mac-tiger', '10.4.8', 'mac-tiger')
        self.assert_name('mac-tiger', '10.5.3', 'mac-tiger')
        self.assert_name('mac-tiger', '10.6.3', 'mac-tiger')

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

        self.assert_name(None, '10.7', 'mac-future')
        self.assert_name(None, '10.7.3', 'mac-future')
        self.assert_name(None, '10.8', 'mac-future')
        self.assert_name('mac', '10.7.3', 'mac-future')
        self.assert_name('mac-future', '10.4.3', 'mac-future')
        self.assert_name('mac-future', '10.5.3', 'mac-future')
        self.assert_name('mac-future', '10.6.3', 'mac-future')
        self.assert_name('mac-future', '10.7.3', 'mac-future')

        self.assertRaises(AssertionError, self.assert_name, None, '10.3.1', 'should-raise-assertion-so-this-value-does-not-matter')


if __name__ == '__main__':
    unittest.main()
