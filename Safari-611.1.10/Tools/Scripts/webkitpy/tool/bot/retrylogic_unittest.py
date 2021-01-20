# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import unittest

from webkitpy.common.net.jsctestresults import JSCTestResults
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.bot.patchanalysistask import *
from webkitpy.tool.commands.earlywarningsystem import AbstractEarlyWarningSystem
from webkitpy.tool.mocktool import MockTool

_log = logging.getLogger(__name__)


class MockPatchAnalysisTask(PatchAnalysisTask):
    def __init__(self, delegate, patch, patches_passed_all_tests):
        self._delegate = delegate
        self._patch = patch
        self._script_error = None
        self._results_archive_from_patch_test_run = None
        self._results_from_patch_test_run = None
        self.error = None
        self._patches_passed_all_tests = patches_passed_all_tests
        self._test_run_count = 0
        self.failure_status_id = 0

    def _test(self):
        self._test_run_count += 1

        if self._patches_passed_all_tests.pop() == True:
            return True
        self._script_error = ScriptError('Regression test')
        return False

    def _build_and_test_without_patch(self):
        self._test_run_count += 1
        return True

    def validate(self):
        return True

    def test_run_count(self):
        return self._test_run_count


# This is the delegate to be used with MockPatchAnalysisTask, above.
class MockJSCEarlyWarningSystem(AbstractEarlyWarningSystem):
    def __init__(self, first_test_results, second_test_results, clean_test_results):
        AbstractEarlyWarningSystem.__init__(self)
        self._group = 'jsc'
        self._results_in_order = [clean_test_results, second_test_results, first_test_results]

    def test_results(self):
        return self._results_in_order.pop()


class JSCEarlyWarningSystemTest(unittest.TestCase):
    def _results_indicate_all_passed(self, results):
        if results == None:
            return False
        return results.all_passed()

    def _create_task(self, first_test_results, second_test_results, clean_test_results):
        queue = MockJSCEarlyWarningSystem(first_test_results, second_test_results, clean_test_results)
        tool = MockTool(log_executive=True)
        patch = tool.bugs.fetch_attachment(10000)
        patches_passed_all_tests = list(map(self._results_indicate_all_passed, [second_test_results, first_test_results]))
        return MockPatchAnalysisTask(queue, patch, patches_passed_all_tests)

    def test_success_case(self):
        first_test_results = JSCTestResults(True, [])
        second_test_results = JSCTestResults(True, [])
        clean_test_results = JSCTestResults(True, [])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 1)
        self.assertTrue(return_value)

    def test_test_failure(self):
        first_test_results = JSCTestResults(True, ['Fail.js'])
        second_test_results = JSCTestResults(True, ['Fail.js'])
        clean_test_results = JSCTestResults(True, [])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        with self.assertRaises(ScriptError):
            return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 3)

    def test_fix(self):
        first_test_results = JSCTestResults(True, [])
        second_test_results = JSCTestResults(True, [])
        clean_test_results = JSCTestResults(True, ['Fail.js'])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 1)
        self.assertTrue(return_value)

    def test_ineffective_patch(self):
        first_test_results = JSCTestResults(False, ['failure1.js', 'failure2.js'])
        second_test_results = JSCTestResults(False, ['failure1.js', 'failure2.js'])
        clean_test_results = JSCTestResults(False, ['failure1.js', 'failure2.js'])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 3)
        self.assertTrue(return_value)

    def test_partially_effective_patch(self):
        first_test_results = JSCTestResults(True, ['failure2.js'])
        second_test_results = JSCTestResults(True, ['failure2.js'])
        clean_test_results = JSCTestResults(False, ['failure1.js', 'failure2.js'])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 3)
        self.assertTrue(return_value)

    def test_different_test_failures_in_patch_and_tree(self):
        first_test_results = JSCTestResults(False, [])
        second_test_results = JSCTestResults(False, [])
        clean_test_results = JSCTestResults(True, ['failure1.js', 'failure2.js'])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        with self.assertRaises(ScriptError):
            return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 3)

    def test_first_results_could_not_be_read(self):
        first_test_results = None
        second_test_results = JSCTestResults(True, [])
        clean_test_results = JSCTestResults(True, [])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 1)
        self.assertFalse(return_value)

    def test_second_results_could_not_be_read(self):
        first_test_results = JSCTestResults(False, ['failure1.js', 'failure2.js'])
        second_test_results = None
        clean_test_results = JSCTestResults(True, [])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 2)
        self.assertFalse(return_value)

    def test_clean_results_could_not_be_read(self):
        first_test_results = JSCTestResults(True, ['failure2.js'])
        second_test_results = JSCTestResults(True, ['failure2.js'])
        clean_test_results = None
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertEqual(task.test_run_count(), 3)
        self.assertFalse(return_value)

    def test_flaky_results_on_clean_tree_pass(self):
        first_test_results = JSCTestResults(True, ['failure2.js'])
        second_test_results = JSCTestResults(True, [])
        clean_test_results = JSCTestResults(True, [])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertTrue(return_value)
        self.assertEqual(task.test_run_count(), 2)

    def test_flaky_results_on_clean_tree_pass_v2(self):
        first_test_results = JSCTestResults(True, [])
        second_test_results = JSCTestResults(True, ['failure2.js'])
        clean_test_results = JSCTestResults(True, [])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertTrue(return_value)
        self.assertEqual(task.test_run_count(), 1)

    def test_flaky_results_on_clean_tree_failure(self):
        first_test_results = JSCTestResults(False, ['failure1.js', 'failure2.js'])
        second_test_results = JSCTestResults(True, ['failure2.js'])
        clean_test_results = JSCTestResults(True, [])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        with self.assertRaises(ScriptError):
            task._test_patch()
        self.assertEqual(task.test_run_count(), 3)

    def test_flaky_results_on_red_tree_pass(self):
        first_test_results = JSCTestResults(True, ['failure1.js'])
        second_test_results = JSCTestResults(True, ['failure1.js', 'failure2.js'])
        clean_test_results = JSCTestResults(True, ['failure1.js'])
        task = self._create_task(first_test_results, second_test_results, clean_test_results)

        return_value = task._test_patch()
        self.assertTrue(return_value)
        self.assertEqual(task.test_run_count(), 3)
