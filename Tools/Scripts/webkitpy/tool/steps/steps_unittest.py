# Copyright (C) 2010 Google Inc. All rights reserved.
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

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.config.ports import DeprecatedPort
from webkitpy.tool.mocktool import MockOptions, MockTool

from webkitpy.tool import steps


class StepsTest(unittest.TestCase):

    def setUp(self):
        # Port._build_path() calls Tools/Scripts/webkit-build-directory and caches the result. When capturing output,
        # this can cause the first invocation of Port._build_path() to have more output than subsequent invocations.
        # This may cause test flakiness when test order changes. By explicitly calling Port._build_path() before running
        # tests in this suite, we avoid such flakiness.
        MockTool().port_factory.get(options=self._step_options())._build_path()

    def _step_options(self):
        options = MockOptions()
        options.group = None
        options.non_interactive = True
        options.port = 'MOCK port'
        options.quiet = True
        options.test = True
        options.iterate_on_new_tests = 0
        options.root = '/tmp'
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
        expected_logs = "Updating working directory\n"
        OutputCapture().assert_outputs(self, self._run_step, [steps.Update, tool, options], expected_logs=expected_logs)

    def test_prompt_for_bug_or_title_step(self):
        tool = MockTool()
        tool.user.prompt = lambda message: 50000
        self._run_step(steps.PromptForBugOrTitle, tool=tool)

    def _post_diff_options(self):
        options = self._step_options()
        options.git_commit = None
        options.description = None
        options.comment = None
        options.review = True
        options.request_commit = False
        options.open_bug = True
        return options

    def _assert_step_output_with_bug(self, step, bug_id, expected_logs, options=None):
        state = {'bug_id': bug_id}
        OutputCapture().assert_outputs(self, self._run_step, [step, MockTool(), options, state], expected_logs=expected_logs)

    def _assert_post_diff_output_for_bug(self, step, bug_id, expected_logs):
        self._assert_step_output_with_bug(step, bug_id, expected_logs, self._post_diff_options())

    def test_post_diff(self):
        expected_logs = "MOCK add_patch_to_bug: bug_id=78, description=Patch, mark_for_review=True, mark_for_commit_queue=False, mark_for_landing=False\nMOCK: user.open_url: http://example.com/78\n"
        self._assert_post_diff_output_for_bug(steps.PostDiff, 78, expected_logs)

    def test_post_diff_for_commit(self):
        expected_logs = "MOCK add_patch_to_bug: bug_id=78, description=Patch for landing, mark_for_review=False, mark_for_commit_queue=False, mark_for_landing=True\n"
        self._assert_post_diff_output_for_bug(steps.PostDiffForCommit, 78, expected_logs)

    def test_ensure_bug_is_open_and_assigned(self):
        expected_logs = "MOCK reopen_bug 50004 with comment 'Reopening to attach new patch.'\n"
        self._assert_step_output_with_bug(steps.EnsureBugIsOpenAndAssigned, 50004, expected_logs)
        expected_logs = "MOCK reassign_bug: bug_id=50002, assignee=None\n"
        self._assert_step_output_with_bug(steps.EnsureBugIsOpenAndAssigned, 50002, expected_logs)

    def test_runtests_args(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "release"
        step = steps.RunTests(MockTool(log_executive=True), mock_options)
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """Running Python unit tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/test-webkitpy'], cwd=/mock-checkout
Running Perl unit tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/test-webkitperl'], cwd=/mock-checkout
Running JavaScriptCore tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast'], cwd=/mock-checkout
Running run-webkit-tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/run-webkit-tests', '--release', '--quiet'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_runtests_debug_args(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "debug"
        step = steps.RunTests(MockTool(log_executive=True), mock_options)
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """Running Python unit tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/test-webkitpy'], cwd=/mock-checkout
Running Perl unit tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/test-webkitperl'], cwd=/mock-checkout
Running JavaScriptCore tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast'], cwd=/mock-checkout
Running run-webkit-tests
MOCK run_and_throw_if_fail: ['Tools/Scripts/run-webkit-tests', '--debug', '--quiet'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_runtests_jsc(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "release"
        mock_options.group = "jsc"
        step = steps.RunTests(MockTool(log_executive=True), mock_options)
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """MOCK run_and_throw_if_fail: ['Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast', '--release', '--json-output=/tmp/jsc_test_results.json'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_runtests_jsc_debug(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "debug"
        mock_options.group = "jsc"
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """MOCK run_and_throw_if_fail: ['Tools/Scripts/run-javascriptcore-tests', '--no-fail-fast', '--debug', '--json-output=/tmp/jsc_test_results.json'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_build_jsc_debug(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "debug"
        mock_options.build = True
        mock_options.architecture = True
        mock_options.group = "jsc"
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.Build(tool, mock_options)
        expected_logs = """Building WebKit
MOCK run_and_throw_if_fail: ['Tools/Scripts/build-jsc', '--debug', 'ARCHS=True'], cwd=/mock-checkout, env={'TERM': 'dumb', 'MOCK_ENVIRON_COPY': '1'}
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_build_jsc(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "release"
        mock_options.build = True
        mock_options.architecture = True
        mock_options.group = "jsc"
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.Build(tool, mock_options)
        expected_logs = """Building WebKit
MOCK run_and_throw_if_fail: ['Tools/Scripts/build-jsc', '--release', 'ARCHS=True'], cwd=/mock-checkout, env={'TERM': 'dumb', 'MOCK_ENVIRON_COPY': '1'}
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_patch_relevant(self):
        self.maxDiff = None
        mock_options = self._step_options()
        tool = MockTool(log_executive=True)
        tool.scm()._mockChangedFiles = ["JSTests/MockFile1", "ChangeLog"]
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.CheckPatchRelevance(tool, mock_options)
        expected_logs = """Checking relevance of patch
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_patch_relevant_jsc(self):
        self.maxDiff = None
        mock_options = self._step_options()
        mock_options.group = "jsc"
        tool = MockTool(log_executive=True)
        tool.scm()._mockChangedFiles = ["JSTests/MockFile1", "ChangeLog"]
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.CheckPatchRelevance(tool, mock_options)
        expected_logs = """Checking relevance of patch
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_patch_not_relevant_jsc(self):
        self.maxDiff = None
        mock_options = self._step_options()
        mock_options.group = "jsc"
        tool = MockTool(log_executive=True)
        tool.scm()._mockChangedFiles = ["Tools/ChangeLog", "Tools/Scripts/webkitpy/tool/steps/steps_unittest.py"]
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.CheckPatchRelevance(tool, mock_options)
        expected_logs = """Checking relevance of patch
This patch does not have relevant changes.
"""
        with self.assertRaises(ScriptError):
            OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_runtests_bindings(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.group = "bindings"
        step = steps.RunTests(MockTool(log_executive=True), mock_options)
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """MOCK run_and_throw_if_fail: ['Tools/Scripts/run-bindings-tests', '--json-output=/tmp/bindings_test_results.json'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_runtests_webkitpy(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.group = "webkitpy"
        step = steps.RunTests(MockTool(log_executive=True), mock_options)
        tool = MockTool(log_executive=True)
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """MOCK run_and_throw_if_fail: ['Tools/Scripts/test-webkitpy', '--json-output=/tmp/python-unittest-results/webkitpy_test_results.json'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_patch_relevant_bindings(self):
        self.maxDiff = None
        mock_options = self._step_options()
        mock_options.group = "bindings"
        tool = MockTool(log_executive=True)
        tool.scm()._mockChangedFiles = ["Source/WebCore/features.json", "Source/ChangeLog"]
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.CheckPatchRelevance(tool, mock_options)
        expected_logs = """Checking relevance of patch
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_patch_not_relevant_bindings(self):
        self.maxDiff = None
        mock_options = self._step_options()
        mock_options.group = "bindings"
        tool = MockTool(log_executive=True)
        tool.scm()._mockChangedFiles = ["Source/JavaScriptCore/Makefile", "Source/ChangeLog"]
        # FIXME: We shouldn't use a real port-object here, but there is too much to mock at the moment.
        tool._deprecated_port = DeprecatedPort()
        step = steps.CheckPatchRelevance(tool, mock_options)
        expected_logs = """Checking relevance of patch
This patch does not have relevant changes.
"""

    def test_runtests_api(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "release"
        mock_options.group = "api"
        step = steps.RunTests(MockTool(log_executive=True), mock_options)
        tool = MockTool(log_executive=True)
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """MOCK run_and_throw_if_fail: ['Tools/Scripts/run-api-tests', '--release', '--json-output=/tmp/api_test_results.json'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)

    def test_runtests_api_debug(self):
        mock_options = self._step_options()
        mock_options.non_interactive = False
        mock_options.build_style = "debug"
        mock_options.group = "api"
        step = steps.RunTests(MockTool(log_executive=True), mock_options)
        tool = MockTool(log_executive=True)
        tool._deprecated_port = DeprecatedPort()
        step = steps.RunTests(tool, mock_options)
        expected_logs = """MOCK run_and_throw_if_fail: ['Tools/Scripts/run-api-tests', '--debug', '--json-output=/tmp/api_test_results.json'], cwd=/mock-checkout
"""
        OutputCapture().assert_outputs(self, step.run, [{}], expected_logs=expected_logs)
