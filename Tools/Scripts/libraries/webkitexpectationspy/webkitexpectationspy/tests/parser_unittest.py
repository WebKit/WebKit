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

"""Tests for expectation parser."""

import unittest

from webkitexpectationspy.parser import ExpectationParser
from webkitexpectationspy.expectations import ResultStatus
from webkitexpectationspy.modifiers import ModifiersBase
from webkitexpectationspy.suites.api_tests import APITestSuite


PASS = ResultStatus.PASS
FAIL = ResultStatus.FAIL
CRASH = ResultStatus.CRASH
TIMEOUT = ResultStatus.TIMEOUT


class ExpectationParserTest(unittest.TestCase):

    def setUp(self):
        self.parser = ExpectationParser()

    def test_parse_simple_failure(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Fail ]')
        self.assertEqual(len(results), 1)
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)
        self.assertEqual(exp.test_pattern, 'TestWebKitAPI.WTF.Test')
        self.assertIn(FAIL, exp.expected)

    def test_parse_multiple_expectations(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Pass Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(PASS, exp.expected)
        self.assertIn(FAIL, exp.expected)
        self.assertTrue(exp.flaky)

    def test_parse_with_webkit_bug(self):
        results = self.parser.parse('test.txt', 'webkit.org/b/12345 TestWebKitAPI.WTF.Test [ Crash ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(exp.bug_ids, ('webkit.org/b/12345',))
        self.assertIn(CRASH, exp.expected)

    def test_parse_with_radar_bug(self):
        results = self.parser.parse('test.txt', 'rdar://12345 TestWebKitAPI.WTF.Test [ Crash ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(exp.bug_ids, ('rdar://12345',))

    def test_parse_with_radar_problem(self):
        results = self.parser.parse('test.txt', 'rdar://problem/12345 TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertEqual(exp.bug_ids, ('rdar://problem/12345',))

    def test_parse_skip(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Skip ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.skip)
        self.assertTrue(exp.modifiers.skip)

    def test_parse_slow(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.slow)
        self.assertIsNotNone(exp.modifiers.slow)

    def test_parse_slow_with_timeout(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Slow:120s ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.slow)
        self.assertEqual(exp.modifiers.slow, 120.0)

    def test_parse_configuration_debug(self):
        results = self.parser.parse('test.txt', '[ Debug ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('debug', exp.configurations)

    def test_parse_configuration_multiple(self):
        results = self.parser.parse('test.txt', '[ mac arm64 ] TestWebKitAPI.WTF.Test [ Crash ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('mac', exp.configurations)
        self.assertIn('arm64', exp.configurations)

    def test_parse_wildcard(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.* [ Skip ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.test_pattern.endswith('*'))

    def test_parse_comment_line(self):
        results = self.parser.parse('test.txt', '# This is a comment')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertEqual(len(warnings), 0)

    def test_parse_empty_line(self):
        results = self.parser.parse('test.txt', '')
        exp, warnings = results[0]
        self.assertIsNone(exp)
        self.assertEqual(len(warnings), 0)

    def test_parse_skip_with_other_expectations(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Skip Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)
        self.assertTrue(exp.skip)
        self.assertIn(FAIL, exp.expected)

    def test_parse_version_specifier(self):
        parser = ExpectationParser()
        results = parser.parse('test.txt', '[ mac Sonoma+ ] TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn('mac', exp.configurations)
        self.assertEqual(len(exp.version_specifiers), 1)

    def test_parse_no_expectations_defaults_to_pass(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(PASS, exp.expected)

    def test_parse_combined_modifiers(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Skip Slow Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.skip)
        self.assertTrue(exp.slow)
        self.assertIn(FAIL, exp.expected)

    def test_case_insensitive_status_lookup(self):
        self.assertEqual(ResultStatus.from_string('fail'), ResultStatus.FAIL)
        self.assertEqual(ResultStatus.from_string('PASS'), ResultStatus.PASS)
        self.assertEqual(ResultStatus.from_string('Crash'), ResultStatus.CRASH)


class ExpectationParserWithSuiteTest(unittest.TestCase):

    def setUp(self):
        self.parser = ExpectationParser(suite=APITestSuite())

    def test_parse_with_suite(self):
        results = self.parser.parse('test.txt', 'TestWebKitAPI.WTF.Test [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIsNotNone(exp)


if __name__ == '__main__':
    unittest.main()
