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

import shlex

from webkitpy.common.system import path


class DriverInput(object):
    def __init__(self, test_name, timeout, image_hash, is_reftest):
        self.test_name = test_name
        self.timeout = timeout  # in ms
        self.image_hash = image_hash
        self.is_reftest = is_reftest


class DriverOutput(object):
    """Groups information about a output from driver for easy passing of data."""

    def __init__(self, text, image, image_hash, audio, crash=False,
            test_time=0, timeout=False, error='', crashed_process_name=None):
        # FIXME: Args could be renamed to better clarify what they do.
        self.text = text
        self.image = image  # May be empty-string if the test crashes.
        self.image_hash = image_hash
        self.image_diff = None  # image_diff gets filled in after construction.
        self.audio = audio  # Binary format is port-dependent.
        self.crash = crash
        self.crashed_process_name = crashed_process_name
        self.test_time = test_time
        self.timeout = timeout
        self.error = error  # stderr output

    def has_stderr(self):
        return bool(self.error)


class Driver(object):
    """Abstract interface for the DumpRenderTree interface."""

    def __init__(self, port, worker_number, pixel_tests, no_timeout=False):
        """Initialize a Driver to subsequently run tests.

        Typically this routine will spawn DumpRenderTree in a config
        ready for subsequent input.

        port - reference back to the port object.
        worker_number - identifier for a particular worker/driver instance
        """
        self._port = port
        self._worker_number = worker_number
        self._pixel_tests = pixel_tests
        self._no_timeout = no_timeout

    def run_test(self, driver_input):
        """Run a single test and return the results.

        Note that it is okay if a test times out or crashes and leaves
        the driver in an indeterminate state. The upper layers of the program
        are responsible for cleaning up and ensuring things are okay.

        Returns a DriverOuput object.
        """
        raise NotImplementedError('Driver.run_test')

    # FIXME: Seems this could just be inlined into callers.
    def _command_wrapper(cls, wrapper_option):
        # Hook for injecting valgrind or other runtime instrumentation,
        # used by e.g. tools/valgrind/valgrind_tests.py.
        return shlex.split(wrapper_option) if wrapper_option else []

    HTTP_DIR = "http/tests/"
    HTTP_LOCAL_DIR = "http/tests/local/"

    def is_http_test(self, test_name):
        return test_name.startswith(self.HTTP_DIR) and not test_name.startswith(self.HTTP_LOCAL_DIR)

    def test_to_uri(self, test_name):
        """Convert a test name to a URI."""
        if not self.is_http_test(test_name):
            return path.abspath_to_uri(self._port.abspath_for_test(test_name))

        relative_path = test_name[len(self.HTTP_DIR):]

        # TODO(dpranke): remove the SSL reference?
        if relative_path.startswith("ssl/"):
            return "https://127.0.0.1:8443/" + relative_path
        return "http://127.0.0.1:8000/" + relative_path

    def uri_to_test(self, uri):
        """Return the base layout test name for a given URI.

        This returns the test name for a given URI, e.g., if you passed in
        "file:///src/LayoutTests/fast/html/keygen.html" it would return
        "fast/html/keygen.html".

        """
        if uri.startswith("file:///"):
            return uri[len(path.abspath_to_uri(self._port.layout_tests_dir()) + "/"):]
        if uri.startswith("http://"):
            return uri.replace('http://127.0.0.1:8000/', self.HTTP_DIR)
        if uri.startswith("https://"):
            return uri.replace('https://127.0.0.1:8443/', self.HTTP_DIR)
        raise NotImplementedError('unknown url type: %s' % uri)

    def has_crashed(self):
        return False

    def stop(self):
        raise NotImplementedError('Driver.stop')

    def cmd_line(self):
        raise NotImplementedError('Driver.cmd_line')


class DriverProxy(object):
    """A wrapper for managing two Driver instances, one with pixel tests and
    one without. This allows us to handle plain text tests and ref tests with a
    single driver."""

    def __init__(self, port, worker_number, driver_instance_constructor, pixel_tests, no_timeout):
        self._driver = driver_instance_constructor(port, worker_number, pixel_tests, no_timeout)
        if pixel_tests:
            self._reftest_driver = self._driver
        else:
            self._reftest_driver = driver_instance_constructor(port, worker_number, pixel_tests, no_timeout)

    def is_http_test(self, test_name):
        return self._driver.is_http_test(test_name)

    def test_to_uri(self, test_name):
        return self._driver.test_to_uri(test_name)

    def uri_to_test(self, uri):
        return self._driver.uri_to_test(uri)

    def run_test(self, driver_input):
        if driver_input.is_reftest:
            return self._reftest_driver.run_test(driver_input)
        return self._driver.run_test(driver_input)

    def has_crashed(self):
        return self._driver.has_crashed() or self._reftest_driver.has_crashed()

    def stop(self):
        self._driver.stop()
        self._reftest_driver.stop()

    def cmd_line(self):
        cmd_line = self._driver.cmd_line()
        if self._driver != self._reftest_driver:
            cmd_line += ['; '] + self._reftest_driver.cmd_line()
        return cmd_line
