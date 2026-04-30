# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
# See LICENSE file for license terms.

"""Tests for expectations linter with 3-section sorting."""

import unittest

from webkitexpectationspy.linter import ExpectationsLinter


class ExpectationsLinterTest(unittest.TestCase):

    def test_lint_category_ordering(self):
        content = '[ arm64 mac ] TestWebKitAPI.WTF.Test [ Fail ]'  # Wrong order
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('out of order' in w.message for w in warnings))

    def test_lint_correct_category_ordering(self):
        content = '[ mac arm64 ] TestWebKitAPI.WTF.Test [ Fail ]'  # Correct order
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertFalse(any('out of order' in w.message for w in warnings))

    def test_lint_alphabetical_ordering_within_category(self):
        content = '[ release debug ] TestWebKitAPI.WTF.Test [ Fail ]'  # Wrong order
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('alphabetically ordered' in w.message for w in warnings))

    def test_lint_combination_collapse(self):
        content = '''[ debug ] TestWebKitAPI.WTF.Test [ Skip ]
[ release ] TestWebKitAPI.WTF.Test [ Skip ]
[ production ] TestWebKitAPI.WTF.Test [ Skip ]
[ asan ] TestWebKitAPI.WTF.Test [ Skip ]
[ guardmalloc ] TestWebKitAPI.WTF.Test [ Skip ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('collapsed' in w.message for w in warnings))

    def test_lint_universal_skip(self):
        content = 'TestWebKitAPI.WTF.Test [ Skip ]'
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('skipped on all configurations' in w.message for w in warnings))

    def test_apply_fixes_reorder(self):
        content = '[ arm64 mac ] TestWebKitAPI.WTF.Test [ Fail ]'
        linter = ExpectationsLinter(content, 'test.txt')
        linter.lint()
        fixed = linter.apply_fixes()
        self.assertIn('[ mac arm64 ]', fixed)

    def test_no_fixes_for_valid_file(self):
        content = '[ mac arm64 ] TestWebKitAPI.WTF.Test [ Fail ]'
        linter = ExpectationsLinter(content, 'test.txt')
        linter.lint()
        fixes = linter.get_fixes()
        self.assertEqual(len(fixes), 0)

    def test_all_skip_with_mixed_entries(self):
        content = '''[ debug ] TestWebKitAPI.WTF.Test [ Skip ]
[ release ] TestWebKitAPI.WTF.Test [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertFalse(any('skipped on all configurations' in w.message for w in warnings))


class ThreeSectionSortingTest(unittest.TestCase):
    """Tests for 3-section sorting: unannotated -> commented -> bug-tracked."""

    def test_section_classification_unannotated(self):
        content = 'TestA [ Fail ]'
        linter = ExpectationsLinter(content, 'test.txt')
        linter.lint()
        # Single entry, no ordering issue
        ordering_warnings = [w for w in linter._warnings if 'section' in w.message or 'alphabetical' in w.message]
        self.assertEqual(len(ordering_warnings), 0)

    def test_section_classification_bug_tracked(self):
        content = '''rdar://12345 TestB [ Fail ]
TestA [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        # TestA (unannotated, section 0) should come before rdar://12345 TestB (section 2)
        self.assertTrue(any('section' in w.message or 'unannotated' in w.message for w in warnings))

    def test_correct_three_section_order(self):
        content = '''TestA [ Fail ]
TestB [ Pass ] # known issue
webkit.org/b/123 TestC [ Crash ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        ordering_warnings = [w for w in warnings if 'section' in w.message or 'alphabetical' in w.message or 'order' in w.message.lower()]
        # Filter out non-ordering warnings (like "skipped on all" etc.)
        section_warnings = [w for w in ordering_warnings if 'Configuration' not in w.message]
        self.assertEqual(len(section_warnings), 0)

    def test_bug_with_comment_goes_to_section_2(self):
        content = '''rdar://12345 TestB [ Fail ] # some note
TestA [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        # Line with bug + comment should be section 2, TestA section 0 should come first
        self.assertTrue(any('section' in w.message or 'unannotated' in w.message for w in warnings))

    def test_wildcard_before_exact_within_section(self):
        content = '''TestExact [ Fail ]
TestWild.* [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('Wildcard' in w.message or 'wildcard' in w.message.lower() for w in warnings))

    def test_correct_wildcard_ordering(self):
        content = '''TestWild.* [ Fail ]
TestExact [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        wildcard_warnings = [w for w in warnings if 'wildcard' in w.message.lower()]
        self.assertEqual(len(wildcard_warnings), 0)

    def test_alphabetical_within_section(self):
        content = '''TestZ [ Fail ]
TestA [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('alphabetical' in w.message for w in warnings))

    def test_generate_sorted_content(self):
        content = '''rdar://12345 TestC [ Crash ]
TestB [ Pass ] # known issue
TestA [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        linter.lint()  # Must lint first to parse entries
        sorted_content = linter.generate_sorted_content()
        lines = [line for line in sorted_content.split('\n') if line.strip()]
        # Section 0 (unannotated) first: TestA
        self.assertIn('TestA', lines[0])
        # Section 1 (commented): TestB
        self.assertIn('TestB', lines[1])
        # Section 2 (bug-tracked): TestC
        self.assertIn('TestC', lines[2])

    def test_generate_sorted_wildcard_first(self):
        content = '''TestExact [ Fail ]
TestWild.* [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        linter.lint()
        sorted_content = linter.generate_sorted_content()
        lines = [line for line in sorted_content.split('\n') if line.strip()]
        self.assertIn('TestWild.*', lines[0])
        self.assertIn('TestExact', lines[1])

    def test_generate_sorted_preserves_header_comments(self):
        content = '''# Header for B
rdar://12345 TestB [ Crash ]
# Header for A
TestA [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        linter.lint()
        sorted_content = linter.generate_sorted_content()
        lines = sorted_content.split('\n')
        # TestA (section 0) should come first, with its header
        non_empty = [line for line in lines if line.strip()]
        self.assertIn('Header for A', non_empty[0])
        self.assertIn('TestA', non_empty[1])
        self.assertIn('Header for B', non_empty[2])
        self.assertIn('TestB', non_empty[3])

    def test_webkit_bug_is_section_2(self):
        content = '''webkit.org/b/999 TestB [ Fail ]
TestA [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('section' in w.message or 'unannotated' in w.message for w in warnings))

    def test_bug_function_is_section_2(self):
        content = '''Bug(user) TestB [ Fail ]
TestA [ Fail ]'''
        linter = ExpectationsLinter(content, 'test.txt')
        warnings = linter.lint()
        self.assertTrue(any('section' in w.message or 'unannotated' in w.message for w in warnings))


if __name__ == '__main__':
    unittest.main()
