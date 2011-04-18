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

from webkitpy.layout_tests.port import mock_drt
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port import port_testcase
from webkitpy.layout_tests.port import test

from webkitpy.tool import mocktool
mock_options = mocktool.MockOptions(use_apache=True,
                                    configuration='Release')


class MockDRTPortTest(port_testcase.PortTestCase):
    def make_port(self, options=mock_options):
        if sys.platform == 'win32':
            # We use this because the 'win' port doesn't work yet.
            return mock_drt.MockDRTPort(port_name='mock-chromium-win', options=options)
        return mock_drt.MockDRTPort(options=options)

    def test_default_worker_model(self):
        # only overridding the default test; we don't care about this one.
        pass

    def test_port_name_in_constructor(self):
        self.assertTrue(mock_drt.MockDRTPort(port_name='mock-test'))

    def test_acquire_http_lock(self):
        # Only checking that no exception is raised.
        self.make_port().acquire_http_lock()

    def test_release_http_lock(self):
        # Only checking that no exception is raised.
        self.make_port().release_http_lock()

    def test_check_build(self):
        port = self.make_port()
        self.assertTrue(port.check_build(True))

    def test_check_sys_deps(self):
        port = self.make_port()
        self.assertTrue(port.check_sys_deps(True))

    def test_start_helper(self):
        # Only checking that no exception is raised.
        self.make_port().start_helper()

    def test_start_http_server(self):
        # Only checking that no exception is raised.
        self.make_port().start_http_server()

    def test_start_websocket_server(self):
        # Only checking that no exception is raised.
        self.make_port().start_websocket_server()

    def test_stop_helper(self):
        # Only checking that no exception is raised.
        self.make_port().stop_helper()

    def test_stop_http_server(self):
        # Only checking that no exception is raised.
        self.make_port().stop_http_server()

    def test_stop_websocket_server(self):
        # Only checking that no exception is raised.
        self.make_port().stop_websocket_server()


class MockDRTTest(unittest.TestCase):
    def to_path(self, port, test_name):
        return port._filesystem.join(port.layout_tests_dir(), test_name)

    def input_line(self, port, test_name, checksum=None):
        url = port.filename_to_uri(self.to_path(port, test_name))
        # FIXME: we shouldn't have to work around platform-specific issues
        # here.
        if url.startswith('file:////'):
            url = url[len('file:////') - 1:]
        if url.startswith('file:///'):
            url = url[len('file:///') - 1:]

        if checksum:
            return url + "'" + checksum + '\n'
        return url + '\n'

    def extra_args(self, pixel_tests):
        if pixel_tests:
            return ['--pixel-tests', '-']
        return ['-']

    def make_drt(self, options, args, filesystem, stdin, stdout, stderr):
        return mock_drt.MockDRT(options, args, filesystem, stdin, stdout, stderr)

    def make_input_output(self, port, test_name, pixel_tests,
                          expected_checksum, drt_output, drt_input=None):
        path = self.to_path(port, test_name)
        if pixel_tests:
            if not expected_checksum:
                expected_checksum = port.expected_checksum(path)
        if not drt_input:
            drt_input = self.input_line(port, test_name, expected_checksum)
        text_output = port.expected_text(path)

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

    def assertTest(self, test_name, pixel_tests, expected_checksum=None,
                   drt_output=None, filesystem=None):
        platform = 'test'
        filesystem = filesystem or test.unit_test_filesystem()
        port = factory.get(platform, filesystem=filesystem)
        drt_input, drt_output = self.make_input_output(port, test_name,
            pixel_tests, expected_checksum, drt_output)

        args = ['--platform', 'test'] + self.extra_args(pixel_tests)
        stdin = newstringio.StringIO(drt_input)
        stdout = newstringio.StringIO()
        stderr = newstringio.StringIO()
        options, args = mock_drt.parse_options(args)

        drt = self.make_drt(options, args, filesystem, stdin, stdout, stderr)
        res = drt.run()

        self.assertEqual(res, 0)

        # We use the StringIO.buflist here instead of getvalue() because
        # the StringIO might be a mix of unicode/ascii and 8-bit strings.
        self.assertEqual(stdout.buflist, drt_output)
        self.assertEqual(stderr.getvalue(), '')

    def test_main(self):
        filesystem = test.unit_test_filesystem()
        stdin = newstringio.StringIO()
        stdout = newstringio.StringIO()
        stderr = newstringio.StringIO()
        res = mock_drt.main(['--platform', 'test'] + self.extra_args(False),
                            filesystem, stdin, stdout, stderr)
        self.assertEqual(res, 0)
        self.assertEqual(stdout.getvalue(), '')
        self.assertEqual(stderr.getvalue(), '')
        self.assertEqual(filesystem.written_files, {})

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
                        'Content-Length: 13\n',
                        'checksum\x8a-png',
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

    def make_drt(self, options, args, filesystem, stdin, stdout, stderr):
        options.chromium = True

        # We have to set these by hand because --platform test won't trigger
        # the Chromium code paths.
        options.pixel_path = '/tmp/png_result0.png'
        options.pixel_tests = True

        return mock_drt.MockChromiumDRT(options, args, filesystem, stdin, stdout, stderr)

    def input_line(self, port, test_name, checksum=None):
        url = port.filename_to_uri(self.to_path(port, test_name))
        if checksum:
            return url + ' 6000 ' + checksum + '\n'
        return url + ' 6000\n'

    def expected_output(self, port, test_name, pixel_tests, text_output, expected_checksum):
        url = port.filename_to_uri(self.to_path(port, test_name))
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
        filesystem = test.unit_test_filesystem()
        self.assertTest('failures/expected/checksum.html', pixel_tests=True,
            expected_checksum='wrong-checksum',
            drt_output=['#URL:file:///test.checkout/LayoutTests/failures/expected/checksum.html\n',
                        '#MD5:checksum-checksum\n',
                        'checksum-txt',
                        '\n',
                        '#EOF\n'],
            filesystem=filesystem)
        self.assertEquals(filesystem.written_files,
            {'/tmp/png_result0.png': 'checksum\x8a-png'})

    def test_chromium_parse_options(self):
        options, args = mock_drt.parse_options(['--platform', 'chromium-mac',
            '--pixel-tests=/tmp/png_result0.png'])
        self.assertTrue(options.chromium)
        self.assertTrue(options.pixel_tests)
        self.assertEquals(options.pixel_path, '/tmp/png_result0.png')


if __name__ == '__main__':
    unittest.main()
