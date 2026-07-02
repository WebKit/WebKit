# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import os
import tempfile
import textwrap
import unittest
from unittest.mock import MagicMock, patch

from webkitpy.style.checkers.wpt import (
    WPTLintRulesImportError,
    _load_wpt_lint_rules_module,
    _normalize_rule_name,
    run_wpt_lint,
    wpt_lint_categories,
)


class RunWPTLintTest(unittest.TestCase):
    @patch('webkitpy.style.checkers.wpt.WPTLinter')
    def test_no_errors(self, MockLinter):
        mock_instance = MagicMock()
        mock_instance.lint.return_value = iter([])
        MockLinter.return_value = mock_instance

        configuration = MagicMock()
        increment = MagicMock()

        run_wpt_lint('/wpt', ['css/test.html'], configuration, increment)

        mock_instance.lint.assert_called_once_with(['css/test.html'])
        configuration.write_style_error.assert_not_called()
        increment.assert_not_called()

    @patch('webkitpy.style.checkers.wpt.WPTLinter')
    def test_reports_each_error(self, MockLinter):
        mock_instance = MagicMock()
        mock_instance.lint.return_value = iter([
            {'path': 'css/a.html', 'lineno': 12, 'rule': 'TRAILING WHITESPACE', 'message': 'trailing'},
            {'path': 'css/b.html', 'lineno': None, 'rule': 'PARSE-FAILED', 'message': 'parse failed'},
        ])
        MockLinter.return_value = mock_instance

        configuration = MagicMock()
        configuration.is_reportable.return_value = True
        increment = MagicMock()

        run_wpt_lint('/wpt', ['css/a.html', 'css/b.html'], configuration, increment)

        self.assertEqual(configuration.write_style_error.call_count, 2)
        self.assertEqual(increment.call_count, 2)

        first_call = configuration.write_style_error.call_args_list[0]
        self.assertEqual(first_call.kwargs['category'], 'wpt/lint/trailing_whitespace')
        self.assertEqual(first_call.kwargs['confidence_in_error'], 5)
        self.assertEqual(first_call.kwargs['line_number'], 12)
        self.assertIn('TRAILING WHITESPACE', first_call.kwargs['message'])
        self.assertIn('trailing', first_call.kwargs['message'])
        self.assertTrue(first_call.kwargs['file_path'].endswith('css/a.html'))

        second_call = configuration.write_style_error.call_args_list[1]
        self.assertEqual(second_call.kwargs['category'], 'wpt/lint/parse_failed')
        self.assertEqual(second_call.kwargs['line_number'], '-')

    @patch('webkitpy.style.checkers.wpt.WPTLinter')
    def test_filtered_out_errors_are_not_counted(self, MockLinter):
        mock_instance = MagicMock()
        mock_instance.lint.return_value = iter([
            {'path': 'css/a.html', 'lineno': 1, 'rule': 'DUPLICATE-BASENAME-PATH', 'message': 'm'},
        ])
        MockLinter.return_value = mock_instance

        configuration = MagicMock()
        configuration.is_reportable.return_value = False
        increment = MagicMock()

        run_wpt_lint('/wpt', ['css/a.html'], configuration, increment)

        configuration.is_reportable.assert_called_once()
        self.assertEqual(
            configuration.is_reportable.call_args.kwargs['category'],
            'wpt/lint/duplicate_basename_path')
        configuration.write_style_error.assert_not_called()
        increment.assert_not_called()


class NormalizeRuleNameTest(unittest.TestCase):
    def test_spaces_and_hyphens_become_underscores(self):
        self.assertEqual(_normalize_rule_name('WORKER COLLISION'), 'worker_collision')
        self.assertEqual(_normalize_rule_name('DUPLICATE-BASENAME-PATH'), 'duplicate_basename_path')
        self.assertEqual(_normalize_rule_name('MISSING-LINK'), 'missing_link')
        self.assertEqual(_normalize_rule_name('MOJOM-JS'), 'mojom_js')
        self.assertEqual(_normalize_rule_name('GITIGNORE'), 'gitignore')


class WPTLintCategoriesTest(unittest.TestCase):
    def test_happy_path_contains_known_rules(self):
        categories = wpt_lint_categories()
        self.assertIn('wpt/lint', categories)
        self.assertIn('wpt/lint/duplicate_basename_path', categories)
        self.assertIn('wpt/lint/worker_collision', categories)

    @patch('webkitpy.style.checkers.wpt.os.path.exists', return_value=False)
    def test_rules_file_absent_returns_only_coarse_category(self, _mock_exists):
        self.assertEqual(wpt_lint_categories(), {'wpt/lint'})

    def test_broken_rules_file_raises(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            rules_dir = os.path.join(tmpdir, 'tools', 'lint')
            os.makedirs(rules_dir)
            with open(os.path.join(rules_dir, 'rules.py'), 'w') as f:
                f.write('this is not valid python (\n')

            # Patch IMPORTED_WPT_DIR to be relative from LayoutTests to tmpdir
            layout_tests_dir = 'LayoutTests'
            rel_from_layout_tests = os.path.relpath(tmpdir, layout_tests_dir)
            with patch('webkitpy.style.checkers.wpt.IMPORTED_WPT_DIR', rel_from_layout_tests):
                self.assertRaises(WPTLintRulesImportError, _load_wpt_lint_rules_module)


class LoadWPTLintRulesModuleTest(unittest.TestCase):
    def test_executes_stub_rules_module(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            rules_dir = os.path.join(tmpdir, 'tools', 'lint')
            os.makedirs(rules_dir)
            with open(os.path.join(rules_dir, 'rules.py'), 'w') as f:
                f.write(textwrap.dedent("""
                    class Rule:
                        pass

                    class A(Rule):
                        name = "FOO BAR"

                    class B(Rule):
                        name = "BAZ-QUX"
                """))

            # Patch IMPORTED_WPT_DIR to be relative from LayoutTests to tmpdir
            layout_tests_dir = 'LayoutTests'
            rel_from_layout_tests = os.path.relpath(tmpdir, layout_tests_dir)
            with patch('webkitpy.style.checkers.wpt.IMPORTED_WPT_DIR', rel_from_layout_tests):
                categories = wpt_lint_categories()

        self.assertIn('wpt/lint/foo_bar', categories)
        self.assertIn('wpt/lint/baz_qux', categories)
