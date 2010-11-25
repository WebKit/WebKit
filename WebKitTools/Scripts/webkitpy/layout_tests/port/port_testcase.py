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

"""Unit testing base class for Port implementations."""

import os
import tempfile
import unittest

from webkitpy.tool import mocktool
mock_options = mocktool.MockOptions(results_directory='layout-test-results',
                                    use_apache=True,
                                    configuration='Release')

# FIXME: This should be used for all ports, not just WebKit Mac. See
# https://bugs.webkit.org/show_bug.cgi?id=50043 .

class PortTestCase(unittest.TestCase):
    """Tests the WebKit port implementation."""
    def make_port(self, options=mock_options):
        """Override in subclass."""
        raise NotImplementedError()

    def test_driver_cmd_line(self):
        port = self.make_port()
        if not port:
            return
        self.assertTrue(len(port.driver_cmd_line()))

    def test_http_server(self):
        port = self.make_port()
        if not port:
            return
        port.start_http_server()
        port.stop_http_server()

    def test_image_diff(self):
        port = self.make_port()
        if not port:
            return

        # FIXME: not sure why this shouldn't always be True
        #self.assertTrue(port.check_image_diff())
        if not port.check_image_diff():
            return

        dir = port.layout_tests_dir()
        file1 = os.path.join(dir, 'fast', 'css', 'button_center.png')
        fh1 = file(file1)
        contents1 = fh1.read()
        file2 = os.path.join(dir, 'fast', 'css',
                             'remove-shorthand-expected.png')
        fh2 = file(file2)
        contents2 = fh2.read()
        tmpfile = tempfile.mktemp()

        self.assertFalse(port.diff_image(contents1, contents1))
        self.assertTrue(port.diff_image(contents1, contents2))

        self.assertTrue(port.diff_image(contents1, contents2, tmpfile))
        fh1.close()
        fh2.close()
        # FIXME: this may not be being written?
        # self.assertTrue(os.path.exists(tmpfile))
        # os.remove(tmpfile)

    def test_websocket_server(self):
        port = self.make_port()
        if not port:
            return
        port.start_websocket_server()
        port.stop_websocket_server()
