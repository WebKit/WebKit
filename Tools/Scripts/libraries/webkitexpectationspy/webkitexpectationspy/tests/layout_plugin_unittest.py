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

"""Tests for layout test suite."""

import unittest

from webkitexpectationspy.suites.layout_tests import LayoutTestSuite, LayoutTestStatus

PASS = LayoutTestStatus.PASS
FAIL = LayoutTestStatus.FAIL
CRASH = LayoutTestStatus.CRASH
TIMEOUT = LayoutTestStatus.TIMEOUT


class LayoutTestSuiteTest(unittest.TestCase):

    def setUp(self):
        self.suite = LayoutTestSuite()

    def test_name(self):
        self.assertEqual(self.suite.name, 'layout-tests')

    def test_expectation_map_basic(self):
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

    def test_expectation_map_layout_specific(self):
        exp_map = self.suite.expectation_map
        self.assertEqual(exp_map['imageonlyfailure'], LayoutTestStatus.IMAGE)
        self.assertEqual(exp_map['image'], LayoutTestStatus.IMAGE)
        self.assertEqual(exp_map['text'], LayoutTestStatus.TEXT)
        self.assertEqual(exp_map['audio'], LayoutTestStatus.AUDIO)
        self.assertEqual(exp_map['leak'], LayoutTestStatus.LEAK)

    def test_matches_test_exact(self):
        self.assertTrue(self.suite.matches_test(
            'fast/css/test.html', 'fast/css/test.html'))
        self.assertFalse(self.suite.matches_test(
            'fast/css/test.html', 'fast/css/other.html'))

    def test_matches_test_directory_with_slash(self):
        self.assertTrue(self.suite.matches_test('fast/css/', 'fast/css/test.html'))
        self.assertTrue(self.suite.matches_test('fast/css/', 'fast/css/subdir/test.html'))
        self.assertFalse(self.suite.matches_test('fast/css/', 'fast/dom/test.html'))

    def test_matches_test_wildcard(self):
        self.assertTrue(self.suite.matches_test('fast/css/*', 'fast/css/test.html'))
        self.assertFalse(self.suite.matches_test('fast/css/*', 'fast/dom/test.html'))

    def test_matches_test_prefix_wildcard(self):
        self.assertTrue(self.suite.matches_test('fast/css/border*', 'fast/css/border-radius.html'))
        self.assertFalse(self.suite.matches_test('fast/css/border*', 'fast/css/margin.html'))

    def test_matches_test_directory_without_slash(self):
        self.assertTrue(self.suite.matches_test('fast/css', 'fast/css/test.html'))
        self.assertFalse(self.suite.matches_test('fast/css', 'fast/dom/test.html'))

    def test_validate_test_pattern_valid(self):
        for pattern in ['fast/css/test.html', 'fast/css/', 'fast/css/*', 'fast']:
            self.assertIsNotNone(self.suite.validate_test_pattern(pattern))

    def test_validate_test_pattern_invalid(self):
        for pattern in ['', '/absolute/path/test.html']:
            self.assertIsNone(self.suite.validate_test_pattern(pattern))

    def test_version_tokens(self):
        tokens = self.suite.version_tokens
        self.assertIn('sonoma', tokens)
        self.assertIn('ventura', tokens)

    def test_version_order(self):
        order = self.suite.version_order
        self.assertTrue(order.index('monterey') < order.index('ventura'))
        self.assertTrue(order.index('ventura') < order.index('sonoma'))

    def test_is_wildcard_pattern(self):
        self.assertTrue(self.suite.is_wildcard_pattern('fast/css/*'))
        self.assertTrue(self.suite.is_wildcard_pattern('fast/css/'))
        self.assertTrue(self.suite.is_wildcard_pattern('fast/css'))
        self.assertFalse(self.suite.is_wildcard_pattern('fast/css/test.html'))

    def test_layout_test_status_enum(self):
        self.assertIsNotNone(LayoutTestStatus.TEXT)
        self.assertIsNotNone(LayoutTestStatus.IMAGE)
        self.assertIsNotNone(LayoutTestStatus.AUDIO)
        self.assertIsNotNone(LayoutTestStatus.LEAK)

    def test_layout_test_status_str(self):
        self.assertEqual(str(LayoutTestStatus.IMAGE), 'ImageOnlyFailure')
        self.assertEqual(str(LayoutTestStatus.IMAGE_PLUS_TEXT), 'ImagePlusText')
        self.assertEqual(str(LayoutTestStatus.PASS), 'Pass')

    def test_remove_pixel_failures(self):
        expected = LayoutTestStatus.IMAGE | LayoutTestStatus.TEXT
        result = self.suite.remove_pixel_failures(expected)
        self.assertNotIn(LayoutTestStatus.IMAGE, result)
        self.assertIn(LayoutTestStatus.TEXT, result)

    def test_remove_leak_failures(self):
        expected = LayoutTestStatus.LEAK | LayoutTestStatus.FAIL
        result = self.suite.remove_leak_failures(expected)
        self.assertNotIn(LayoutTestStatus.LEAK, result)
        self.assertIn(LayoutTestStatus.FAIL, result)


class LayoutTestSuiteIntegrationTest(unittest.TestCase):

    def test_parse_layout_test_expectations(self):
        from webkitexpectationspy.parser import ExpectationParser
        parser = ExpectationParser(suite=LayoutTestSuite())
        results = parser.parse('test.txt', 'fast/css/test.html [ Fail ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(FAIL, exp.expected)

    def test_parse_image_only_failure(self):
        from webkitexpectationspy.parser import ExpectationParser
        parser = ExpectationParser(suite=LayoutTestSuite())
        results = parser.parse('test.txt', 'fast/css/test.html [ ImageOnlyFailure ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(LayoutTestStatus.IMAGE, exp.expected)

    def test_parse_text_expectation(self):
        from webkitexpectationspy.parser import ExpectationParser
        parser = ExpectationParser(suite=LayoutTestSuite())
        results = parser.parse('test.txt', 'fast/css/test.html [ Text ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(LayoutTestStatus.TEXT, exp.expected)

    def test_parse_multiple_layout_expectations(self):
        from webkitexpectationspy.parser import ExpectationParser
        parser = ExpectationParser(suite=LayoutTestSuite())
        results = parser.parse('test.txt', 'fast/css/test.html [ Pass Fail ImageOnlyFailure ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(PASS, exp.expected)
        self.assertIn(FAIL, exp.expected)
        self.assertIn(LayoutTestStatus.IMAGE, exp.expected)

    def test_parse_leak_expectation(self):
        from webkitexpectationspy.parser import ExpectationParser
        parser = ExpectationParser(suite=LayoutTestSuite())
        results = parser.parse('test.txt', 'fast/dom/test.html [ Leak ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertIn(LayoutTestStatus.LEAK, exp.expected)

    def test_manager_with_layout_suite(self):
        from webkitexpectationspy.manager import ExpectationsManager
        manager = ExpectationsManager(suite=LayoutTestSuite())
        manager.load_content('TestExpectations', '''fast/css/border-radius.html [ ImageOnlyFailure ]
fast/dom/ [ Skip ]
fast/css/test.html [ Pass Fail ]''')

        exp = manager.get_expectation('fast/css/border-radius.html')
        self.assertIsNotNone(exp)
        self.assertIn(LayoutTestStatus.IMAGE, exp.expected)

        exp = manager.get_expectation('fast/dom/test.html')
        self.assertIsNotNone(exp)
        self.assertTrue(exp.skip)

        exp = manager.get_expectation('fast/css/test.html')
        self.assertTrue(exp.flaky)

    def test_parse_skip_with_expected_result(self):
        from webkitexpectationspy.parser import ExpectationParser
        parser = ExpectationParser(suite=LayoutTestSuite())
        results = parser.parse('test.txt', 'fast/css/test.html [ Skip ImageOnlyFailure ]')
        exp, warnings = results[0]
        self.assertEqual(len(warnings), 0)
        self.assertTrue(exp.skip)
        self.assertIn(LayoutTestStatus.IMAGE, exp.expected)


if __name__ == '__main__':
    unittest.main()
