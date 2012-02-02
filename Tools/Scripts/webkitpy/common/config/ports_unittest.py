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

from webkitpy.common.config.ports import *


class WebKitPortTest(unittest.TestCase):
    def test_mac_port(self):
        self.assertEquals(MacPort.name(), "Mac")
        self.assertEquals(MacPort.flag(), "--port=mac")
        self.assertEquals(MacPort.run_webkit_tests_command(), WebKitPort.script_shell_command("run-webkit-tests"))
        self.assertEquals(MacPort.build_webkit_command(), WebKitPort.script_shell_command("build-webkit"))
        self.assertEquals(MacPort.build_webkit_command(build_style="debug"), WebKitPort.script_shell_command("build-webkit") + ["--debug"])
        self.assertEquals(MacPort.build_webkit_command(build_style="release"), WebKitPort.script_shell_command("build-webkit") + ["--release"])

        class TestIsLeopard(MacPort):
            @classmethod
            def _system_version(cls):
                return [10, 5]
        self.assertTrue(TestIsLeopard.is_leopard())

    def test_gtk_port(self):
        self.assertEquals(GtkPort.name(), "Gtk")
        self.assertEquals(GtkPort.flag(), "--port=gtk")
        self.assertEquals(GtkPort.run_webkit_tests_command(), WebKitPort.script_shell_command("run-webkit-tests") + ["--gtk"])
        self.assertEquals(GtkPort.build_webkit_command(), WebKitPort.script_shell_command("build-webkit") + ["--gtk", "--update-gtk", WebKitPort.makeArgs()])
        self.assertEquals(GtkPort.build_webkit_command(build_style="debug"), WebKitPort.script_shell_command("build-webkit") + ["--debug", "--gtk", "--update-gtk", WebKitPort.makeArgs()])

    def test_qt_port(self):
        self.assertEquals(QtPort.name(), "Qt")
        self.assertEquals(QtPort.flag(), "--port=qt")
        self.assertEquals(QtPort.run_webkit_tests_command(), WebKitPort.script_shell_command("run-webkit-tests"))
        self.assertEquals(QtPort.build_webkit_command(), WebKitPort.script_shell_command("build-webkit") + ["--qt", WebKitPort.makeArgs()])
        self.assertEquals(QtPort.build_webkit_command(build_style="debug"), WebKitPort.script_shell_command("build-webkit") + ["--debug", "--qt", WebKitPort.makeArgs()])

    def test_chromium_port(self):
        self.assertEquals(ChromiumPort.name(), "Chromium")
        self.assertEquals(ChromiumPort.flag(), "--port=chromium")
        self.assertEquals(ChromiumPort.run_webkit_tests_command(), WebKitPort.script_shell_command("new-run-webkit-tests") + ["--chromium", "--skip-failing-tests"])
        self.assertEquals(ChromiumPort.build_webkit_command(), WebKitPort.script_shell_command("build-webkit") + ["--chromium", "--update-chromium"])
        self.assertEquals(ChromiumPort.build_webkit_command(build_style="debug"), WebKitPort.script_shell_command("build-webkit") + ["--debug", "--chromium", "--update-chromium"])
        self.assertEquals(ChromiumPort.update_webkit_command(), WebKitPort.script_shell_command("update-webkit") + ["--chromium"])

    def test_chromium_xvfb_port(self):
        self.assertEquals(ChromiumXVFBPort.run_webkit_tests_command(), ['xvfb-run'] + WebKitPort.script_shell_command('new-run-webkit-tests') + ['--chromium', '--skip-failing-tests'])

if __name__ == '__main__':
    unittest.main()
