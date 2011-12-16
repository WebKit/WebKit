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

The MockDRT objects emulate what a real DRT would do. In particular, they
return the output a real DRT would return for a given test, assuming that
test actually passes (except for reftests, which currently cause the
MockDRT to crash).
"""

import base64
import logging
import sys

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.layout_tests.port.factory import PortFactory
from webkitpy.tool.mocktool import MockOptions

_log = logging.getLogger(__name__)


class MockDRTPort(object):
    """MockPort implementation of the Port interface."""

    def __init__(self, host, **kwargs):
        prefix = 'mock-'
        if 'port_name' in kwargs:
            kwargs['port_name'] = kwargs['port_name'][len(prefix):]
        self._host = host
        self.__delegate = PortFactory(host).get(**kwargs)
        self.__real_name = prefix + self.__delegate.name()

    def real_name(self):
        return self.__real_name

    def __getattr__(self, name):
        return getattr(self.__delegate, name)

    def check_build(self, needs_http):
        return True

    def check_sys_deps(self, needs_http):
        return True

    def driver_cmd_line(self):
        driver = self.create_driver(0)
        return driver.cmd_line()

    def _path_to_driver(self):
        return self._host.filesystem.abspath(__file__)

    def create_driver(self, worker_number):
        # We need to create a driver object as the delegate would, but
        # overwrite the path to the driver binary in its command line. We do
        # this by actually overwriting its cmd_line() method with a proxy
        # method that splices in the mock_drt path and command line arguments
        # in place of the actual path to the driver binary.

        def overriding_cmd_line():
            cmd = self.__original_driver_cmd_line()
            index = cmd.index(self.__delegate._path_to_driver())
            # FIXME: Why does this need to use sys.executable (instead of something mockable)?
            cmd[index:index + 1] = [sys.executable, self._path_to_driver(), '--platform', self.name()]
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

    def acquire_http_lock(self):
        pass

    def stop_helper(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def release_http_lock(self):
        pass


def main(argv, host, stdin, stdout, stderr):
    """Run the tests."""

    options, args = parse_options(argv)
    if options.chromium:
        drt = MockChromiumDRT(options, args, host, stdin, stdout, stderr)
    else:
        drt = MockDRT(options, args, host, stdin, stdout, stderr)
    return drt.run()


def parse_options(argv):
    # FIXME: We have to do custom arg parsing instead of using the optparse
    # module.  First, Chromium and non-Chromium DRTs have a different argument
    # syntax.  Chromium uses --pixel-tests=<path>, and non-Chromium uses
    # --pixel-tests as a boolean flag. Second, we don't want to have to list
    # every command line flag DRT accepts, but optparse complains about
    # unrecognized flags. At some point it might be good to share a common
    # DRT options class between this file and webkit.py and chromium.py
    # just to get better type checking.
    platform_index = argv.index('--platform')
    platform = argv[platform_index + 1]

    pixel_tests = False
    pixel_path = None
    chromium = False
    if platform.startswith('chromium'):
        chromium = True
        for arg in argv:
            if arg.startswith('--pixel-tests'):
                pixel_tests = True
                pixel_path = arg[len('--pixel-tests='):]
    else:
        pixel_tests = '--pixel-tests' in argv
    options = MockOptions(chromium=chromium, platform=platform, pixel_tests=pixel_tests, pixel_path=pixel_path)
    return (options, [])


# FIXME: Should probably change this to use DriverInput after
# https://bugs.webkit.org/show_bug.cgi?id=53004 lands (it's landed as of 2/3/11).
class _DRTInput(object):
    def __init__(self, line):
        vals = line.strip().split("'")
        if len(vals) == 1:
            self.uri = vals[0]
            self.checksum = None
        else:
            self.uri = vals[0]
            self.checksum = vals[1]


class MockDRT(object):
    def __init__(self, options, args, host, stdin, stdout, stderr):
        self._options = options
        self._args = args
        self._host = host
        self._stdout = stdout
        self._stdin = stdin
        self._stderr = stderr

        port_name = None
        if options.platform:
            port_name = options.platform
        self._port = PortFactory(host).get(port_name=port_name, options=options)

    def run(self):
        while True:
            line = self._stdin.readline()
            if not line:
                break
            self.run_one_test(self.parse_input(line))
        return 0

    def parse_input(self, line):
        return _DRTInput(line)

    def run_one_test(self, test_input):
        port = self._port
        if test_input.uri.startswith('http'):
            test_name = port.uri_to_test_name(test_input.uri)
        else:
            test_name = port.relative_test_filename(test_input.uri)

        actual_text = port.expected_text(test_name)
        actual_audio = port.expected_audio(test_name)
        if self._options.pixel_tests and test_input.checksum:
            actual_checksum = port.expected_checksum(test_name)
            actual_image = port.expected_image(test_name)

        if actual_audio:
            self._stdout.write('Content-Type: audio/wav\n')
            self._stdout.write('Content-Transfer-Encoding: base64\n')
            output = base64.b64encode(actual_audio)
            self._stdout.write(output)
        else:
            self._stdout.write('Content-Type: text/plain\n')
            # FIXME: Note that we don't ensure there is a trailing newline!
            # This mirrors actual (Mac) DRT behavior but is a bug.
            self._stdout.write(actual_text)

        self._stdout.write('#EOF\n')

        if self._options.pixel_tests and test_input.checksum:
            self._stdout.write('\n')
            self._stdout.write('ActualHash: %s\n' % actual_checksum)
            self._stdout.write('ExpectedHash: %s\n' % test_input.checksum)
            if actual_checksum != test_input.checksum:
                self._stdout.write('Content-Type: image/png\n')
                self._stdout.write('Content-Length: %s\n' % len(actual_image))
                self._stdout.write(actual_image)
        self._stdout.write('#EOF\n')
        self._stdout.flush()
        self._stderr.flush()


# FIXME: Should probably change this to use DriverInput after
# https://bugs.webkit.org/show_bug.cgi?id=53004 lands.
class _ChromiumDRTInput(_DRTInput):
    def __init__(self, line):
        vals = line.strip().split()
        if len(vals) == 3:
            self.uri, self.timeout, self.checksum = vals
        else:
            self.uri = vals[0]
            self.timeout = vals[1]
            self.checksum = None


class MockChromiumDRT(MockDRT):
    def parse_input(self, line):
        return _ChromiumDRTInput(line)

    def run_one_test(self, test_input):
        port = self._port
        test_name = self._port.uri_to_test_name(test_input.uri)

        actual_text = port.expected_text(test_name)
        actual_image = ''
        actual_checksum = ''
        if self._options.pixel_tests and test_input.checksum:
            actual_checksum = port.expected_checksum(test_name)
            if actual_checksum != test_input.checksum:
                actual_image = port.expected_image(test_name)

        self._stdout.write("#URL:%s\n" % test_input.uri)
        if self._options.pixel_tests and test_input.checksum:
            self._stdout.write("#MD5:%s\n" % actual_checksum)
            self._host.filesystem.write_binary_file(self._options.pixel_path,
                                               actual_image)
        self._stdout.write(actual_text)

        # FIXME: (See above FIXME as well). Chromium DRT appears to always
        # ensure the text output has a trailing newline. Mac DRT does not.
        if not actual_text.endswith('\n'):
            self._stdout.write('\n')
        self._stdout.write('#EOF\n')
        self._stdout.flush()


if __name__ == '__main__':
    # Note that the Mock in MockDRT refers to the fact that it is emulating a
    # real DRT, and as such, it needs access to a real SystemHost, not a MockSystemHost.
    sys.exit(main(sys.argv[1:], SystemHost(), sys.stdin, sys.stdout, sys.stderr))
