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
#     * Neither the Google name nor the names of its
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

"""
This is an implementation of the Port interface that overrides other
ports and changes the Driver binary to "MockDRT".
"""

import logging
import optparse
import os
import sys

from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.port import factory

_log = logging.getLogger(__name__)


class MockDRTPort(object):
    """MockPort implementation of the Port interface."""

    def __init__(self, **kwargs):
        prefix = 'mock-'
        if 'port_name' in kwargs:
            kwargs['port_name'] = kwargs['port_name'][len(prefix):]
        self.__delegate = factory.get(**kwargs)

    def __getattr__(self, name):
        return getattr(self.__delegate, name)

    def acquire_http_lock(self):
        pass

    def release_http_lock(self):
        pass

    def check_build(self, needs_http):
        return True

    def check_sys_deps(self, needs_http):
        return True

    def driver_cmd_line(self):
        driver = self.create_driver(0)
        return driver.cmd_line()

    def _path_to_driver(self):
        return os.path.abspath(__file__)

    def create_driver(self, worker_number):
        # We need to create a driver object as the delegate would, but
        # overwrite the path to the driver binary in its command line. We do
        # this by actually overwriting its cmd_line() method with a proxy
        # method that splices in the mock_drt path and command line arguments
        # in place of the actual path to the driver binary.

        # FIXME: This doesn't yet work for Chromium test_shell ports.
        def overriding_cmd_line():
            cmd = self.__original_driver_cmd_line()
            index = cmd.index(self.__delegate._path_to_driver())
            cmd[index:index + 1] = [sys.executable, self._path_to_driver(),
                                    '--platform', self.name()]
            return cmd

        delegated_driver = self.__delegate.create_driver(worker_number)
        self.__original_driver_cmd_line = delegated_driver.cmd_line
        delegated_driver.cmd_line = overriding_cmd_line
        return delegated_driver

    def start_helper(self):
        pass

    def start_http_server(self):
        pass

    def start_websocket_server(self):
        pass

    def stop_helper(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass


def main(argv, stdin, stdout, stderr):
    """Run the tests."""

    options, args = parse_options(argv)
    drt = MockDRT(options, args, stdin, stdout, stderr)
    return drt.run()


def parse_options(argv):
    # FIXME: We need to figure out how to handle variants that have
    # different command-line conventions.
    option_list = [
        optparse.make_option('--platform', action='store',
                             help='platform to emulate'),
        optparse.make_option('--layout-tests', action='store_true',
                             default=True, help='run layout tests'),
        optparse.make_option('--pixel-tests', action='store_true',
                             default=False,
                             help='output image for pixel tests'),
    ]
    option_parser = optparse.OptionParser(option_list=option_list)
    return option_parser.parse_args(argv)


class MockDRT(object):
    def __init__(self, options, args, stdin, stdout, stderr):
        self._options = options
        self._args = args
        self._stdout = stdout
        self._stdin = stdin
        self._stderr = stderr

        port_name = None
        if options.platform:
            port_name = options.platform
        self._port = factory.get(port_name, options=options)

    def run(self):
        while True:
            line = self._stdin.readline()
            if not line:
                break

            url, expected_checksum = self.parse_input(line)
            self.run_one_test(url, expected_checksum)
        return 0

    def parse_input(self, line):
        line = line.strip()
        if "'" in line:
            return line.split("'", 1)
        return (line, None)

    def raw_bytes(self, unicode_str):
        return unicode_str.encode('utf-8')

    def run_one_test(self, url, expected_checksum):
        port = self._port
        if url.startswith('http'):
            test_name = port.uri_to_test_name(url)
            test_path = port._filesystem.join(port.layout_tests_dir(), test_name)
        else:
            test_path = url

        actual_text_bytes = self.raw_bytes(port.expected_text(test_path))
        if self._options.pixel_tests and expected_checksum:
            actual_checksum_bytes = self.raw_bytes(port.expected_checksum(test_path))
            actual_image_bytes = port.expected_image(test_path)

        self._stdout.write('Content-Type: text/plain\n')
        self._stdout.write(actual_text_bytes)
        self._stdout.write('#EOF\n')

        if self._options.pixel_tests and expected_checksum:
            expected_checksum_bytes = self.raw_bytes(expected_checksum)
            self._stdout.write('\n')
            self._stdout.write('ActualHash: %s\n' % actual_checksum_bytes)
            self._stdout.write('ExpectedHash: %s\n' % expected_checksum_bytes)
            if actual_checksum_bytes != expected_checksum_bytes:
                self._stdout.write('Content-Type: image/png\n')
                self._stdout.write('Content-Length: %s\n\n' % len(actual_image_bytes))
                self._stdout.write(actual_image_bytes)
        self._stdout.write('#EOF\n')
        self._stdout.flush()
        self._stderr.flush()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], sys.stdin, sys.stdout, sys.stderr))
