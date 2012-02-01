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

import sys
import unittest

from webkitpy.common import newstringio
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.layout_tests.port import mock_drt
from webkitpy.layout_tests.port import port_testcase
from webkitpy.layout_tests.port import test
from webkitpy.layout_tests.port.factory import PortFactory
from webkitpy.tool import mocktool


mock_options = mocktool.MockOptions(configuration='Release')


class MockDRTPortTest(port_testcase.PortTestCase):

    def make_port(self, options=mock_options):
        host = MockSystemHost()
        test.add_unit_tests_to_mock_filesystem(host.filesystem)
        if sys.platform == 'win32':
            # We use this because the 'win' port doesn't work yet.
            host.platform.os_name = 'win'
            host.platform.os_version = 'xp'
            return mock_drt.MockDRTPort(host, port_name='mock-chromium-win', options=options)
        return mock_drt.MockDRTPort(host, port_name='mock-mac', options=options)

    def test_default_worker_model(self):
        # only overridding the default test; we don't care about this one.
        pass

    def test_port_name_in_constructor(self):
        self.assertTrue(mock_drt.MockDRTPort(MockSystemHost(), port_name='mock-test'))

    def test_check_sys_deps(self):
        pass

    def test_uses_apache(self):
        pass

    def integration_test_http_lock(self):
        pass

    def integration_test_start_helper(self):
        pass

    def integration_test_http_server__normal(self):
        pass

    def integration_test_http_server__fails(self):
        pass

    def integration_test_websocket_server__normal(self):
        pass

    def integration_test_websocket_server__fails(self):
        pass

    def integration_test_helper(self):
        pass


class MockDRTTest(unittest.TestCase):
    def input_line(self, port, test_name, checksum=None):
        url = port.create_driver(0).test_to_uri(test_name)
        if url.startswith('file://'):
            if sys.platform == 'win32':
                url = url[len('file:///'):]
            else:
                url = url[len('file://'):]

        if checksum:
            return url + "'" + checksum + '\n'
        return url + '\n'

    def extra_args(self, pixel_tests):
        if pixel_tests:
            return ['--pixel-tests', '-']
        return ['-']

    def make_drt(self, options, args, host, stdin, stdout, stderr):
        return mock_drt.MockDRT(options, args, host, stdin, stdout, stderr)

    def make_input_output(self, port, test_name, pixel_tests,
                          expected_checksum, drt_output, drt_input=None):
        if pixel_tests:
            if not expected_checksum:
                expected_checksum = port.expected_checksum(test_name)
        if not drt_input:
            drt_input = self.input_line(port, test_name, expected_checksum)
        text_output = port.expected_text(test_name)

        if not drt_output:
            drt_output = self.expected_output(port, test_name, pixel_tests,
                                              text_output, expected_checksum)
        return (drt_input, drt_output)

    def expected_output(self, port, test_name, pixel_tests, text_output, expected_checksum):
        if pixel_tests and expected_checksum:
            return ['Content-Type: text/plain\n',
                    text_output,
                    '#EOF\n',
                    '\n',
                    'ActualHash: %s\n' % expected_checksum,
                    'ExpectedHash: %s\n' % expected_checksum,
                    '#EOF\n']
        else:
            return ['Content-Type: text/plain\n',
                    text_output,
                    '#EOF\n',
                    '#EOF\n']

    def assertTest(self, test_name, pixel_tests, expected_checksum=None, drt_output=None, host=None):
        port_name = 'test'
        host = host or MockSystemHost()
        test.add_unit_tests_to_mock_filesystem(host.filesystem)
        port = PortFactory(host).get(port_name)
        drt_input, drt_output = self.make_input_output(port, test_name,
            pixel_tests, expected_checksum, drt_output)

        args = ['--platform', port_name] + self.extra_args(pixel_tests)
        stdin = newstringio.StringIO(drt_input)
        stdout = newstringio.StringIO()
        stderr = newstringio.StringIO()
        options, args = mock_drt.parse_options(args)

        drt = self.make_drt(options, args, host, stdin, stdout, stderr)
        res = drt.run()

        self.assertEqual(res, 0)

        # We use the StringIO.buflist here instead of getvalue() because
        # the StringIO might be a mix of unicode/ascii and 8-bit strings.
        self.assertEqual(stdout.buflist, drt_output)
        self.assertEqual(stderr.getvalue(), '' if options.chromium else '#EOF\n')

    def test_main(self):
        host = MockSystemHost()
        test.add_unit_tests_to_mock_filesystem(host.filesystem)
        stdin = newstringio.StringIO()
        stdout = newstringio.StringIO()
        stderr = newstringio.StringIO()
        res = mock_drt.main(['--platform', 'test'] + self.extra_args(False),
                            host, stdin, stdout, stderr)
        self.assertEqual(res, 0)
        self.assertEqual(stdout.getvalue(), '')
        self.assertEqual(stderr.getvalue(), '')
        self.assertEqual(host.filesystem.written_files, {})

    def test_pixeltest_passes(self):
        # This also tests that we handle HTTP: test URLs properly.
        self.assertTest('http/tests/passes/text.html', True)

    def test_pixeltest__fails(self):
        self.assertTest('failures/expected/checksum.html', pixel_tests=True,
            expected_checksum='wrong-checksum',
            drt_output=['Content-Type: text/plain\n',
                        'checksum-txt',
                        '#EOF\n',
                        '\n',
                        'ActualHash: checksum-checksum\n',
                        'ExpectedHash: wrong-checksum\n',
                        'Content-Type: image/png\n',
                        'Content-Length: 43\n',
                        'checksum\x8a-pngtEXtchecksum\x00checksum-checksum',
                        '#EOF\n'])

    def test_textonly(self):
        self.assertTest('passes/image.html', False)

    def test_checksum_in_png(self):
        self.assertTest('passes/checksum_in_image.html', True)


class MockChromiumDRTTest(MockDRTTest):
    def extra_args(self, pixel_tests):
        if pixel_tests:
            return ['--pixel-tests=/tmp/png_result0.png']
        return []

    def make_drt(self, options, args, host, stdin, stdout, stderr):
        options.chromium = True

        # We have to set these by hand because --platform test won't trigger
        # the Chromium code paths.
        options.pixel_path = '/tmp/png_result0.png'
        options.pixel_tests = True

        return mock_drt.MockChromiumDRT(options, args, host, stdin, stdout, stderr)

    def input_line(self, port, test_name, checksum=None):
        url = port.create_driver(0).test_to_uri(test_name)
        if checksum:
            return url + ' 6000 ' + checksum + '\n'
        return url + ' 6000\n'

    def expected_output(self, port, test_name, pixel_tests, text_output, expected_checksum):
        url = port.create_driver(0).test_to_uri(test_name)
        if pixel_tests and expected_checksum:
            return ['#URL:%s\n' % url,
                    '#MD5:%s\n' % expected_checksum,
                    text_output,
                    '\n',
                    '#EOF\n']
        else:
            return ['#URL:%s\n' % url,
                    text_output,
                    '\n',
                    '#EOF\n']

    def test_pixeltest__fails(self):
        host = MockSystemHost()
        url = '#URL:file://'
        if sys.platform == 'win32':
            host = MockSystemHost(os_name='win', os_version='xp')
            url = '#URL:file:///'
        url = url + '%s/failures/expected/checksum.html' % PortFactory(host).get('test').layout_tests_dir()
        self.assertTest('failures/expected/checksum.html', pixel_tests=True,
            expected_checksum='wrong-checksum',
            drt_output=[url + '\n',
                        '#MD5:checksum-checksum\n',
                        'checksum-txt',
                        '\n',
                        '#EOF\n'],
            host=host)
        self.assertEquals(host.filesystem.written_files,
            {'/tmp/png_result0.png': 'checksum\x8a-pngtEXtchecksum\x00checksum-checksum'})

    def test_chromium_parse_options(self):
        options, args = mock_drt.parse_options(['--platform', 'chromium-mac',
            '--pixel-tests=/tmp/png_result0.png'])
        self.assertTrue(options.chromium)
        self.assertTrue(options.pixel_tests)
        self.assertEquals(options.pixel_path, '/tmp/png_result0.png')


if __name__ == '__main__':
    port_testcase.main()
