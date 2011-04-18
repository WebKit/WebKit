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

import sys
import unittest

# Handle Python < 2.6 where multiprocessing isn't available.
try:
    import multiprocessing
except ImportError:
    multiprocessing = None

from webkitpy.tool import mocktool
mock_options = mocktool.MockOptions(use_apache=True,
                                    configuration='Release')

# FIXME: This should be used for all ports, not just WebKit Mac. See
# https://bugs.webkit.org/show_bug.cgi?id=50043 .

class PortTestCase(unittest.TestCase):
    """Tests the WebKit port implementation."""
    def port_maker(self, platform):
        """Override to return the class object of the port to be tested,
        or None if a valid port object cannot be constructed on the specified
        platform."""
        raise NotImplementedError()

    def make_port(self, options=mock_options):
        """This routine should be used for tests that should only be run
        when we can create a full, valid port object."""
        maker = self.port_maker(sys.platform)
        if not maker:
            return None

        return maker(options=options)

    def test_default_worker_model(self):
        port = self.make_port()
        if not port:
            return

        if multiprocessing:
            self.assertEqual(port.default_worker_model(), 'processes')
        else:
            self.assertEqual(port.default_worker_model(), 'threads')

    def test_driver_cmd_line(self):
        port = self.make_port()
        if not port:
            return
        self.assertTrue(len(port.driver_cmd_line()))

        options = mocktool.MockOptions(additional_drt_flag=['--foo=bar', '--foo=baz'])
        port = self.make_port(options=options)
        cmd_line = port.driver_cmd_line()
        self.assertTrue('--foo=bar' in cmd_line)
        self.assertTrue('--foo=baz' in cmd_line)

    def disabled_test_http_server(self):
        port = self.make_port()
        if not port:
            return
        port.start_http_server()
        port.stop_http_server()

    def disabled_test_image_diff(self):
        port = self.make_port()
        if not port:
            return

        if not port.check_image_diff():
            # The port hasn't been built - don't run the tests.
            return

        dir = port.layout_tests_dir()
        file1 = port._filesystem.join(dir, 'fast', 'css', 'button_center.png')
        contents1 = port._filesystem.read_binary_file(file1)
        file2 = port._filesystem.join(dir, 'fast', 'css',
                                      'remove-shorthand-expected.png')
        contents2 = port._filesystem.read_binary_file(file2)
        tmpfd, tmpfile = port._filesystem.open_binary_tempfile('')
        tmpfd.close()

        self.assertFalse(port.diff_image(contents1, contents1))
        self.assertTrue(port.diff_image(contents1, contents2))

        self.assertTrue(port.diff_image(contents1, contents2, tmpfile))

        port._filesystem.remove(tmpfile)

    def test_diff_image__missing_both(self):
        port = self.make_port()
        if not port:
            return
        self.assertFalse(port.diff_image(None, None, None))
        self.assertFalse(port.diff_image(None, '', None))
        self.assertFalse(port.diff_image('', None, None))
        self.assertFalse(port.diff_image('', '', None))

    def test_diff_image__missing_actual(self):
        port = self.make_port()
        if not port:
            return
        self.assertTrue(port.diff_image(None, 'foo', None))
        self.assertTrue(port.diff_image('', 'foo', None))

    def test_diff_image__missing_expected(self):
        port = self.make_port()
        if not port:
            return
        self.assertTrue(port.diff_image('foo', None, None))
        self.assertTrue(port.diff_image('foo', '', None))


    def disabled_test_websocket_server(self):
        port = self.make_port()
        if not port:
            return
        port.start_websocket_server()
        port.stop_websocket_server()

    def test_test_configuration(self):
        port = self.make_port()
        if not port:
            return
        self.assertTrue(port.test_configuration())

    def test_all_test_configurations(self):
        port = self.make_port()
        if not port:
            return
        self.assertTrue(len(port.all_test_configurations()) > 0)

    def test_baseline_search_path(self):
        port = self.make_port()
        if not port:
            return
        self.assertTrue(port.baseline_path() in port.baseline_search_path())
