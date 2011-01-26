#!/usr/bin/env python
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

"""Unit tests for MockDRT."""

import unittest

from webkitpy.common import newstringio

from webkitpy.layout_tests.port import mock_drt
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port import port_testcase


class MockDRTPortTest(port_testcase.PortTestCase):
    def make_port(self):
        return mock_drt.MockDRTPort()


class MockDRTTest(unittest.TestCase):
    def setUp(self):
        # We base our tests on the mac-snowleopard port.
        self._port = factory.get('mac-snowleopard')
        self._layout_tests_dir = self._port.layout_tests_dir()

    def to_path(self, test_name):
        return self._port._filesystem.join(self._layout_tests_dir, test_name)

    def input_line(self, test_name, checksum=None):
        url = self._port.filename_to_uri(self.to_path(test_name))
        if checksum:
            return url + "'" + checksum + '\n'
        return url + '\n'

    def make_drt(self, input_string, args=None, extra_args=None):
        args = args or ['--platform', 'mac-snowleopard', '-']
        extra_args = extra_args or []
        args += extra_args
        stdin = newstringio.StringIO(input_string)
        stdout = newstringio.StringIO()
        stderr = newstringio.StringIO()
        options, args = mock_drt.parse_options(args)
        drt = mock_drt.MockDRT(options, args, stdin, stdout, stderr)
        return (drt, stdout, stderr)

    def make_input_output(self, test_name, pixel_tests):
        path = self.to_path(test_name)
        expected_checksum = None
        if pixel_tests:
            expected_checksum = self._port.expected_checksum(path)
        drt_input = self.input_line(test_name, expected_checksum)
        text_output = self._port.expected_text(path)

        if pixel_tests:
            drt_output = (
                "Content-Type: text/plain\n"
                "%s#EOF\n"
                "\n"
                "ActualHash: %s\n"
                "ExpectedHash: %s\n"
                "#EOF\n") % (text_output, expected_checksum, expected_checksum)
        else:
            drt_output = (
                "Content-Type: text/plain\n"
                "%s#EOF\n"
                "#EOF\n") % text_output

        return (drt_input, drt_output)

    def assertTest(self, test_name, pixel_tests):
        drt_input, drt_output = self.make_input_output(test_name, pixel_tests)
        extra_args = []
        if pixel_tests:
            extra_args = ['--pixel-tests']
        drt, stdout, stderr = self.make_drt(drt_input, extra_args)
        res = drt.run()
        self.assertEqual(res, 0)
        self.assertEqual(stdout.getvalue(), drt_output)
        self.assertEqual(stderr.getvalue(), '')

    def test_pixeltest(self):
        self.assertTest('fast/html/keygen.html', True)

    def test_textonly(self):
        self.assertTest('fast/html/article-element.html', False)


if __name__ == '__main__':
    unittest.main()
