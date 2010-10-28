# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import os
import sys
import unittest
import chromium_win
from webkitpy.common.system import outputcapture
from webkitpy.tool import mocktool


class ChromiumWinTest(unittest.TestCase):

    class RegisterCygwinOption(object):
        def __init__(self):
            self.register_cygwin = True

    def setUp(self):
        self.orig_platform = sys.platform

    def tearDown(self):
        sys.platform = self.orig_platform

    def _mock_path_from_chromium_base(self, *comps):
        return os.path.join("/chromium/src", *comps)

    def test_setup_environ_for_server(self):
        port = chromium_win.ChromiumWinPort()
        port._executive = mocktool.MockExecutive(should_log=True)
        port.path_from_chromium_base = self._mock_path_from_chromium_base
        output = outputcapture.OutputCapture()
        orig_environ = os.environ.copy()
        env = output.assert_outputs(self, port.setup_environ_for_server)
        self.assertEqual(orig_environ["PATH"], os.environ["PATH"])
        self.assertNotEqual(env["PATH"], os.environ["PATH"])

    def test_setup_environ_for_server_register_cygwin(self):
        sys.platform = "win32"
        port = chromium_win.ChromiumWinPort(
            options=ChromiumWinTest.RegisterCygwinOption())
        port._executive = mocktool.MockExecutive(should_log=True)
        port.path_from_chromium_base = self._mock_path_from_chromium_base
        setup_mount = self._mock_path_from_chromium_base("third_party",
                                                         "cygwin",
                                                         "setup_mount.bat")
        expected_stderr = "MOCK run_command: %s\n" % [setup_mount]
        output = outputcapture.OutputCapture()
        output.assert_outputs(self, port.setup_environ_for_server,
                              expected_stderr=expected_stderr)
