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
import unittest

import mac
import port_testcase


class MacTest(port_testcase.PortTestCase):
    def make_port(self, options=port_testcase.MockOptions()):
        port_obj = mac.MacPort(options=options)
        port_obj._options.results_directory = port_obj.results_directory()
        port_obj._options.configuration = 'Release'
        return port_obj

    def test_skipped_file_paths(self):
        port = self.make_port()
        skipped_paths = port._skipped_file_paths()
        # FIXME: _skipped_file_paths should return WebKit-relative paths.
        # So to make it unit testable, we strip the WebKit directory from the path.
        relative_paths = [path[len(port.path_from_webkit_base()):] for path in skipped_paths]
        self.assertEqual(relative_paths, ['LayoutTests/platform/mac-leopard/Skipped', 'LayoutTests/platform/mac/Skipped'])

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

    def test_skipped_file_paths(self):
        port = self.make_port()
        skipped_file = StringIO.StringIO(self.example_skipped_file)
        self.assertEqual(port._tests_from_skipped_file(skipped_file), self.example_skipped_tests)


if __name__ == '__main__':
    unittest.main()
