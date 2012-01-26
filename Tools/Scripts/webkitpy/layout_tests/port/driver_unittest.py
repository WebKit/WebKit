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

import sys
import unittest

from webkitpy.common.system.path import abspath_to_uri
from webkitpy.common.system.systemhost_mock import MockSystemHost

from webkitpy.layout_tests.port import Port, Driver, DriverOutput


class DriverOutputTest(unittest.TestCase):
    def test_strip_metrics(self):
        patterns = [
            ('RenderView at (0,0) size 800x600', 'RenderView '),
            ('text run at (0,0) width 100: "some text"', '"some text"'),
            ('RenderBlock {HTML} at (0,0) size 800x600', 'RenderBlock {HTML} '),
            ('RenderBlock {INPUT} at (29,3) size 12x12 [color=#000000]', 'RenderBlock {INPUT}'),

            ('RenderBlock (floating) {DT} at (5,5) size 79x310 [border: (5px solid #000000)]',
            'RenderBlock (floating) {DT} [border: px solid #000000)]'),

            ('\n    "truncate text    "\n', '\n    "truncate text"\n'),

            ('RenderText {#text} at (0,3) size 41x12\n    text run at (0,3) width 41: "whimper "\n',
            'RenderText {#text} \n    "whimper"\n'),

            ("""text run at (0,0) width 109: ".one {color: green;}"
          text run at (109,0) width 0: " "
          text run at (0,17) width 81: ".1 {color: red;}"
          text run at (81,17) width 0: " "
          text run at (0,34) width 102: ".a1 {color: green;}"
          text run at (102,34) width 0: " "
          text run at (0,51) width 120: "P.two {color: purple;}"
          text run at (120,51) width 0: " "\n""",
            '".one {color: green;}  .1 {color: red;}  .a1 {color: green;}  P.two {color: purple;}"\n'),

            ('text-- other text', 'text--other text'),

            (' some output   "truncate trailing spaces at end of line after text"   \n',
            ' some output   "truncate trailing spaces at end of line after text"\n'),

            (r'scrollWidth 120', r'scrollWidth'),
            (r'scrollHeight 120', r'scrollHeight'),
        ]

        for pattern in patterns:
            driver_output = DriverOutput(pattern[0], None, None, None)
            driver_output.strip_metrics()
            self.assertEqual(driver_output.text, pattern[1])


class DriverTest(unittest.TestCase):
    def make_port(self):
        return Port(MockSystemHost())

    def assertVirtual(self, method, *args, **kwargs):
        self.assertRaises(NotImplementedError, method, *args, **kwargs)

    def _assert_wrapper(self, wrapper_string, expected_wrapper):
        wrapper = Driver(self.make_port(), None, pixel_tests=False)._command_wrapper(wrapper_string)
        self.assertEqual(wrapper, expected_wrapper)

    def test_virtual_driver_methods(self):
        driver = Driver(self.make_port(), None, pixel_tests=False)
        self.assertVirtual(driver.run_test, None)
        self.assertVirtual(driver.stop)
        self.assertVirtual(driver.cmd_line)

    def test_command_wrapper(self):
        self._assert_wrapper(None, [])
        self._assert_wrapper("valgrind", ["valgrind"])

        # Validate that shlex works as expected.
        command_with_spaces = "valgrind --smc-check=\"check with spaces!\" --foo"
        expected_parse = ["valgrind", "--smc-check=check with spaces!", "--foo"]
        self._assert_wrapper(command_with_spaces, expected_parse)

    def test_test_to_uri(self):
        port = self.make_port()
        driver = Driver(port, None, pixel_tests=False)
        if sys.platform in ('cygwin', 'win32'):
            self.assertEqual(driver.test_to_uri('foo/bar.html'), 'file:///%s/foo/bar.html' % port.layout_tests_dir())
        else:
            self.assertEqual(driver.test_to_uri('foo/bar.html'), 'file://%s/foo/bar.html' % port.layout_tests_dir())
        self.assertEqual(driver.test_to_uri('http/tests/foo.html'), 'http://127.0.0.1:8000/foo.html')
        self.assertEqual(driver.test_to_uri('http/tests/ssl/bar.html'), 'https://127.0.0.1:8443/ssl/bar.html')

    def test_uri_to_test(self):
        port = self.make_port()
        driver = Driver(port, None, pixel_tests=False)
        if sys.platform in ('cygwin', 'win32'):
            self.assertEqual(driver.uri_to_test('file:///%s/foo/bar.html' % port.layout_tests_dir()), 'foo/bar.html')
        else:
            self.assertEqual(driver.uri_to_test('file://%s/foo/bar.html' % port.layout_tests_dir()), 'foo/bar.html')
        self.assertEqual(driver.uri_to_test('http://127.0.0.1:8000/foo.html'), 'http/tests/foo.html')
        self.assertEqual(driver.uri_to_test('https://127.0.0.1:8443/ssl/bar.html'), 'http/tests/ssl/bar.html')


if __name__ == '__main__':
    unittest.main()
