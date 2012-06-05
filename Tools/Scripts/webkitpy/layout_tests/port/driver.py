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

import copy
import re
import shlex

from webkitpy.common.system import path


class DriverInput(object):
    def __init__(self, test_name, timeout, image_hash, should_run_pixel_test, args=None):
        self.test_name = test_name
        self.timeout = timeout  # in ms
        self.image_hash = image_hash
        self.should_run_pixel_test = should_run_pixel_test
        self.args = args or []


class DriverOutput(object):
    """Groups information about a output from driver for easy passing
    and post-processing of data."""

    strip_patterns = []
    strip_patterns.append((re.compile('at \(-?[0-9]+,-?[0-9]+\) *'), ''))
    strip_patterns.append((re.compile('size -?[0-9]+x-?[0-9]+ *'), ''))
    strip_patterns.append((re.compile('text run width -?[0-9]+: '), ''))
    strip_patterns.append((re.compile('text run width -?[0-9]+ [a-zA-Z ]+: '), ''))
    strip_patterns.append((re.compile('RenderButton {BUTTON} .*'), 'RenderButton {BUTTON}'))
    strip_patterns.append((re.compile('RenderImage {INPUT} .*'), 'RenderImage {INPUT}'))
    strip_patterns.append((re.compile('RenderBlock {INPUT} .*'), 'RenderBlock {INPUT}'))
    strip_patterns.append((re.compile('RenderTextControl {INPUT} .*'), 'RenderTextControl {INPUT}'))
    strip_patterns.append((re.compile('\([0-9]+px'), 'px'))
    strip_patterns.append((re.compile(' *" *\n +" *'), ' '))
    strip_patterns.append((re.compile('" +$'), '"'))
    strip_patterns.append((re.compile('- '), '-'))
    strip_patterns.append((re.compile('\s+"\n'), '"\n'))
    strip_patterns.append((re.compile('scrollWidth [0-9]+'), 'scrollWidth'))
    strip_patterns.append((re.compile('scrollHeight [0-9]+'), 'scrollHeight'))

    def __init__(self, text, image, image_hash, audio, crash=False,
            test_time=0, timeout=False, error='', crashed_process_name='??',
            crashed_pid=None, crash_log=None):
        # FIXME: Args could be renamed to better clarify what they do.
        self.text = text
        self.image = image  # May be empty-string if the test crashes.
        self.image_hash = image_hash
        self.image_diff = None  # image_diff gets filled in after construction.
        self.audio = audio  # Binary format is port-dependent.
        self.crash = crash
        self.crashed_process_name = crashed_process_name
        self.crashed_pid = crashed_pid
        self.crash_log = crash_log
        self.test_time = test_time
        self.timeout = timeout
        self.error = error  # stderr output

    def has_stderr(self):
        return bool(self.error)

    def strip_metrics(self):
        if not self.text:
            return
        for pattern in self.strip_patterns:
            self.text = re.sub(pattern[0], pattern[1], self.text)


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
            return path.abspath_to_uri(self._port.host.platform, self._port.abspath_for_test(test_name))

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
            prefix = path.abspath_to_uri(self._port.host.platform, self._port.layout_tests_dir())
            if not prefix.endswith('/'):
                prefix += '/'
            return uri[len(prefix):]
        if uri.startswith("http://"):
            return uri.replace('http://127.0.0.1:8000/', self.HTTP_DIR)
        if uri.startswith("https://"):
            return uri.replace('https://127.0.0.1:8443/', self.HTTP_DIR)
        raise NotImplementedError('unknown url type: %s' % uri)

    def has_crashed(self):
        return False

    def start(self):
        raise NotImplementedError('Driver.start')

    def stop(self):
        raise NotImplementedError('Driver.stop')

    def cmd_line(self, pixel_tests, per_test_args):
        raise NotImplementedError('Driver.cmd_line')


class DriverProxy(object):
    """A wrapper for managing two Driver instances, one with pixel tests and
    one without. This allows us to handle plain text tests and ref tests with a
    single driver."""

    def __init__(self, port, worker_number, driver_instance_constructor, pixel_tests, no_timeout):
        self._port = port
        self._worker_number = worker_number
        self._driver_instance_constructor = driver_instance_constructor
        self._no_timeout = no_timeout

        # FIXME: We shouldn't need to create a driver until we actually run a test.
        self._driver = self._make_driver(pixel_tests)
        self._running_drivers = {}
        self._running_drivers[self._cmd_line_as_key(pixel_tests, [])] = self._driver

    def _make_driver(self, pixel_tests):
        return self._driver_instance_constructor(self._port, self._worker_number, pixel_tests, self._no_timeout)

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def is_http_test(self, test_name):
        return self._driver.is_http_test(test_name)

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def test_to_uri(self, test_name):
        return self._driver.test_to_uri(test_name)

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def uri_to_test(self, uri):
        return self._driver.uri_to_test(uri)

    def run_test(self, driver_input):
        base = self._port.lookup_virtual_test_base(driver_input.test_name)
        if base:
            virtual_driver_input = copy.copy(driver_input)
            virtual_driver_input.test_name = base
            virtual_driver_input.args = self._port.lookup_virtual_test_args(driver_input.test_name)
            return self.run_test(virtual_driver_input)

        pixel_tests_needed = driver_input.should_run_pixel_test
        cmd_line_key = self._cmd_line_as_key(pixel_tests_needed, driver_input.args)
        if not cmd_line_key in self._running_drivers:
            self._running_drivers[cmd_line_key] = self._make_driver(pixel_tests_needed)

        return self._running_drivers[cmd_line_key].run_test(driver_input)

    def start(self):
        # FIXME: Callers shouldn't normally call this, since this routine
        # may not be specifying the correct combination of pixel test and
        # per_test args.
        #
        # The only reason we have this routine at all is so the perftestrunner
        # can pause before running a test; it might be better to push that
        # into run_test() directly.
        self._driver.start(self._port.get_option('pixel_tests'), [])

    def has_crashed(self):
        return any(driver.has_crashed() for driver in self._running_drivers.values())

    def stop(self):
        for driver in self._running_drivers.values():
            driver.stop()

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def cmd_line(self, pixel_tests=None, per_test_args=None):
        return self._driver.cmd_line(pixel_tests, per_test_args or [])

    def _cmd_line_as_key(self, pixel_tests, per_test_args):
        return ' '.join(self.cmd_line(pixel_tests, per_test_args))
