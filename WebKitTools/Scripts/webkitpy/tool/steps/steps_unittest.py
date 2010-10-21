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

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.config.ports import WebKitPort
from webkitpy.tool.mocktool import MockOptions, MockTool
from webkitpy.tool.steps.update import Update
from webkitpy.tool.steps.runtests import RunTests
from webkitpy.tool.steps.promptforbugortitle import PromptForBugOrTitle


class StepsTest(unittest.TestCase):
    def _step_options(self):
        options = MockOptions()
        options.non_interactive = True
        options.port = 'MOCK port'
        options.quiet = True
        options.test = True
        return options

    def _run_step(self, step, tool=None, options=None, state=None):
        if not tool:
            tool = MockTool()
        if not options:
            options = self._step_options()
        if not state:
            state = {}
        step(tool, options).run(state)

    def test_update_step(self):
        tool = MockTool()
        options = self._step_options()
        options.update = True
        expected_stderr = "Updating working directory\n"
        OutputCapture().assert_outputs(self, self._run_step, [Update, tool, options], expected_stderr=expected_stderr)

    def test_prompt_for_bug_or_title_step(self):
        tool = MockTool()
        tool.user.prompt = lambda message: 42
        self._run_step(PromptForBugOrTitle, tool=tool)

    def test_runtests_leopard_commit_queue_hack_step(self):
        expected_stderr = "Running Python unit tests\nRunning Perl unit tests\nRunning JavaScriptCore tests\nRunning run-webkit-tests\n"
        OutputCapture().assert_outputs(self, self._run_step, [RunTests], expected_stderr=expected_stderr)

    def test_runtests_leopard_commit_queue_hack_command(self):
        mock_options = self._step_options()
        step = RunTests(MockTool(log_executive=True), mock_options)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        mock_port = WebKitPort()
        mock_port.name = lambda: "Mac"
        mock_port.is_leopard = lambda: True
        tool = MockTool(log_executive=True)
        tool.port = lambda: mock_port
        step = RunTests(tool, mock_options)
        expected_stderr = """Running Python unit tests
MOCK run_and_throw_if_fail: ['WebKitTools/Scripts/test-webkitpy']
Running Perl unit tests
MOCK run_and_throw_if_fail: ['WebKitTools/Scripts/test-webkitperl']
Running JavaScriptCore tests
MOCK run_and_throw_if_fail: ['WebKitTools/Scripts/run-javascriptcore-tests']
Running run-webkit-tests
MOCK run_and_throw_if_fail: ['WebKitTools/Scripts/run-webkit-tests', '--no-new-test-results', '--no-launch-safari', '--exit-after-n-failures=1', '--wait-for-httpd', '--ignore-tests', 'compositing,media', '--quiet']
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_stderr=expected_stderr)
