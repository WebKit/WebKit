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
        curdir = os.path.abspath(__file__)
        self.topdir = curdir[0:curdir.index("WebKitTools")]
        return os.path.join(self.topdir, 'LayoutTests', 'platform', 'test')

    def baseline_search_path(self):
        return [self.baseline_path()]

    def check_sys_deps(self):
        return True

    def diff_image(self, actual_filename, expected_filename, diff_filename):
        return False

    def compare_text(self, actual_text, expected_text):
        return False

    def diff_text(self, actual_text, expected_text,
                  actual_filename, expected_filename):
        return ''

    def name(self):
        return self._name

    def num_cores(self):
        return int(os.popen2("sysctl -n hw.ncpu")[1].read())

    def options(self):
        return self._options

    def results_directory(self):
        return '/tmp' + self._options.results_directory

    def setup_test_run(self):
        pass

    def show_results_html_file(self, filename):
        pass

    def start_driver(self, image_path, options):
        return TestDriver(image_path, options, self)

    def start_http_server(self):
        pass

    def start_websocket_server(self):
        pass

    def start_helper(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def stop_helper(self):
        pass

    def test_expectations(self):
        return ''

    def test_base_platform_names(self):
        return ('test',)

    def test_platform_name(self):
        return 'test'

    def test_platform_names(self):
        return self.test_base_platform_names()

    def version():
        return ''

    def wdiff_text(self, actual_filename, expected_filename):
        return ''


class TestDriver(base.Driver):
    """Test/Dummy implementation of the DumpRenderTree interface."""

    def __init__(self, image_path, test_driver_options, port):
        self._driver_options = test_driver_options
        self._image_path = image_path
        self._port = port

    def poll(self):
        return True

    def returncode(self):
        return 0

    def run_test(self, uri, timeoutms, image_hash):
        return (False, False, image_hash, '', None)

    def stop(self):
        pass
