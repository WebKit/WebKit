# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Tests for ExpectationsManager."""

import unittest

from webkitexpectationspy.manager import ExpectationsManager
from webkitexpectationspy.suites.api_tests import APITestSuite
from webkitexpectationspy.expectations import ResultStatus

PASS = ResultStatus.PASS
FAIL = ResultStatus.FAIL
CRASH = ResultStatus.CRASH
TIMEOUT = ResultStatus.TIMEOUT


class ExpectationsManagerTest(unittest.TestCase):

    def test_create_with_default_suite(self):
        manager = ExpectationsManager()
        self.assertIsNotNone(manager)

    def test_create_with_custom_suite(self):
        suite = APITestSuite()
        manager = ExpectationsManager(suite=suite)
        self.assertEqual(manager._suite, suite)

    def test_load_content(self):
        manager = ExpectationsManager()
        warnings = manager.load_content('test.txt', 'TestWebKitAPI.WTF.Test [ Fail ]')
        self.assertEqual(len(warnings), 0)

        exp = manager.get_expectation('TestWebKitAPI.WTF.Test')
        self.assertIsNotNone(exp)
        self.assertIn(FAIL, exp.expected)

    def test_load_multiple_content(self):
        manager = ExpectationsManager()
        manager.load_content('file1.txt', 'Test1 [ Fail ]')
        manager.load_content('file2.txt', 'Test2 [ Skip ]')

        exp1 = manager.get_expectation('Test1')
        exp2 = manager.get_expectation('Test2')
        self.assertIn(FAIL, exp1.expected)
        self.assertTrue(exp2.skip)

    def test_get_expectation_with_configuration(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', '''[ debug ] TestDebug.Test [ Fail ]
[ release ] TestRelease.Test [ Pass ]''')

        exp_debug = manager.get_expectation('TestDebug.Test', current_config={'debug'})
        self.assertIsNotNone(exp_debug)
        self.assertIn(FAIL, exp_debug.expected)

        exp_release = manager.get_expectation('TestRelease.Test', current_config={'release'})
        self.assertIsNotNone(exp_release)
        self.assertIn(PASS, exp_release.expected)

        exp_wrong = manager.get_expectation('TestDebug.Test', current_config={'release'})
        self.assertIsNone(exp_wrong)

    def test_get_skipped_tests(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', '''Test1 [ Skip ]
Test2 [ Fail ]
TestWTF.* [ Skip ]''')

        all_tests = ['Test1', 'Test2', 'TestWTF.A', 'TestWTF.B', 'Test3']
        skipped = manager.get_skipped_tests(all_tests)

        self.assertIn('Test1', skipped)
        self.assertNotIn('Test2', skipped)
        self.assertIn('TestWTF.A', skipped)
        self.assertIn('TestWTF.B', skipped)
        self.assertNotIn('Test3', skipped)

    def test_get_slow_tests(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', '''Test1 [ Slow ]
Test2 [ Slow:120s ]
Test3 [ Fail ]''')

        all_tests = ['Test1', 'Test2', 'Test3']
        slow = manager.get_slow_tests(all_tests)

        self.assertIn('Test1', slow)
        self.assertIsNone(slow['Test1'])
        self.assertEqual(slow['Test2'], 120.0)
        self.assertNotIn('Test3', slow)

    def test_lint_returns_warnings(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', '[ arm64 mac ] Test [ Fail ]')

        warnings = manager.lint()
        self.assertTrue(any('order' in str(w) for w in warnings))

    def test_all_expectations(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', '''Test1 [ Fail ]
Test2 [ Skip ]''')

        all_exps = manager.all_expectations()
        self.assertEqual(len(all_exps), 2)

    def test_clear(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', 'Test [ Fail ]')

        self.assertIsNotNone(manager.get_expectation('Test'))
        manager.clear()
        self.assertIsNone(manager.get_expectation('Test'))

    def test_skip_with_expected_result(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', 'Test [ Skip Fail ]')

        exp = manager.get_expectation('Test')
        self.assertIsNotNone(exp)
        self.assertTrue(exp.skip)
        self.assertIn(FAIL, exp.expected)


class ExpectationsManagerMultiSuiteTest(unittest.TestCase):

    def test_multiple_test_suites(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', '''TestWebKitAPI.WTF.String [ Fail ]
TestIPC.Connection.Test [ Skip ]
TestWGSL.Shaders.Compile [ Crash ]
TestWTF.Memory.Leak [ Timeout ]''')

        self.assertIn(FAIL, manager.get_expectation('TestWebKitAPI.WTF.String').expected)
        self.assertTrue(manager.get_expectation('TestIPC.Connection.Test').skip)
        self.assertIn(CRASH, manager.get_expectation('TestWGSL.Shaders.Compile').expected)
        self.assertIn(TIMEOUT, manager.get_expectation('TestWTF.Memory.Leak').expected)

    def test_wildcard_across_suites(self):
        manager = ExpectationsManager()
        manager.load_content('exp.txt', '''TestIPC.* [ Skip ]
TestWGSL.Shaders.* [ Fail ]''')

        all_tests = [
            'TestIPC.Connection.Test',
            'TestIPC.Messages.Send',
            'TestWGSL.Shaders.Compile',
            'TestWGSL.Shaders.Link',
            'TestWGSL.Memory.Alloc',
            'TestWebKitAPI.WTF.String',
        ]
        skipped = manager.get_skipped_tests(all_tests)

        self.assertIn('TestIPC.Connection.Test', skipped)
        self.assertIn('TestIPC.Messages.Send', skipped)
        self.assertNotIn('TestWGSL.Shaders.Compile', skipped)
        self.assertNotIn('TestWebKitAPI.WTF.String', skipped)


if __name__ == '__main__':
    unittest.main()
