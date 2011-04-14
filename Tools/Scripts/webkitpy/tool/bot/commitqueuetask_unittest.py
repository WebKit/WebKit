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

from webkitpy.common.net import bugzilla
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.deprecated_logging import error, log
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.layout_package import test_results
from webkitpy.layout_tests.layout_package import test_failures
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

    def report_flaky_tests(self, patch, flaky_results, results_archive):
        flaky_tests = [result.filename for result in flaky_results]
        log("report_flaky_tests: patch='%s' flaky_tests='%s' archive='%s'" % (patch.id(), flaky_tests, results_archive.filename))

    def archive_last_layout_test_results(self, patch):
        log("archive_last_layout_test_results: patch='%s'" % patch.id())
        archive = Mock()
        archive.filename = "mock-archive-%s.zip" % patch.id()
        return archive


class CommitQueueTaskTest(unittest.TestCase):
    def _run_through_task(self, commit_queue, expected_stderr, expected_exception=None, expect_retry=False):
        tool = MockTool(log_executive=True)
        patch = tool.bugs.fetch_attachment(197)
        task = CommitQueueTask(commit_queue, patch)
        success = OutputCapture().assert_outputs(self, task.run, expected_stderr=expected_stderr, expected_exception=expected_exception)
        if not expected_exception:
            self.assertEqual(success, not expect_retry)

    def test_success_case(self):
        commit_queue = MockCommitQueue([])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_passed: success_message='Passed tests' patch='197'
run_webkit_patch: ['land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
command_passed: success_message='Landed patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr)

    def test_clean_failure(self):
        commit_queue = MockCommitQueue([
            ScriptError("MOCK clean failure"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_failed: failure_message='Unable to clean working directory' script_error='MOCK clean failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, expect_retry=True)

    def test_update_failure(self):
        commit_queue = MockCommitQueue([
            None,
            ScriptError("MOCK update failure"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_failed: failure_message='Unable to update working directory' script_error='MOCK update failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, expect_retry=True)

    def test_apply_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            ScriptError("MOCK apply failure"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_failed: failure_message='Patch does not apply' script_error='MOCK apply failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, ScriptError)

    def test_build_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            ScriptError("MOCK build failure"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
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
            None,
            None,
            ScriptError("MOCK build failure"),
            ScriptError("MOCK clean build failure"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_failed: failure_message='Patch does not build' script_error='MOCK build failure' patch='197'
run_webkit_patch: ['build', '--force-clean', '--no-update', '--build-style=both']
command_failed: failure_message='Unable to build without patch' script_error='MOCK clean build failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, expect_retry=True)

    def test_flaky_test_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            ScriptError("MOCK tests failure"),
        ])
        # CommitQueueTask will only report flaky tests if we successfully parsed
        # results.html and returned a LayoutTestResults object, so we fake one.
        commit_queue.layout_test_results = lambda: LayoutTestResults([])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK tests failure' patch='197'
archive_last_layout_test_results: patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_passed: success_message='Passed tests' patch='197'
report_flaky_tests: patch='197' flaky_tests='[]' archive='mock-archive-197.zip'
run_webkit_patch: ['land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
command_passed: success_message='Landed patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr)

    def test_failed_archive(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            ScriptError("MOCK tests failure"),
        ])
        commit_queue.layout_test_results = lambda: LayoutTestResults([])
        # It's possible delegate to fail to archive layout tests, don't try to report
        # flaky tests when that happens.
        commit_queue.archive_last_layout_test_results = lambda patch: None
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK tests failure' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_passed: success_message='Passed tests' patch='197'
run_webkit_patch: ['land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
command_passed: success_message='Landed patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr)

    def test_double_flaky_test_failure(self):
        class DoubleFlakyCommitQueue(MockCommitQueue):
            def __init__(self, error_plan):
                MockCommitQueue.__init__(self, error_plan)
                self._double_flaky_test_counter = 0

            def run_command(self, command):
                self._double_flaky_test_counter += 1
                MockCommitQueue.run_command(self, command)

            def _mock_test_result(self, testname):
                return test_results.TestResult(testname, [test_failures.FailureTextMismatch()])

            def layout_test_results(self):
                if self._double_flaky_test_counter % 2:
                    return LayoutTestResults([self._mock_test_result('foo.html')])
                return LayoutTestResults([self._mock_test_result('bar.html')])

        commit_queue = DoubleFlakyCommitQueue([
            None,
            None,
            None,
            None,
            ScriptError("MOCK test failure"),
            ScriptError("MOCK test failure again"),
        ])
        # The (subtle) point of this test is that report_flaky_tests does not appear
        # in the expected_stderr for this run.
        # Note also that there is no attempt to run the tests w/o the patch.
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure' patch='197'
archive_last_layout_test_results: patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure again' patch='197'
"""
        tool = MockTool(log_executive=True)
        patch = tool.bugs.fetch_attachment(197)
        task = CommitQueueTask(commit_queue, patch)
        success = OutputCapture().assert_outputs(self, task.run, expected_stderr=expected_stderr)
        self.assertEqual(success, False)

    def test_test_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            ScriptError("MOCK test failure"),
            ScriptError("MOCK test failure again"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure' patch='197'
archive_last_layout_test_results: patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure again' patch='197'
archive_last_layout_test_results: patch='197'
run_webkit_patch: ['build-and-test', '--force-clean', '--no-update', '--build', '--test', '--non-interactive']
command_passed: success_message='Able to pass tests without patch' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, ScriptError)

    def test_red_test_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            ScriptError("MOCK test failure"),
            ScriptError("MOCK test failure again"),
            ScriptError("MOCK clean test failure"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure' patch='197'
archive_last_layout_test_results: patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_failed: failure_message='Patch does not pass tests' script_error='MOCK test failure again' patch='197'
archive_last_layout_test_results: patch='197'
run_webkit_patch: ['build-and-test', '--force-clean', '--no-update', '--build', '--test', '--non-interactive']
command_failed: failure_message='Unable to pass tests without patch (tree is red?)' script_error='MOCK clean test failure' patch='197'
"""
        self._run_through_task(commit_queue, expected_stderr, expect_retry=True)

    def test_land_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            None,
            ScriptError("MOCK land failure"),
        ])
        expected_stderr = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='197'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='197'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 197]
command_passed: success_message='Applied patch' patch='197'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=both']
command_passed: success_message='Built patch' patch='197'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive']
command_passed: success_message='Passed tests' patch='197'
run_webkit_patch: ['land-attachment', '--force-clean', '--ignore-builders', '--non-interactive', '--parent-command=commit-queue', 197]
command_failed: failure_message='Unable to land patch' script_error='MOCK land failure' patch='197'
"""
        # FIXME: This should really be expect_retry=True for a better user experiance.
        self._run_through_task(commit_queue, expected_stderr, ScriptError)

    def _expect_validate(self, patch, is_valid):
        class MockDelegate(object):
            def refetch_patch(self, patch):
                return patch

        task = CommitQueueTask(MockDelegate(), patch)
        self.assertEquals(task._validate(), is_valid)

    def _mock_patch(self, attachment_dict={}, bug_dict={'bug_status': 'NEW'}, committer="fake"):
        bug = bugzilla.Bug(bug_dict, None)
        patch = bugzilla.Attachment(attachment_dict, bug)
        patch._committer = committer
        return patch

    def test_validate(self):
        self._expect_validate(self._mock_patch(), True)
        self._expect_validate(self._mock_patch({'is_obsolete': True}), False)
        self._expect_validate(self._mock_patch(bug_dict={'bug_status': 'CLOSED'}), False)
        self._expect_validate(self._mock_patch(committer=None), False)
        self._expect_validate(self._mock_patch({'review': '-'}), False)
