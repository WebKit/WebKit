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

    def base_platforms(self):
        return ('test',)

    def baseline_path(self):
        return os.path.join(self.layout_tests_dir(), 'platform',
                            self.name())

    def baseline_search_path(self):
        return [self.baseline_path()]

    def check_build(self, needs_http):
        return True

    def compare_text(self, expected_text, actual_text):
        return False

    def diff_image(self, expected_filename, actual_filename,
                   diff_filename=None, tolerance=0):
        return False

    def diff_text(self, expected_text, actual_text,
                  expected_filename, actual_filename):
        return ''

    def layout_tests_dir(self):
        return self.path_from_webkit_base('WebKitTools', 'Scripts',
                                          'webkitpy', 'layout_tests', 'data')

    def name(self):
        return self._name

    def options(self):
        return self._options

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('WebKitTools', 'Scripts',
            'webkitpy', 'layout_tests', 'data', 'platform', 'test',
            'test_expectations.txt')

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
        return ('test',)

    def test_platform_name(self):
        return 'test'

    def test_platform_names(self):
        return self.test_base_platform_names()

    def test_platform_name_to_name(self, test_platform_name):
        return test_platform_name

    def version():
        return ''

    def wdiff_text(self, expected_filename, actual_filename):
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
        if not self._image_written and self._port._options.pixel_tests:
            with open(self._image_path, "w") as f:
                f.write("bad png file from TestDriver")
                self._image_written = True

        # We special-case this because we can't fake an image hash for a
        # missing expectation.
        if uri.find('misc/missing-expectation') != -1:
            return (False, False, 'deadbeefdeadbeefdeadbeefdeadbeef', '', None)
        return (False, False, image_hash, '', None)

    def start(self):
        pass

    def stop(self):
        pass
