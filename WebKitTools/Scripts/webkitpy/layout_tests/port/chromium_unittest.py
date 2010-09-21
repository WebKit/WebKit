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

import chromium
import chromium_linux
import chromium_mac
import chromium_win
import unittest
import StringIO
import os

from webkitpy.thirdparty.mock import Mock


class ChromiumDriverTest(unittest.TestCase):

    def setUp(self):
        mock_port = Mock()
        # FIXME: The Driver should not be grabbing at port._options!
        mock_port._options = Mock()
        mock_port._options.wrapper = ""
        self.driver = chromium.ChromiumDriver(mock_port, image_path=None, options=None)

    def test_test_shell_command(self):
        expected_command = "test.html 2 checksum\n"
        self.assertEqual(self.driver._test_shell_command("test.html", 2, "checksum"), expected_command)

    def _assert_write_command_and_read_line(self, input=None, expected_line=None, expected_stdin=None, expected_crash=False):
        if not expected_stdin:
            if input:
                expected_stdin = input
            else:
                # We reset stdin, so we should expect stdin.getValue = ""
                expected_stdin = ""
        self.driver._proc.stdin = StringIO.StringIO()
        line, did_crash = self.driver._write_command_and_read_line(input)
        self.assertEqual(self.driver._proc.stdin.getvalue(), expected_stdin)
        self.assertEqual(line, expected_line)
        self.assertEqual(did_crash, expected_crash)

    def test_write_command_and_read_line(self):
        self.driver._proc = Mock()
        # Set up to read 3 lines before we get an IOError
        self.driver._proc.stdout = StringIO.StringIO("first\nsecond\nthird\n")

        unicode_input = u"I \u2661 Unicode"
        utf8_input = unicode_input.encode("utf-8")
        # Test unicode input conversion to utf-8
        self._assert_write_command_and_read_line(input=unicode_input, expected_stdin=utf8_input, expected_line="first\n")
        # Test str() input.
        self._assert_write_command_and_read_line(input="foo", expected_line="second\n")
        # Test input=None
        self._assert_write_command_and_read_line(expected_line="third\n")
        # Test reading from a closed/empty stream.
        # reading from a StringIO does not raise IOError like a real file would, so raise IOError manually.
        def mock_readline():
            raise IOError
        self.driver._proc.stdout.readline = mock_readline
        self._assert_write_command_and_read_line(expected_crash=True)

    def test_path_to_image_diff(self):
        class MockOptions:
            def __init__(self):
                self.use_drt = True

        port = chromium_linux.ChromiumLinuxPort('test-port', options=MockOptions())
        self.assertTrue(port._path_to_image_diff().endswith(
            '/out/Release/ImageDiff'))
        port = chromium_mac.ChromiumMacPort('test-port', options=MockOptions())
        self.assertTrue(port._path_to_image_diff().endswith(
            '/xcodebuild/Release/ImageDiff'))
        # FIXME: Figure out how this is going to work on Windows.
        #port = chromium_win.ChromiumWinPort('test-port', options=MockOptions())

    def test_skipped_layout_tests(self):
        class MockOptions:
            def __init__(self):
                self.use_drt = True

        port = chromium_linux.ChromiumLinuxPort('test-port', options=MockOptions())

        fake_test = os.path.join(port.layout_tests_dir(), "fast/js/not-good.js")

        port.test_expectations = lambda: """BUG_TEST SKIP : fast/js/not-good.js = TEXT
DEFER LINUX WIN : fast/js/very-good.js = TIMEOUT PASS"""
        port.test_expectations_overrides = lambda: ''
        port.tests = lambda paths: set()
        port.path_exists = lambda test: True

        skipped_tests = port.skipped_layout_tests(extra_test_files=[fake_test, ])
        self.assertTrue("fast/js/not-good.js" in skipped_tests)

if __name__ == '__main__':
    unittest.main()
