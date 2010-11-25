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

import os
import unittest
import StringIO

from webkitpy.tool import mocktool
from webkitpy.thirdparty.mock import Mock

import chromium
import chromium_linux
import chromium_mac
import chromium_win

class ChromiumDriverTest(unittest.TestCase):

    def setUp(self):
        mock_port = Mock()
        mock_port.get_option = lambda option_name: ''
        self.driver = chromium.ChromiumDriver(mock_port, worker_number=0)

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


class ChromiumPortTest(unittest.TestCase):
    class TestMacPort(chromium_mac.ChromiumMacPort):
        def __init__(self, options):
            chromium_mac.ChromiumMacPort.__init__(self,
                                                  port_name='test-port',
                                                  options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    class TestLinuxPort(chromium_linux.ChromiumLinuxPort):
        def __init__(self, options):
            chromium_linux.ChromiumLinuxPort.__init__(self,
                                                      port_name='test-port',
                                                      options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    def test_path_to_image_diff(self):
        mock_options = mocktool.MockOptions(use_drt=True)
        port = ChromiumPortTest.TestLinuxPort(options=mock_options)
        self.assertTrue(port._path_to_image_diff().endswith(
            '/out/default/ImageDiff'), msg=port._path_to_image_diff())
        port = ChromiumPortTest.TestMacPort(options=mock_options)
        self.assertTrue(port._path_to_image_diff().endswith(
            '/xcodebuild/default/ImageDiff'))
        # FIXME: Figure out how this is going to work on Windows.
        #port = chromium_win.ChromiumWinPort('test-port', options=MockOptions())

    def test_skipped_layout_tests(self):
        mock_options = mocktool.MockOptions(use_drt=True)
        port = ChromiumPortTest.TestLinuxPort(options=mock_options)

        fake_test = os.path.join(port.layout_tests_dir(), "fast/js/not-good.js")

        port.test_expectations = lambda: """BUG_TEST SKIP : fast/js/not-good.js = TEXT
LINUX WIN : fast/js/very-good.js = TIMEOUT PASS"""
        port.test_expectations_overrides = lambda: ''
        port.tests = lambda paths: set()
        port.path_exists = lambda test: True

        skipped_tests = port.skipped_layout_tests(extra_test_files=[fake_test, ])
        self.assertTrue("fast/js/not-good.js" in skipped_tests)

    def test_default_configuration(self):
        mock_options = mocktool.MockOptions()
        port = ChromiumPortTest.TestLinuxPort(options=mock_options)
        self.assertEquals(mock_options.configuration, 'default')
        self.assertTrue(port.default_configuration_called)

        mock_options = mocktool.MockOptions(configuration=None)
        port = ChromiumPortTest.TestLinuxPort(mock_options)
        self.assertEquals(mock_options.configuration, 'default')
        self.assertTrue(port.default_configuration_called)

    def test_diff_image(self):
        class TestPort(ChromiumPortTest.TestLinuxPort):
            def _path_to_image_diff(self):
                return "/path/to/image_diff"

        class MockExecute:
            def __init__(self, result):
                self._result = result

            def run_command(self,
                            args,
                            cwd=None,
                            input=None,
                            error_handler=None,
                            return_exit_code=False,
                            return_stderr=True,
                            decode_output=False):
                if return_exit_code:
                    return self._result
                return ''

        mock_options = mocktool.MockOptions(use_drt=False)
        port = ChromiumPortTest.TestLinuxPort(mock_options)

        # Images are different.
        port._executive = MockExecute(0)
        self.assertEquals(False, port.diff_image("EXPECTED", "ACTUAL"))

        # Images are the same.
        port._executive = MockExecute(1)
        self.assertEquals(True, port.diff_image("EXPECTED", "ACTUAL"))

        # There was some error running image_diff.
        port._executive = MockExecute(2)
        exception_raised = False
        try:
            port.diff_image("EXPECTED", "ACTUAL")
        except ValueError, e:
            exception_raised = True
        self.assertFalse(exception_raised)

if __name__ == '__main__':
    unittest.main()
