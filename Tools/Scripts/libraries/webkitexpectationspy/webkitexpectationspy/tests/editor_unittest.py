# Copyright (C) 2026 Apple Inc. All rights reserved.
# See LICENSE file for license terms.

import unittest

from webkitexpectationspy.editor import remove_expectations, set_expectation_results
from webkitexpectationspy.parser import ExpectationParser
from webkitexpectationspy.suites.api_tests import APITestSuite
from webkitexpectationspy.suites.layout_tests import LayoutTestSuite


def parsed(content, suite=None):
    return {
        expectation.test_pattern: expectation
        for expectation, _ in ExpectationParser(suite or LayoutTestSuite()).parse('TestExpectations', content)
        if expectation
    }


class RemoveExpectationsTest(unittest.TestCase):

    def test_removes_one_entry_and_keeps_shared_header(self):
        content = '# Shared reason\nfast/a.html [ Failure ]\nfast/b.html [ Failure ]\n'
        self.assertEqual(
            remove_expectations(content, [parsed(content)['fast/a.html']]),
            '# Shared reason\nfast/b.html [ Failure ]\n',
        )

    def test_removes_header_when_every_entry_under_it_is_removed(self):
        content = 'fast/x.html [ Pass ]\n\n# Reason\nfast/a.html [ Failure ]\nfast/b.html [ Timeout ]\n\nfast/y.html [ Pass ]\n'
        expectations = parsed(content)
        self.assertEqual(
            remove_expectations(content, [expectations['fast/a.html'], expectations['fast/b.html']]),
            'fast/x.html [ Pass ]\n\nfast/y.html [ Pass ]\n',
        )

    def test_keeps_banner_header(self):
        content = 'fast/x.html [ Pass ]\n\n##########\n# Crashes #\n##########\nfast/a.html [ Crash ]\n\nfast/b.html [ Crash ]\n'
        self.assertEqual(
            remove_expectations(content, [parsed(content)['fast/a.html']]),
            'fast/x.html [ Pass ]\n\n##########\n# Crashes #\n##########\n\nfast/b.html [ Crash ]\n',
        )

    def test_keeps_header_starting_on_first_line(self):
        content = '# Copyright\nfast/a.html [ Failure ]\n\nfast/b.html [ Failure ]\n'
        self.assertEqual(
            remove_expectations(content, [parsed(content)['fast/a.html']]),
            '# Copyright\n\nfast/b.html [ Failure ]\n',
        )

    def test_removes_leading_blank_left_at_file_start(self):
        content = 'fast/a.html [ Failure ]\n\nfast/b.html [ Failure ]\n'
        self.assertEqual(remove_expectations(content, [parsed(content)['fast/a.html']]), 'fast/b.html [ Failure ]\n')

    def test_removes_trailing_blank_left_at_file_end(self):
        content = 'fast/a.html [ Failure ]\n\nfast/b.html [ Failure ]\n'
        self.assertEqual(remove_expectations(content, [parsed(content)['fast/b.html']]), 'fast/a.html [ Failure ]\n')

    def test_keeps_missing_trailing_newline(self):
        content = 'fast/a.html [ Failure ]\nfast/b.html [ Failure ]'
        self.assertEqual(remove_expectations(content, [parsed(content)['fast/a.html']]), 'fast/b.html [ Failure ]')

    def test_keeps_unrelated_blank_runs(self):
        content = 'fast/x.html [ Pass ]\n\n\nfast/a.html [ Failure ]\nfast/b.html [ Failure ]\n'
        self.assertEqual(
            remove_expectations(content, [parsed(content)['fast/a.html']]),
            'fast/x.html [ Pass ]\n\n\nfast/b.html [ Failure ]\n',
        )

    def test_removes_entry_with_bug_config_and_inline_comment(self):
        content = 'fast/x.html [ Pass ]\nwebkit.org/b/1 [ Debug Sequoia Tahoe ] fast/a.html [ Failure ] # flaky\n'
        self.assertEqual(remove_expectations(content, [parsed(content)['fast/a.html']]), 'fast/x.html [ Pass ]\n')

    def test_no_expectations_returns_content_unchanged(self):
        content = '# Reason\nfast/a.html [ Failure ]\n'
        self.assertEqual(remove_expectations(content, []), content)

    def test_rejects_line_that_no_longer_matches(self):
        expectation = parsed('fast/a.html [ Failure ]\n')['fast/a.html']
        with self.assertRaises(ValueError):
            remove_expectations('fast/b.html [ Failure ]\n', [expectation])

    def test_rejects_line_past_end_of_file(self):
        expectation = parsed('\n\nfast/a.html [ Failure ]\n')['fast/a.html']
        with self.assertRaises(ValueError):
            remove_expectations('fast/a.html [ Failure ]', [expectation])

    def test_rejects_match_inside_comment(self):
        expectation = parsed('fast/a.html [ Failure ]\n')['fast/a.html']
        with self.assertRaises(ValueError):
            remove_expectations('# fast/a.html [ Failure ]\n', [expectation])


class SetExpectationResultsTest(unittest.TestCase):

    def test_replaces_results_and_keeps_bug_config_and_comment(self):
        content = 'webkit.org/b/1 [ Debug arm64 ] fast/a.html [ Failure Timeout ] # reason\n'
        self.assertEqual(
            set_expectation_results(content, parsed(content)['fast/a.html'], ['Pass'], LayoutTestSuite()),
            'webkit.org/b/1 [ Debug arm64 ] fast/a.html [ Pass ] # reason\n',
        )

    def test_keeps_layout_modifiers(self):
        content = 'fast/a.html [ Failure Slow DumpJSConsoleLogInStdErr ]\n'
        self.assertEqual(
            set_expectation_results(content, parsed(content)['fast/a.html'], ['Pass'], LayoutTestSuite()),
            'fast/a.html [ Pass Slow DumpJSConsoleLogInStdErr ]\n',
        )

    def test_keeps_slow_timeout_modifier(self):
        content = 'TestWebKitAPI.WebKit.Load [ Fail Slow:120s ]\n'
        expectation = parsed(content, APITestSuite())['TestWebKitAPI.WebKit.Load']
        self.assertEqual(
            set_expectation_results(content, expectation, ['Pass'], APITestSuite()),
            'TestWebKitAPI.WebKit.Load [ Pass Slow:120s ]\n',
        )

    def test_without_suite_keeps_only_default_modifiers(self):
        content = 'fast/a.html [ Failure Slow DumpJSConsoleLogInStdErr ]\n'
        self.assertEqual(
            set_expectation_results(content, parsed(content)['fast/a.html'], ['Pass']),
            'fast/a.html [ Pass Slow ]\n',
        )

    def test_narrows_results(self):
        content = 'fast/a.html [ Pass Failure Timeout ]\n'
        self.assertEqual(
            set_expectation_results(content, parsed(content)['fast/a.html'], ['Pass', 'Failure'], LayoutTestSuite()),
            'fast/a.html [ Pass Failure ]\n',
        )

    def test_adds_results_bracket_when_missing(self):
        content = 'fast/a.html # reason\n'
        self.assertEqual(
            set_expectation_results(content, parsed(content)['fast/a.html'], ['Pass'], LayoutTestSuite()),
            'fast/a.html [ Pass ] # reason\n',
        )

    def test_touches_only_target_line(self):
        content = 'fast/a.html [ Failure ]\nfast/b.html [ Failure ]'
        self.assertEqual(
            set_expectation_results(content, parsed(content)['fast/b.html'], ['Pass'], LayoutTestSuite()),
            'fast/a.html [ Failure ]\nfast/b.html [ Pass ]',
        )

    def test_rejects_empty_results(self):
        content = 'fast/a.html [ Failure ]\n'
        with self.assertRaises(ValueError):
            set_expectation_results(content, parsed(content)['fast/a.html'], [], LayoutTestSuite())

    def test_rejects_line_that_no_longer_matches(self):
        expectation = parsed('fast/a.html [ Failure ]\n')['fast/a.html']
        with self.assertRaises(ValueError):
            set_expectation_results('fast/b.html [ Failure ]\n', expectation, ['Pass'], LayoutTestSuite())
