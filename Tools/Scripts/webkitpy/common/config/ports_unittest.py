# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (C) 2017 Apple Inc. All rights reserved.
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


class DeprecatedPortTest(unittest.TestCase):
    def test_mac_port(self):
        self.assertEqual(MacPort().flag(), "--port=mac")
        self.assertEqual(MacPort().run_webkit_tests_command(), DeprecatedPort().script_shell_command("run-webkit-tests") + ["--dump-render-tree"])
        self.assertEqual(MacPort().build_webkit_command(), DeprecatedPort().script_shell_command("build-webkit"))
        self.assertEqual(MacPort().build_webkit_command(build_style="debug"), DeprecatedPort().script_shell_command("build-webkit") + ["--debug"])
        self.assertEqual(MacPort().build_webkit_command(build_style="release"), DeprecatedPort().script_shell_command("build-webkit") + ["--release"])

        self.assertEqual(MacPort().build_jsc_command(), DeprecatedPort().script_shell_command("build-jsc"))
        self.assertEqual(MacPort().build_jsc_command(build_style="release"), DeprecatedPort().script_shell_command("build-jsc") + ["--release"])
        self.assertEqual(MacPort().build_jsc_command(build_style="debug"), DeprecatedPort().script_shell_command("build-jsc") + ["--debug"])

    def test_gtk_wk2_port(self):
        self.assertEqual(GtkWK2Port().flag(), "--port=gtk-wk2")
        self.assertEqual(GtkWK2Port().run_webkit_tests_command(), DeprecatedPort().script_shell_command("run-webkit-tests") + ["--gtk"])
        self.assertEqual(GtkWK2Port().build_webkit_command(), DeprecatedPort().script_shell_command("build-webkit") + ["--gtk", "--update-gtk", DeprecatedPort().makeArgs()])
        self.assertEqual(GtkWK2Port().build_webkit_command(build_style="debug"), DeprecatedPort().script_shell_command("build-webkit") + ["--debug", "--gtk", "--update-gtk", DeprecatedPort().makeArgs()])

    def test_wpe_port(self):
        self.assertEqual(WpePort().flag(), "--port=wpe")
        self.assertEqual(WpePort().run_webkit_tests_command(), DeprecatedPort().script_shell_command("run-webkit-tests") + ["--wpe"])
        self.assertEqual(WpePort().build_webkit_command(), DeprecatedPort().script_shell_command("build-webkit") + ["--wpe", "--update-wpe", DeprecatedPort().makeArgs()])
        self.assertEqual(WpePort().build_webkit_command(build_style="debug"), DeprecatedPort().script_shell_command("build-webkit") + ["--debug", "--wpe", "--update-wpe", DeprecatedPort().makeArgs()])

    def test_wincairo_port(self):
        self.assertEqual(WinCairoPort().flag(), "--port=wincairo")
        self.assertEqual(WinCairoPort().run_webkit_tests_command(), DeprecatedPort().script_shell_command("run-webkit-tests") + ["--wincairo"])
        self.assertEqual(WinCairoPort().build_webkit_command(), DeprecatedPort().script_shell_command("build-webkit") + ["--wincairo"])
        self.assertEqual(WinCairoPort().build_webkit_command(build_style="debug"), DeprecatedPort().script_shell_command("build-webkit") + ["--debug", "--wincairo"])

    def test_jsconly_port(self):
        self.assertEqual(JscOnlyPort().flag(), "--port=jsc-only")
        self.assertEqual(JscOnlyPort().build_jsc_command(), DeprecatedPort().script_shell_command("build-jsc") + ["--jsc-only"])
        self.assertEqual(JscOnlyPort().build_jsc_command(build_style="release"), DeprecatedPort().script_shell_command("build-jsc") + ["--jsc-only", "--release"])
        self.assertEqual(JscOnlyPort().build_jsc_command(build_style="debug"), DeprecatedPort().script_shell_command("build-jsc") + ["--jsc-only", "--debug"])
        self.assertEqual(JscOnlyPort().run_javascriptcore_tests_command(build_style="debug"), DeprecatedPort().script_shell_command("run-javascriptcore-tests") + ['--no-fail-fast', '--no-testmasm', '--no-testair', '--no-testb3', '--no-testdfg', '--no-testapi', '--debug'])
