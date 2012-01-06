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
from webkitpy.common.host_mock import MockHost

from webkitpy.layout_tests.port import Port, Driver, DriverOutput


class DriverTest(unittest.TestCase):
    def make_port(self):
        return Port(MockHost())

    def _assert_wrapper(self, wrapper_string, expected_wrapper):
        wrapper = Driver(Port(MockHost()), None, pixel_tests=False)._command_wrapper(wrapper_string)
        self.assertEqual(wrapper, expected_wrapper)

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
