# Copyright (c) 2010 Google Inc. All rights reserved.
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

from datetime import datetime
import unittest

from webkitpy.common.system.deprecated_logging import error, log
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.bot.commitqueuetask import *
from webkitpy.tool.mocktool import MockTool


class MockCommitQueue(CommitQueueTaskDelegate):
    def __init__(self, error_plan):
        self._error_plan = error_plan

    def run_command(self, command):
        log("run_webkit_patch: %s" % command)
        if self._error_plan:
            error = self._error_plan.pop(0)
            if error:
                raise error

    def command_passed(self, success_message, patch):
        log("command_passed: success_message='%s' patch='%s'" % (
            success_message, patch.id()))

    def command_failed(self, failure_message, script_error, patch):
        log("command_failed: failure_message='%s' script_error='%s' patch='%s'" % (
            failure_message, script_error, patch.id()))
        return 3947

    def refetch_patch(self, patch):
        return patch

    def layout_test_results(self):
        return None

    def report_flaky_tests(self, patch, flaky_tests):
        log("report_flaky_tests: patch='%s' flaky_tests='%s'" % (patch.id(), flaky_tests))


class CommitQueueTaskTest(unittest.TestCase):
    def _run_through_task(self, commit_queue, expected_stderr, expected_exception=None):
        tool = MockTool(log_executive=True)
        patch = tool.bugs.fetch_attachment(197)
        task = CommitQueueTask(commit_queue, patch)
        OutputCapture().assert_outputs(self, task.run, expected_stderr=expected_stderr, expected_exception=expected_exception)

    def test_success_case(self):
        commit_queue = MockCommitQueue([])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_passed: success_message='Passed tests' patch='197'
run_webkit_patch: ['land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
command_passed: success_message='Landed patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr)

    def test_apply_failure(self):
        commit_queue = MockCommitQueue([
            ScriptError("MOCK apply failure"),
        ])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_failed: failure_message='Patch does not apply' script_error='MOCK apply failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, ScriptError)

    def test_build_failure(self):
        commit_queue = MockCommitQueue([
            None,
            ScriptError("MOCK build failure"),
        ])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_failed: failure_message='Patch does not build' script_error='MOCK build failure' patch='197'
run_webkit_patch: ['build', '--force-clean', '--no-update', '--build-style=both']
command_passed: success_message='Able to build without patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, ScriptError)

    def test_red_build_failure(self):
        commit_queue = MockCommitQueue([
            None,
            ScriptError("MOCK build failure"),
            ScriptError("MOCK clean build failure"),
        ])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_failed: failure_message='Patch does not build' script_error='MOCK build failure' patch='197'
run_webkit_patch: ['build', '--force-clean', '--no-update', '--build-style=both']
command_failed: failure_message='Unable to build without patch' script_error='MOCK clean build failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr)

    def test_flaky_test_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            ScriptError("MOCK tests failure"),
        ])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK tests failure' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_passed: success_message='Passed tests' patch='197'
report_flaky_tests: patch='197' flaky_tests='None'
run_webkit_patch: ['land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
command_passed: success_message='Landed patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr)

    def test_test_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            ScriptError("MOCK test failure"),
            ScriptError("MOCK test failure again"),
        ])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure again' patch='197'
run_webkit_patch: ['build-and-test', '--force-clean', '--no-update', '--build', '--test', '--non-interactive']
command_passed: success_message='Able to pass tests without patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, ScriptError)

    def test_red_test_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            ScriptError("MOCK test failure"),
            ScriptError("MOCK test failure again"),
            ScriptError("MOCK clean test failure"),
        ])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure again' patch='197'
run_webkit_patch: ['build-and-test', '--force-clean', '--no-update', '--build', '--test', '--non-interactive']
command_failed: failure_message='Unable to pass tests without patch (tree is red?)' script_error='MOCK clean test failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr)

    def test_land_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            ScriptError("MOCK land failure"),
        ])
        expected_stderr = """run_webkit_patch: ['apply-attachment', '--force-clean', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_passed: success_message='Passed tests' patch='197'
run_webkit_patch: ['land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
command_failed: failure_message='Unable to land patch' script_error='MOCK land failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, ScriptError)
