# Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

import json
import os
import unittest
from unittest.mock import MagicMock, patch

from webkitpy.common.system.logtesting import LoggingTestCase
from webkitpy.style.checkers.eslint import WEB_INSPECTOR_UI_ESLINT_CONFIGURATION, ESLintChecker


CWD = '/mock-checkout'
JAVASCRIPT_FILE = os.path.join('Source', 'WebInspectorUI', 'UserInterface', 'Example.js')


def _absolute_path(relative_path):
    return os.path.join(CWD, relative_path)


class ESLintCheckerTest(LoggingTestCase):
    def setUp(self):
        super().setUp()
        self.configuration = MagicMock()
        self.configuration.is_reportable.return_value = True
        self.configuration.max_reports_per_category = {}
        self.increment_error_count = MagicMock()
        self.host = MagicMock()
        self.host.filesystem.exists.return_value = True

    def _check(self, files, output='[]'):
        self.host.executive.run_command.return_value = output
        with patch('webkitpy.style.checkers.eslint.shutil.which', return_value='/mock-bin/eslint'):
            ESLintChecker.check(files, self.configuration, CWD, self.increment_error_count, host=self.host)

    def test_unrelated_files_do_not_invoke_eslint(self):
        with patch('webkitpy.style.checkers.eslint.shutil.which') as which:
            ESLintChecker.check(
                {_absolute_path('Source/WebCore/Example.js'): [1]},
                self.configuration,
                CWD,
                self.increment_error_count,
                host=self.host,
            )

        which.assert_not_called()
        self.host.executive.run_command.assert_not_called()

    def test_deleted_files_do_not_invoke_eslint(self):
        self.host.filesystem.exists.return_value = False

        with patch('webkitpy.style.checkers.eslint.shutil.which') as which:
            ESLintChecker.check(
                {_absolute_path(JAVASCRIPT_FILE): None},
                self.configuration,
                CWD,
                self.increment_error_count,
                host=self.host,
            )

        which.assert_not_called()
        self.host.executive.run_command.assert_not_called()

    def test_missing_eslint_is_skipped(self):
        with patch('webkitpy.style.checkers.eslint.shutil.which', return_value=None):
            ESLintChecker.check(
                {_absolute_path(JAVASCRIPT_FILE): [1]},
                self.configuration,
                CWD,
                self.increment_error_count,
                host=self.host,
            )

        self.assertLog(['ESLint was not found in PATH; skipping WebInspectorUI lint.\n'])
        self.host.executive.run_command.assert_not_called()
        self.increment_error_count.assert_not_called()

    def test_changed_javascript_files_are_linted_together(self):
        second_file = os.path.join('Source', 'WebInspectorUI', 'UserInterface', 'Second.js')
        self._check({
            _absolute_path(second_file): [1],
            _absolute_path(JAVASCRIPT_FILE): [1],
        })

        self.host.executive.run_command.assert_called_once_with(
            [
                '/mock-bin/eslint',
                '--format=json',
                '--no-warn-ignored',
                os.path.join('UserInterface', 'Example.js'),
                os.path.join('UserInterface', 'Second.js'),
            ],
            cwd=_absolute_path(os.path.join('Source', 'WebInspectorUI')),
            decode_output=True,
            ignore_errors=True,
            return_stderr=False,
        )

    def test_only_errors_on_modified_lines_are_reported(self):
        output = json.dumps([{
            'filePath': _absolute_path(JAVASCRIPT_FILE),
            'messages': [
                {'line': 1, 'message': 'Existing error.', 'ruleId': 'no-undef', 'severity': 2},
                {'line': 2, 'message': 'New error.', 'ruleId': 'no-unused-vars', 'severity': 2},
                {'line': 2, 'message': 'New warning.', 'ruleId': 'example', 'severity': 1},
            ],
        }])
        self._check({_absolute_path(JAVASCRIPT_FILE): [2]}, output=output)

        self.configuration.write_style_error.assert_called_once()
        call = self.configuration.write_style_error.call_args
        self.assertEqual(call.kwargs['file_path'], JAVASCRIPT_FILE)
        self.assertEqual(call.kwargs['line_number'], 2)
        self.assertEqual(call.kwargs['category'], 'js/eslint')
        self.assertEqual(call.kwargs['message'], '[no-unused-vars] New error.')
        self.increment_error_count.assert_called_once()

    def test_configuration_changes_lint_all_javascript(self):
        output = json.dumps([{
            'filePath': _absolute_path(JAVASCRIPT_FILE),
            'messages': [
                {'line': 1, 'message': 'Existing error.', 'ruleId': 'no-undef', 'severity': 2},
            ],
        }])
        self._check({_absolute_path(WEB_INSPECTOR_UI_ESLINT_CONFIGURATION): [1]}, output=output)

        command = self.host.executive.run_command.call_args.args[0]
        self.assertEqual(command[-1], '.')
        self.configuration.write_style_error.assert_called_once()
        self.increment_error_count.assert_called_once()

    def test_invalid_output_is_reported(self):
        self._check({_absolute_path(JAVASCRIPT_FILE): [1]}, output='invalid')

        self.configuration.write_style_error.assert_called_once()
        call = self.configuration.write_style_error.call_args
        self.assertEqual(call.kwargs['file_path'], WEB_INSPECTOR_UI_ESLINT_CONFIGURATION)
        self.assertEqual(call.kwargs['line_number'], 0)
        self.assertEqual(call.kwargs['category'], 'js/eslint')
        self.increment_error_count.assert_called_once()


if __name__ == '__main__':
    unittest.main()
