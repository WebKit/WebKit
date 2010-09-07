#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Dummy Port implementation used for testing."""
from __future__ import with_statement

import codecs
import os
import time

import base


class TestPort(base.Port):
    """Test implementation of the Port interface."""

    def __init__(self, port_name=None, options=None):
        base.Port.__init__(self, port_name, options)

    def baseline_path(self):
        return os.path.join(self.layout_tests_dir(), 'platform',
                            self.name() + self.version())

    def baseline_search_path(self):
        return [self.baseline_path()]

    def check_build(self, needs_http):
        return True

    def diff_image(self, expected_filename, actual_filename,
                   diff_filename=None, tolerance=0):
        with codecs.open(actual_filename, "r", "utf-8") as actual_fh:
            actual_contents = actual_fh.read()
        with codecs.open(expected_filename, "r", "utf-8") as expected_fh:
            expected_contents = expected_fh.read()
        diffed = actual_contents != expected_contents
        if diffed and diff_filename:
            with codecs.open(diff_filename, "w", "utf-8") as diff_fh:
                diff_fh.write("< %s\n---\n> %s\n" %
                              (expected_contents, actual_contents))
        return diffed

    def layout_tests_dir(self):
        return self.path_from_webkit_base('WebKitTools', 'Scripts',
                                          'webkitpy', 'layout_tests', 'data')

    def name(self):
        return self._name

    def options(self):
        return self._options

    def skipped_layout_tests(self):
        return []

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('WebKitTools', 'Scripts',
            'webkitpy', 'layout_tests', 'data', 'platform', 'test',
            'test_expectations.txt')

    def _path_to_wdiff(self):
        return None

    def results_directory(self):
        return '/tmp/' + self._options.results_directory

    def setup_test_run(self):
        pass

    def show_results_html_file(self, filename):
        pass

    def create_driver(self, image_path, options):
        return TestDriver(image_path, options, self)

    def start_http_server(self):
        pass

    def start_websocket_server(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def test_expectations(self):
        """Returns the test expectations for this port.

        Basically this string should contain the equivalent of a
        test_expectations file. See test_expectations.py for more details."""
        expectations_path = self.path_to_test_expectations_file()
        with codecs.open(expectations_path, "r", "utf-8") as file:
            return file.read()

    def test_base_platform_names(self):
        return ('mac', 'win')

    def test_platform_name(self):
        return 'mac'

    def test_platform_names(self):
        return self.test_base_platform_names()

    def test_platform_name_to_name(self, test_platform_name):
        return test_platform_name

    def version(self):
        return ''


class TestDriver(base.Driver):
    """Test/Dummy implementation of the DumpRenderTree interface."""

    def __init__(self, image_path, test_driver_options, port):
        self._driver_options = test_driver_options
        self._image_path = image_path
        self._port = port
        self._image_written = False

    def poll(self):
        return True

    def returncode(self):
        return 0

    def run_test(self, uri, timeoutms, image_hash):
        basename = uri[(uri.rfind("/") + 1):uri.rfind(".html")]

        if 'error' in basename:
            error = basename + "_error\n"
        else:
            error = ''
        checksum = None
        # There are four currently supported types of tests: text, image,
        # image hash (checksum), and stderr output. The fake output
        # is the basename of the file + "-" plus the type of test output
        # (or a blank string for stderr).
        #
        # If 'image' or 'check' appears in the basename, we assume this is
        # simulating a pixel test.
        #
        # If 'failures' appears in the URI, then we assume this test should
        # fail. Which type of failures are determined by which strings appear
        # in the basename of the test. For failures that produce outputs,
        # we change the fake output to basename + "_failed-".
        #
        # The fact that each test produces (more or less) unique output data
        # will allow us to see if any results get crossed by the rest of the
        # program.
        if 'failures' in uri:
            if 'keyboard' in basename:
                raise KeyboardInterrupt
            if 'exception' in basename:
                raise ValueError('exception from ' + basename)

            crash = 'crash' in basename
            timeout = 'timeout' in basename
            if 'text' in basename:
                output = basename + '_failed-txt\n'
            else:
                output = basename + '-txt\n'
            if self._port.options().pixel_tests:
                if ('image' in basename or 'check' in basename):
                    checksum = basename + "-checksum\n"

                if 'image' in basename:
                    with open(self._image_path, "w") as f:
                        f.write(basename + "_failed-png\n")
                elif 'check' in basename:
                    with open(self._image_path, "w") as f:
                        f.write(basename + "-png\n")
                if 'checksum' in basename:
                    checksum = basename + "_failed-checksum\n"
        else:
            crash = False
            timeout = False
            output = basename + '-txt\n'
            if self._port.options().pixel_tests and (
                'image' in basename or 'check' in basename):
                checksum = basename + '-checksum\n'
                with open(self._image_path, "w") as f:
                    f.write(basename + "-png")

        return (crash, timeout, checksum, output, error)

    def start(self):
        pass

    def stop(self):
        pass
