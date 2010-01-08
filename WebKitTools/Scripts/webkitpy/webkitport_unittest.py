#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
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

import unittest

from webkitpy.executive import Executive
from webkitpy.webkitport import WebKitPort, MacPort, GtkPort, QtPort, ChromiumPort


class WebKitPortTest(unittest.TestCase):
    def test_mac_port(self):
        self.assertEquals(MacPort.name(), "Mac")
        self.assertEquals(MacPort.flag(), "--port=mac")
        self.assertEquals(MacPort.run_webkit_tests_command(), [WebKitPort.script_path("run-webkit-tests")])
        self.assertEquals(MacPort.build_webkit_command(), [WebKitPort.script_path("build-webkit")])
        self.assertEquals(MacPort.build_webkit_command(build_style="debug"), [WebKitPort.script_path("build-webkit"), "--debug"])
        self.assertEquals(MacPort.build_webkit_command(build_style="release"), [WebKitPort.script_path("build-webkit"), "--release"])

    def test_gtk_port(self):
        self.assertEquals(GtkPort.name(), "Gtk")
        self.assertEquals(GtkPort.flag(), "--port=gtk")
        self.assertEquals(GtkPort.run_webkit_tests_command(), [WebKitPort.script_path("run-webkit-tests"), "--gtk"])
        self.assertEquals(GtkPort.build_webkit_command(), [WebKitPort.script_path("build-webkit"), "--gtk", '--makeargs="-j%s"' % Executive.cpu_count()])
        self.assertEquals(GtkPort.build_webkit_command(build_style="debug"), [WebKitPort.script_path("build-webkit"), "--debug", "--gtk", '--makeargs="-j%s"' % Executive.cpu_count()])

    def test_qt_port(self):
        self.assertEquals(QtPort.name(), "Qt")
        self.assertEquals(QtPort.flag(), "--port=qt")
        self.assertEquals(QtPort.run_webkit_tests_command(), [WebKitPort.script_path("run-webkit-tests")])
        self.assertEquals(QtPort.build_webkit_command(), [WebKitPort.script_path("build-webkit"), "--qt", '--makeargs="-j%s"' % Executive.cpu_count()])
        self.assertEquals(QtPort.build_webkit_command(build_style="debug"), [WebKitPort.script_path("build-webkit"), "--debug", "--qt", '--makeargs="-j%s"' % Executive.cpu_count()])

    def test_chromium_port(self):
        self.assertEquals(ChromiumPort.name(), "Chromium")
        self.assertEquals(ChromiumPort.flag(), "--port=chromium")
        self.assertEquals(ChromiumPort.run_webkit_tests_command(), [WebKitPort.script_path("run-webkit-tests")])
        self.assertEquals(ChromiumPort.build_webkit_command(), [WebKitPort.script_path("build-webkit"), "--chromium"])
        self.assertEquals(ChromiumPort.build_webkit_command(build_style="debug"), [WebKitPort.script_path("build-webkit"), "--debug", "--chromium"])
        self.assertEquals(ChromiumPort.update_webkit_command(), [WebKitPort.script_path("update-webkit"), "--chromium"])


if __name__ == '__main__':
    unittest.main()
