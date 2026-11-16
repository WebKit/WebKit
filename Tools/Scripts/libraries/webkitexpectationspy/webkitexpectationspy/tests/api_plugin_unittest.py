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

"""Tests for API test suite."""

import unittest

from webkitexpectationspy.suites.api_tests import APITestSuite
from webkitexpectationspy.expectations import ResultStatus

PASS = ResultStatus.PASS
FAIL = ResultStatus.FAIL
CRASH = ResultStatus.CRASH
TIMEOUT = ResultStatus.TIMEOUT


class APITestSuiteTest(unittest.TestCase):

    def setUp(self):
        self.suite = APITestSuite()

    def test_name(self):
        self.assertEqual(self.suite.name, 'api-tests')

    def test_expectation_map(self):
        exp_map = self.suite.expectation_map
        self.assertEqual(exp_map['pass'], PASS)
        self.assertEqual(exp_map['fail'], FAIL)
        self.assertEqual(exp_map['crash'], CRASH)
        self.assertEqual(exp_map['timeout'], TIMEOUT)

    def test_modifier_map(self):
        mod_map = self.suite.modifier_map
        self.assertIn('skip', mod_map)
        self.assertIn('slow', mod_map)
        self.assertNotIn('skip', self.suite.expectation_map)
        self.assertNotIn('slow', self.suite.expectation_map)

    def test_matches_test_exact(self):
        self.assertTrue(self.suite.matches_test('TestWebKitAPI.WTF.Test', 'TestWebKitAPI.WTF.Test'))
        self.assertFalse(self.suite.matches_test('TestWebKitAPI.WTF.Test', 'TestWebKitAPI.WTF.Other'))

    def test_matches_test_wildcard(self):
        self.assertTrue(self.suite.matches_test('TestWebKitAPI.WTF.*', 'TestWebKitAPI.WTF.Test'))
        self.assertTrue(self.suite.matches_test('TestWebKitAPI.WTF.*', 'TestWebKitAPI.WTF.Other'))
        self.assertFalse(self.suite.matches_test('TestWebKitAPI.WTF.*', 'TestWebKitAPI.WebKit.Test'))

    def test_matches_test_broad_wildcard(self):
        self.assertTrue(self.suite.matches_test('TestWebKitAPI.*', 'TestWebKitAPI.WTF.Test'))
        self.assertTrue(self.suite.matches_test('TestWebKitAPI.*', 'TestWebKitAPI.WebKit.Other'))
        self.assertFalse(self.suite.matches_test('TestWebKitAPI.*', 'TestIPC.Suite.Test'))

    def test_supports_testipc_suite(self):
        self.assertTrue(self.suite.matches_test('TestIPC.IPCTests.Test', 'TestIPC.IPCTests.Test'))
        self.assertTrue(self.suite.matches_test('TestIPC.IPCTests.*', 'TestIPC.IPCTests.AnyTest'))
        self.assertTrue(self.suite.matches_test('TestIPC.*', 'TestIPC.IPCTests.Test'))
        self.assertIsNotNone(self.suite.validate_test_pattern('TestIPC.IPCTests.Test'))
        self.assertIsNotNone(self.suite.validate_test_pattern('TestIPC.IPCTests.*'))

    def test_supports_testwgsl_suite(self):
        self.assertTrue(self.suite.matches_test('TestWGSL.Shaders.Test', 'TestWGSL.Shaders.Test'))
        self.assertTrue(self.suite.matches_test('TestWGSL.Shaders.*', 'TestWGSL.Shaders.AnyTest'))
        self.assertTrue(self.suite.matches_test('TestWGSL.*', 'TestWGSL.Shaders.Test'))
        self.assertIsNotNone(self.suite.validate_test_pattern('TestWGSL.Shaders.Test'))
        self.assertIsNotNone(self.suite.validate_test_pattern('TestWGSL.Shaders.*'))

    def test_supports_testwtf_suite(self):
        self.assertTrue(self.suite.matches_test('TestWTF.WTF.Test', 'TestWTF.WTF.Test'))
        self.assertTrue(self.suite.matches_test('TestWTF.WTF.*', 'TestWTF.WTF.AnyTest'))
        self.assertIsNotNone(self.suite.validate_test_pattern('TestWTF.WTF.Test'))

    def test_supports_any_binary_prefix(self):
        self.assertTrue(self.suite.matches_test('SomeNewBinary.Suite.Test', 'SomeNewBinary.Suite.Test'))
        self.assertTrue(self.suite.matches_test('InternalTest.Suite.*', 'InternalTest.Suite.Foo'))
        self.assertIsNotNone(self.suite.validate_test_pattern('CustomBinary.Suite.Test'))

    def test_validate_valid_patterns(self):
        valid_patterns = [
            'TestWebKitAPI.WTF.StringTest',
            'TestWebKitAPI.WTF.*',
            'TestWebKitAPI.*',
            'TestIPC.IPCTests.Connection',
            'TestWGSL.Shaders.Compile',
            'TestWTF.Memory.Buffer',
        ]
        for pattern in valid_patterns:
            self.assertIsNotNone(self.suite.validate_test_pattern(pattern),
                                 f"Pattern should be valid: {pattern}")

    def test_validate_invalid_patterns(self):
        invalid_patterns = [
            '',
            '*',
        ]
        for pattern in invalid_patterns:
            self.assertIsNone(self.suite.validate_test_pattern(pattern),
                              f"Pattern should be invalid: {pattern}")


if __name__ == '__main__':
    unittest.main()
