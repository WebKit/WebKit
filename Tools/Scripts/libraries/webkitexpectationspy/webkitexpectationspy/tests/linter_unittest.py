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


class SortedContentFormattingTest(unittest.TestCase):
    """Formatting rules for generate_sorted_content: newline, blank lines, comment groups."""

    def _sorted(self, content):
        linter = ExpectationsLinter(content, 'test.txt')
        linter.lint()
        return linter.generate_sorted_content()

    def test_output_ends_with_newline(self):
        self.assertTrue(self._sorted('TestA [ Fail ]').endswith('\n'))

    def test_blank_line_between_sections(self):
        lines = self._sorted('webkit.org/b/1 TestZ [ Fail ]\nTestA [ Fail ]').split('\n')
        a_index = next(i for i, line in enumerate(lines) if 'TestA' in line)
        z_index = next(i for i, line in enumerate(lines) if 'TestZ' in line)
        self.assertLess(a_index, z_index)
        self.assertTrue(any(not lines[i].strip() for i in range(a_index + 1, z_index)))

    def test_group_comment_stays_with_its_entries(self):
        content = '''# webkit.org/b/1 [ iOS ] Foo.* are flaky
[ ios ] Foo.BbbTest [ Crash ]
[ ios ] Foo.AaaTest [ Crash ]'''
        non_empty = [line for line in self._sorted(content).split('\n') if line.strip()]
        self.assertEqual(non_empty[0], '# webkit.org/b/1 [ iOS ] Foo.* are flaky')
        self.assertIn('Foo.AaaTest', non_empty[1])
        self.assertIn('Foo.BbbTest', non_empty[2])

    def test_removes_redundant_bug_group_comment(self):
        content = '''# webkit.org/b/2 Grp.* are constant timeouts
webkit.org/b/2 [ mac ] Grp.Bbb [ Timeout ]
webkit.org/b/2 [ mac ] Grp.Aaa [ Timeout ]'''
        non_empty = [line for line in self._sorted(content).split('\n') if line.strip()]
        self.assertFalse(any(line.startswith('#') for line in non_empty))
        self.assertIn('Grp.Aaa', non_empty[0])
        self.assertIn('Grp.Bbb', non_empty[1])

    def test_keeps_group_comment_when_entries_lack_bug_id(self):
        content = '''# webkit.org/b/3 Grp.* are flaky
[ mac ] Grp.Aaa [ Crash ]'''
        non_empty = [line for line in self._sorted(content).split('\n') if line.strip()]
        self.assertEqual(non_empty[0], '# webkit.org/b/3 Grp.* are flaky')
        self.assertIn('Grp.Aaa', non_empty[1])

    def test_blank_line_around_comment_group(self):
        content = '''# note about group
[ mac ] Grp.One [ Skip ]

TestZebra [ Fail ]'''
        lines = self._sorted(content).split('\n')
        zebra_index = next(i for i, line in enumerate(lines) if 'TestZebra' in line)
        one_index = next(i for i, line in enumerate(lines) if 'Grp.One' in line)
        self.assertLess(zebra_index, one_index)
        self.assertTrue(any(not lines[i].strip() for i in range(zebra_index + 1, one_index)))

    def test_uncommented_entry_precedes_commented_group(self):
        content = '''# webkit.org/b/1 Grp.* are flaky
[ mac ] Grp.Aaa [ Crash ]

TestZebra [ Fail ]'''
        non_empty = [line for line in self._sorted(content).split('\n') if line.strip()]
        self.assertIn('TestZebra', non_empty[0])
        self.assertEqual(non_empty[1], '# webkit.org/b/1 Grp.* are flaky')
        self.assertIn('Grp.Aaa', non_empty[2])

    def test_prologue_blank_lines_normalized(self):
        content = '''# License header



TestA [ Fail ]'''
        lines = self._sorted(content).split('\n')
        self.assertEqual(lines[0], '# License header')
        self.assertEqual(lines[1], '')
        self.assertTrue(lines[2].startswith('TestA'))

    def test_generate_sorted_content_is_idempotent(self):
        content = '''# webkit.org/b/1 Grp.* are flaky
[ mac ] Grp.Bbb [ Crash ]
[ mac ] Grp.Aaa [ Crash ]

webkit.org/b/9 TestZzz [ Failure ]
rdar://5 TestAaa [ Skip ]'''
        once = self._sorted(content)
        relinter = ExpectationsLinter(once, 'test.txt')
        relinter.lint()
        self.assertEqual(once, relinter.generate_sorted_content())


if __name__ == '__main__':
    unittest.main()
