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

import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.rebaseline import *
from webkitpy.tool.mocktool import MockTool, MockExecutive


class TestRebaseline(unittest.TestCase):
    def test_tests_to_update(self):
        command = Rebaseline()
        command.bind_to_tool(MockTool())
        build = Mock()
        OutputCapture().assert_outputs(self, command._tests_to_update, [build])

    def test_rebaseline_test(self):
        command = RebaselineTest()
        command.bind_to_tool(MockTool())
        expected_stdout = "Retrieving http://example.com/f/builders/Webkit Linux/results/userscripts/another-test-actual.txt.\n"
        OutputCapture().assert_outputs(self, command._rebaseline_test, ["Webkit Linux", "userscripts/another-test.html", "txt"], expected_stdout=expected_stdout)

    def test_rebaseline_expectations(self):
        command = RebaselineExpectations()
        tool = MockTool()
        tool.executive = MockExecutive(should_log=True)
        command.bind_to_tool(tool)
        expected_stdout = """Retrieving results for chromium-gpu-mac-leopard from Webkit Mac10.5 - GPU.
Retrieving results for chromium-gpu-mac-snowleopard from Webkit Mac10.6 - GPU.
Retrieving results for chromium-gpu-win-win7 from Webkit Win7 - GPU.
Retrieving results for chromium-gpu-win-xp from Webkit Win - GPU.
Retrieving results for chromium-linux-x86 from Webkit Linux 32.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-linux-x86_64 from Webkit Linux (dbg)(2).
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-mac-leopard from Webkit Mac10.5 (dbg)(1).
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-mac-snowleopard from Webkit Mac10.6 (dbg).
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-win-vista from Webkit Vista.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-win-win7 from Webkit Win7.
    userscripts/another-test.html
    userscripts/images.svg
Retrieving results for chromium-win-xp from Webkit Win (dbg)(2).
    userscripts/another-test.html
    userscripts/images.svg
Optimizing baselines for userscripts/another-test.html.
Optimizing baselines for userscripts/images.svg.
"""
        expected_stderr = """MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux 32', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux 32', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux (dbg)(2)', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Linux (dbg)(2)', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.5 (dbg)(1)', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.5 (dbg)(1)', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.6 (dbg)', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Mac10.6 (dbg)', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Vista', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Vista', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win7', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win7', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win (dbg)(2)', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'rebaseline-test', 'Webkit Win (dbg)(2)', 'userscripts/images.svg'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', 'userscripts/another-test.html'], cwd=/mock-checkout
MOCK run_command: ['echo', 'optimize-baselines', 'userscripts/images.svg'], cwd=/mock-checkout
"""
        command._tests_to_rebaseline = lambda port: [] if not port.name().find('-gpu-') == -1 else ['userscripts/another-test.html', 'userscripts/images.svg']
        OutputCapture().assert_outputs(self, command.execute, [MockTool(), None, None], expected_stdout=expected_stdout, expected_stderr=expected_stderr)
