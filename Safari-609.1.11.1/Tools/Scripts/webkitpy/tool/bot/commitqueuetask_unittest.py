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
import logging
import unittest

from webkitpy.common.net import bugzilla
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.models import test_results
from webkitpy.layout_tests.models import test_failures
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.bot.commitqueuetask import *
from webkitpy.tool.mocktool import MockTool

_log = logging.getLogger(__name__)


class MockCommitQueue(CommitQueueTaskDelegate):
    def __init__(self, error_plan):
        self._error_plan = error_plan
        self._failure_status_id = 0
        self._flaky_tests = []

    def run_command(self, command):
        _log.info("run_webkit_patch: %s" % command)
        if self._error_plan:
            error = self._error_plan.pop(0)
            if error:
                raise error

    def command_passed(self, success_message, patch):
        _log.info("command_passed: success_message='%s' patch='%s'" % (
            success_message, patch.id()))

    def command_failed(self, failure_message, script_error, patch):
        _log.info("command_failed: failure_message='%s' script_error='%s' patch='%s'" % (
            failure_message, script_error, patch.id()))
        self._failure_status_id += 1
        return self._failure_status_id

    def refetch_patch(self, patch):
        return patch

    def test_results(self):
        return LayoutTestResults(test_results=[], did_exceed_test_failure_limit=False)

    def report_flaky_tests(self, patch, flaky_results, results_archive):
        current_flaky_tests = [result.test_name for result in flaky_results]
        self._flaky_tests += current_flaky_tests
        _log.info("report_flaky_tests: patch='%s' flaky_tests='%s' archive='%s'" % (patch.id(), current_flaky_tests, results_archive.filename))

    def get_reported_flaky_tests(self):
        return self._flaky_tests

    def archive_last_test_results(self, patch):
        _log.info("archive_last_test_results: patch='%s'" % patch.id())
        archive = Mock()
        archive.filename = "mock-archive-%s.zip" % patch.id()
        return archive

    def build_style(self):
        return "release"

    def did_pass_testing_ews(self, patch):
        return False


class FailingTestCommitQueue(MockCommitQueue):
    def __init__(self, error_plan, test_failure_plan):
        MockCommitQueue.__init__(self, error_plan)
        self._test_run_counter = -1  # Special value to indicate tests have never been run.
        self._test_failure_plan = test_failure_plan

    def run_command(self, command):
        if command[0] == "build-and-test":
            self._test_run_counter += 1
        MockCommitQueue.run_command(self, command)

    def _mock_test_result(self, testname):
        return test_results.TestResult(testname, [test_failures.FailureTextMismatch()])

    def test_results(self):
        # Doesn't make sense to ask for the test_results until the tests have run at least once.
        assert(self._test_run_counter >= 0)
        failures_for_run = self._test_failure_plan[self._test_run_counter]
        assert(isinstance(failures_for_run, list))
        results = LayoutTestResults(test_results=map(self._mock_test_result, failures_for_run), did_exceed_test_failure_limit=(len(failures_for_run) >= 10))
        return results


class PatchAnalysisResult(object):
    FAIL = "Fail"
    DEFER = "Defer"
    PASS = "Pass"


class MockSimpleTestPlanCommitQueue(MockCommitQueue):
    def __init__(self, first_test_failures, second_test_failures, clean_test_failures):
        MockCommitQueue.__init__(self, [])
        self._did_run_clean_tests = False
        self._patch_test_results = [first_test_failures, second_test_failures]
        self._clean_test_results = [clean_test_failures]
        self._current_test_results = []

    def run_command(self, command):
        MockCommitQueue.run_command(self, command)
        if command[0] == "build-and-test":
            if "--no-clean" in command:
                self._current_test_results = self._patch_test_results.pop(0)
            else:
                self._current_test_results = self._clean_test_results.pop(0)
                self._did_run_clean_tests = True

            if self._current_test_results:
                raise ScriptError("MOCK test failure")

    def _mock_test_result(self, testname):
        return test_results.TestResult(testname, [test_failures.FailureTextMismatch()])

    def test_results(self):
        assert(isinstance(self._current_test_results, list))
        return LayoutTestResults(test_results=map(self._mock_test_result, self._current_test_results), did_exceed_test_failure_limit=(len(self._current_test_results) >= 10))

    def did_run_clean_tests(self):
        return self._did_run_clean_tests


# We use GoldenScriptError to make sure that the code under test throws the
# correct (i.e., golden) exception.
class GoldenScriptError(ScriptError):
    pass


_lots_of_failing_tests = map(lambda num: "test-%s.html" % num, range(0, 100))


class CommitQueueTaskTest(unittest.TestCase):
    def _run_and_expect_patch_analysis_result(self, commit_queue, expected_analysis_result, expected_reported_flaky_tests=[], expect_clean_tests_to_run=False, expected_failure_status_id=0):
        tool = MockTool(log_executive=True)
        patch = tool.bugs.fetch_attachment(10000)
        task = CommitQueueTask(commit_queue, patch)

        try:
            result = task.run()
            if result:
                analysis_result = PatchAnalysisResult.PASS
            else:
                analysis_result = PatchAnalysisResult.DEFER
        except ScriptError:
            analysis_result = PatchAnalysisResult.FAIL

        self.assertEqual(analysis_result, expected_analysis_result)
        self.assertEqual(frozenset(commit_queue.get_reported_flaky_tests()), frozenset(expected_reported_flaky_tests))
        self.assertEqual(commit_queue.did_run_clean_tests(), expect_clean_tests_to_run)

        # The failure status only means anything if we actually failed.
        if expected_analysis_result == PatchAnalysisResult.FAIL:
            self.assertEqual(task.failure_status_id, expected_failure_status_id)
            self.assertIsInstance(task.results_from_patch_test_run(patch), LayoutTestResults)

    def _run_through_task(self, commit_queue, expected_logs, expected_exception=None, expect_retry=False):
        self.maxDiff = None
        tool = MockTool(log_executive=True)
        patch = tool.bugs.fetch_attachment(10000)
        task = CommitQueueTask(commit_queue, patch)
        success = OutputCapture().assert_outputs(self, task.run, expected_logs=expected_logs, expected_exception=expected_exception)
        if not expected_exception:
            self.assertEqual(success, not expect_retry)
        return task

    def test_success_case(self):
        commit_queue = MockCommitQueue([])
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='10000'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 10000]
command_passed: success_message='Applied patch' patch='10000'
run_webkit_patch: ['validate-changelog', '--check-oops', '--non-interactive', 10000]
command_passed: success_message='ChangeLog validated' patch='10000'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=release']
command_passed: success_message='Built patch' patch='10000'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive', '--build-style=release']
command_passed: success_message='Passed tests' patch='10000'
run_webkit_patch: ['land-attachment', '--force-clean', '--non-interactive', '--parent-command=commit-queue', 10000]
command_passed: success_message='Landed patch' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs)

    def test_fast_success_case(self):
        commit_queue = MockCommitQueue([])
        commit_queue.did_pass_testing_ews = lambda patch: True
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='10000'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 10000]
command_passed: success_message='Applied patch' patch='10000'
run_webkit_patch: ['validate-changelog', '--check-oops', '--non-interactive', 10000]
command_passed: success_message='ChangeLog validated' patch='10000'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=release']
command_passed: success_message='Built patch' patch='10000'
run_webkit_patch: ['land-attachment', '--force-clean', '--non-interactive', '--parent-command=commit-queue', 10000]
command_passed: success_message='Landed patch' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs)

    def test_clean_failure(self):
        commit_queue = MockCommitQueue([
            ScriptError("MOCK clean failure"),
        ])
        expected_logs = """run_webkit_patch: ['clean']
command_failed: failure_message='Unable to clean working directory' script_error='MOCK clean failure' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs, expect_retry=True)

    def test_update_failure(self):
        commit_queue = MockCommitQueue([
            None,
            ScriptError("MOCK update failure"),
        ])
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_failed: failure_message='Unable to update working directory' script_error='MOCK update failure' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs, expect_retry=True)

    def test_apply_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            GoldenScriptError("MOCK apply failure"),
        ])
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='10000'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 10000]
command_failed: failure_message='Patch does not apply' script_error='MOCK apply failure' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs, GoldenScriptError)

    def test_validate_changelog_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            GoldenScriptError("MOCK validate failure"),
        ])
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='10000'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 10000]
command_passed: success_message='Applied patch' patch='10000'
run_webkit_patch: ['validate-changelog', '--check-oops', '--non-interactive', 10000]
command_failed: failure_message='ChangeLog did not pass validation' script_error='MOCK validate failure' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs, GoldenScriptError)

    def test_build_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            GoldenScriptError("MOCK build failure"),
        ])
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='10000'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 10000]
command_passed: success_message='Applied patch' patch='10000'
run_webkit_patch: ['validate-changelog', '--check-oops', '--non-interactive', 10000]
command_passed: success_message='ChangeLog validated' patch='10000'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=release']
command_failed: failure_message='Patch does not build' script_error='MOCK build failure' patch='10000'
run_webkit_patch: ['build', '--force-clean', '--no-update', '--build-style=release']
command_passed: success_message='Able to build without patch' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs, GoldenScriptError)

    def test_red_build_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            ScriptError("MOCK build failure"),
            ScriptError("MOCK clean build failure"),
        ])
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='10000'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 10000]
command_passed: success_message='Applied patch' patch='10000'
run_webkit_patch: ['validate-changelog', '--check-oops', '--non-interactive', 10000]
command_passed: success_message='ChangeLog validated' patch='10000'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=release']
command_failed: failure_message='Patch does not build' script_error='MOCK build failure' patch='10000'
run_webkit_patch: ['build', '--force-clean', '--no-update', '--build-style=release']
command_failed: failure_message='Unable to build without patch' script_error='MOCK clean build failure' patch='10000'
"""
        self._run_through_task(commit_queue, expected_logs, expect_retry=True)

    def test_land_failure(self):
        commit_queue = MockCommitQueue([
            None,
            None,
            None,
            None,
            None,
            None,
            GoldenScriptError("MOCK land failure"),
        ])
        expected_logs = """run_webkit_patch: ['clean']
command_passed: success_message='Cleaned working directory' patch='10000'
run_webkit_patch: ['update']
command_passed: success_message='Updated working directory' patch='10000'
run_webkit_patch: ['apply-attachment', '--no-update', '--non-interactive', 10000]
command_passed: success_message='Applied patch' patch='10000'
run_webkit_patch: ['validate-changelog', '--check-oops', '--non-interactive', 10000]
command_passed: success_message='ChangeLog validated' patch='10000'
run_webkit_patch: ['build', '--no-clean', '--no-update', '--build-style=release']
command_passed: success_message='Built patch' patch='10000'
run_webkit_patch: ['build-and-test', '--no-clean', '--no-update', '--test', '--non-interactive', '--build-style=release']
command_passed: success_message='Passed tests' patch='10000'
run_webkit_patch: ['land-attachment', '--force-clean', '--non-interactive', '--parent-command=commit-queue', 10000]
command_failed: failure_message='Unable to land patch' script_error='MOCK land failure' patch='10000'
"""
        # FIXME: This should really be expect_retry=True for a better user experiance.
        self._run_through_task(commit_queue, expected_logs, GoldenScriptError)

    def test_failed_archive(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1"],
            second_test_failures=[],
            clean_test_failures=[])

        # It's possible for the delegate to fail to archive layout tests,
        # but we shouldn't try to report flaky tests when that happens.
        commit_queue.archive_last_test_results = lambda patch: None

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.PASS)

    def test_double_flaky_test_failure(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1"],
            second_test_failures=["Fail2"],
            clean_test_failures=["Fail1"])

        # The (subtle) point of this test is that report_flaky_tests does not get
        # called for this run.
        # Note also that there is no attempt to run the tests w/o the patch.
        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expected_reported_flaky_tests=["Fail1", "Fail2"])

    def test_test_failure(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1"],
            second_test_failures=["Fail1"],
            clean_test_failures=[])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.FAIL, expect_clean_tests_to_run=True, expected_failure_status_id=1)

    def test_red_test_failure(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1"],
            second_test_failures=["Fail1"],
            clean_test_failures=["Fail1"])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.PASS, expect_clean_tests_to_run=True)

    def test_first_failure_limit(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=_lots_of_failing_tests,
            second_test_failures=[],
            clean_test_failures=[])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True, expected_failure_status_id=1)

    def test_first_failure_limit_with_some_tree_redness(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=_lots_of_failing_tests,
            second_test_failures=["Fail1", "Fail2", "Fail3"],
            clean_test_failures=["Fail1", "Fail2", "Fail3"])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True, expected_failure_status_id=1)

    def test_second_failure_limit(self):
        # There need to be some failures in the first set of tests, or it won't even make it to the second test.
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1", "Fail2", "Fail3"],
            second_test_failures=_lots_of_failing_tests,
            clean_test_failures=["Fail1", "Fail2", "Fail3"])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True, expected_failure_status_id=2)

    def test_tree_failure_limit_with_patch_that_potentially_fixes_some_redness(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1", "Fail2", "Fail3"],
            second_test_failures=["Fail1", "Fail2", "Fail3"],
            clean_test_failures=_lots_of_failing_tests)

        # Unfortunately there are cases where the clean build will randomly fail enough tests to hit the failure limit.
        # With that in mind, we can't actually know that this patch is good or bad until we see a clean run that doesn't
        # exceed the failure limit.
        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True)

    def test_first_and_second_failure_limit(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=_lots_of_failing_tests,
            second_test_failures=_lots_of_failing_tests,
            clean_test_failures=[])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.FAIL, expect_clean_tests_to_run=True, expected_failure_status_id=1)

    def test_first_and_clean_failure_limit(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=_lots_of_failing_tests,
            second_test_failures=[],
            clean_test_failures=_lots_of_failing_tests)

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True)

    def test_first_second_and_clean_failure_limit(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=_lots_of_failing_tests,
            second_test_failures=_lots_of_failing_tests,
            clean_test_failures=_lots_of_failing_tests)

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True)

    def test_red_tree_patch_rejection(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1", "Fail2"],
            second_test_failures=["Fail1", "Fail2"],
            clean_test_failures=["Fail1"])

        # failure_status_id should be of the test with patch (1), not the test without patch (2).
        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.FAIL, expect_clean_tests_to_run=True, expected_failure_status_id=1)

    def test_two_flaky_tests(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1"],
            second_test_failures=["Fail2"],
            clean_test_failures=["Fail1", "Fail2"])

        # FIXME: This should pass, but as of right now, it defers.
        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expected_reported_flaky_tests=["Fail1", "Fail2"])

    def test_one_flaky_test(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1"],
            second_test_failures=[],
            clean_test_failures=[])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.PASS, expected_reported_flaky_tests=["Fail1"])

    def test_very_flaky_patch(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1", "Fail2", "Fail3", "Fail4", "Fail5"],
            second_test_failures=["Fail6", "Fail7", "Fail8", "Fail9", "Fail10"],
            clean_test_failures=[])

        # FIXME: This should actually fail, but right now it defers
        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expected_reported_flaky_tests=["Fail1", "Fail2", "Fail3", "Fail4", "Fail5", "Fail6", "Fail7", "Fail8", "Fail9", "Fail10"])

    def test_very_flaky_patch_with_some_tree_redness(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["PreExistingFail1", "PreExistingFail2", "Fail1", "Fail2", "Fail3", "Fail4", "Fail5"],
            second_test_failures=["PreExistingFail1", "PreExistingFail2", "Fail6", "Fail7", "Fail8", "Fail9", "Fail10"],
            clean_test_failures=["PreExistingFail1", "PreExistingFail2"])

        # FIXME: This should actually fail, but right now it defers
        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True, expected_reported_flaky_tests=["Fail1", "Fail2", "Fail3", "Fail4", "Fail5", "Fail6", "Fail7", "Fail8", "Fail9", "Fail10"])

    def test_different_test_failures(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1", "Fail2", "Fail3", "Fail4", "Fail5", "Fail6"],
            second_test_failures=["Fail1", "Fail2", "Fail3", "Fail4", "Fail5"],
            clean_test_failures=[])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.FAIL, expect_clean_tests_to_run=True, expected_reported_flaky_tests=["Fail6"], expected_failure_status_id=1)

    def test_different_test_failures_with_some_tree_redness(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["PreExistingFail1", "PreExistingFail2", "Fail1", "Fail2", "Fail3", "Fail4", "Fail5", "Fail6"],
            second_test_failures=["PreExistingFail1", "PreExistingFail2", "Fail1", "Fail2", "Fail3", "Fail4", "Fail5"],
            clean_test_failures=["PreExistingFail1", "PreExistingFail2"])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.FAIL, expect_clean_tests_to_run=True, expected_reported_flaky_tests=["Fail6"], expected_failure_status_id=1)

    def test_different_test_failures_with_some_tree_redness_and_some_fixes(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["PreExistingFail1", "Fail1", "Fail2", "Fail3", "Fail4", "Fail5", "Fail6"],
            second_test_failures=["PreExistingFail1", "Fail1", "Fail2", "Fail3", "Fail4", "Fail5"],
            clean_test_failures=["PreExistingFail1", "PreExistingFail2"])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.FAIL, expect_clean_tests_to_run=True, expected_reported_flaky_tests=["Fail6"], expected_failure_status_id=1)

    def test_mildly_flaky_patch(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1"],
            second_test_failures=["Fail2"],
            clean_test_failures=[])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=False, expected_reported_flaky_tests=["Fail1", "Fail2"])

    def test_mildly_flaky_patch_with_some_tree_redness(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["PreExistingFail1", "PreExistingFail2", "Fail1"],
            second_test_failures=["PreExistingFail1", "PreExistingFail2", "Fail2"],
            clean_test_failures=["PreExistingFail1", "PreExistingFail2"])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.DEFER, expect_clean_tests_to_run=True, expected_reported_flaky_tests=["Fail1", "Fail2"])

    def test_tree_more_red_than_patch(self):
        commit_queue = MockSimpleTestPlanCommitQueue(
            first_test_failures=["Fail1", "Fail2", "Fail3"],
            second_test_failures=["Fail1", "Fail2", "Fail3"],
            clean_test_failures=["Fail1", "Fail2", "Fail3", "Fail4"])

        self._run_and_expect_patch_analysis_result(commit_queue, PatchAnalysisResult.PASS, expect_clean_tests_to_run=True)

    def _expect_validate(self, patch, is_valid):
        class MockDelegate(object):
            def refetch_patch(self, patch):
                return patch

        task = CommitQueueTask(MockDelegate(), patch)
        self.assertEqual(task.validate(), is_valid)

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
