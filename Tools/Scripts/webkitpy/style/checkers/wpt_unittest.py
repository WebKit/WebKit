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

import logging
import os
import unittest
from unittest.mock import patch, MagicMock

from webkitpy.style.checkers.wpt import filter_wpt_paths, run_wpt_lint

from webkitcorepy import OutputCapture


class FilterWPTPathsTest(unittest.TestCase):

    def test_no_wpt_files(self):
        paths = ['Source/WebCore/foo.cpp', 'Tools/Scripts/bar.py']
        self.assertEqual(filter_wpt_paths(paths), [])

    def test_wpt_files_only(self):
        prefix = os.path.join('LayoutTests', 'imported', 'w3c', 'web-platform-tests') + os.sep
        paths = [prefix + 'css/css-grid/test.html', prefix + 'html/dom/test.html']
        self.assertEqual(filter_wpt_paths(paths), ['css/css-grid/test.html', 'html/dom/test.html'])

    def test_mixed_files(self):
        prefix = os.path.join('LayoutTests', 'imported', 'w3c', 'web-platform-tests') + os.sep
        paths = [
            'Source/WebCore/foo.cpp',
            prefix + 'css/css-grid/test.html',
            'LayoutTests/fast/dom/test.html',
        ]
        self.assertEqual(filter_wpt_paths(paths), ['css/css-grid/test.html'])

    def test_http_wpt_files_not_included(self):
        paths = [os.path.join('LayoutTests', 'http', 'wpt', 'test.html')]
        self.assertEqual(filter_wpt_paths(paths), [])


class RunWPTLintTest(unittest.TestCase):

    def test_no_wpt_files_returns_zero(self):
        result = run_wpt_lint('/checkout', ['Source/WebCore/foo.cpp'])
        self.assertEqual(result, 0)

    @patch('webkitpy.style.checkers.wpt.WPTLinter')
    @patch('os.path.isdir', return_value=True)
    def test_wpt_files_invokes_linter(self, mock_isdir, MockLinter):
        mock_linter_instance = MagicMock()
        mock_linter_instance.lint.return_value = (0, '')
        MockLinter.return_value = mock_linter_instance

        prefix = os.path.join('LayoutTests', 'imported', 'w3c', 'web-platform-tests') + os.sep
        changed_files = [prefix + 'css/css-grid/test.html']

        with OutputCapture(level=logging.INFO):
            result = run_wpt_lint('/checkout', changed_files)

        self.assertEqual(result, 0)
        mock_linter_instance.lint.assert_called_once_with(['css/css-grid/test.html'])

    @patch('webkitpy.style.checkers.wpt.WPTLinter')
    @patch('os.path.isdir', return_value=True)
    def test_wpt_lint_errors_returned(self, mock_isdir, MockLinter):
        mock_linter_instance = MagicMock()
        mock_linter_instance.lint.return_value = (3, 'ERROR: some lint error\n')
        MockLinter.return_value = mock_linter_instance

        prefix = os.path.join('LayoutTests', 'imported', 'w3c', 'web-platform-tests') + os.sep
        changed_files = [prefix + 'css/css-grid/test.html']

        with OutputCapture(level=logging.INFO):
            result = run_wpt_lint('/checkout', changed_files)

        self.assertEqual(result, 3)

    @patch('os.path.isdir', return_value=False)
    def test_missing_wpt_dir_returns_zero(self, mock_isdir):
        prefix = os.path.join('LayoutTests', 'imported', 'w3c', 'web-platform-tests') + os.sep
        changed_files = [prefix + 'css/css-grid/test.html']

        with OutputCapture(level=logging.DEBUG):
            result = run_wpt_lint('/checkout', changed_files)

        self.assertEqual(result, 0)
