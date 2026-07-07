# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import os
import unittest
from unittest.mock import MagicMock, patch

from webkitpy.style.checkers.api_test_expectations import APITestExpectationsChecker

_MODULE = 'webkitpy.style.checkers.api_test_expectations'


class _Warning(object):
    def __init__(self, filename, line_number, message=None, error=None, fixable=False):
        self.filename = filename
        self.line_number = line_number
        if message is not None:
            self.message = message
        if error is not None:
            self.error = error
        self.fixable = fixable


class _FileSystem(object):
    def join(self, *parts):
        return os.path.join(*parts)


class _Host(object):
    def __init__(self):
        self.filesystem = _FileSystem()
        self.port_factory = MagicMock()


class _CapturingHandler(object):
    reported = []

    def __init__(self, *args, **kwargs):
        pass

    def __call__(self, line_number, category, confidence, message):
        _CapturingHandler.reported.append((line_number, category, message))


class ShouldLogWarningTest(unittest.TestCase):
    def setUp(self):
        self.host = _Host()
        self.cwd = '/checkout'
        self.filename = '/checkout/TestExpectations/apitests'

    def test_warning_on_patched_line_is_logged(self):
        warning = _Warning(self.filename, 5, message='out of order')
        files = {self.filename: {5, 6}}
        self.assertTrue(APITestExpectationsChecker._should_log_warning(warning, files, self.cwd, self.host))

    def test_warning_on_unpatched_line_is_skipped(self):
        warning = _Warning(self.filename, 9, message='out of order')
        files = {self.filename: {5, 6}}
        self.assertFalse(APITestExpectationsChecker._should_log_warning(warning, files, self.cwd, self.host))

    def test_warning_for_untouched_file_is_skipped(self):
        warning = _Warning('/checkout/TestExpectations/other', 5, message='x')
        files = {self.filename: {5}}
        self.assertFalse(APITestExpectationsChecker._should_log_warning(warning, files, self.cwd, self.host))

    def test_whole_file_in_patch_is_logged(self):
        warning = _Warning(self.filename, 5, message='x')
        files = {self.filename: None}
        self.assertTrue(APITestExpectationsChecker._should_log_warning(warning, files, self.cwd, self.host))


class LintTestExpectationsTest(unittest.TestCase):
    def setUp(self):
        self.filename = '/checkout/TestExpectations/apitests'
        _CapturingHandler.reported = []

    def _run(self, warnings, files):
        fake_expectations = MagicMock()
        fake_expectations.lint.return_value = warnings
        with patch('{}.APITestExpectations'.format(_MODULE), return_value=fake_expectations), \
                patch('{}.DefaultStyleErrorHandler'.format(_MODULE), _CapturingHandler), \
                patch('{}._log'.format(_MODULE)) as mock_log:
            error_count = APITestExpectationsChecker.lint_test_expectations(
                files, configuration=None, cwd='/checkout', increment_error_count=lambda: 0, host=_Host())
        return error_count, mock_log

    def test_reports_only_patched_lines_and_hints_fix(self):
        warnings = [
            _Warning(self.filename, 5, message='entries are not in alphabetical order', fixable=True),
            _Warning(self.filename, 99, message='pre-existing issue', fixable=True),
        ]
        error_count, mock_log = self._run(warnings, {self.filename: {5}})

        self.assertEqual(error_count, 1)
        self.assertEqual(_CapturingHandler.reported, [(5, 'apitest/expectations', 'entries are not in alphabetical order')])
        mock_log.info.assert_called_once()
        self.assertIn('--fix-expectations', mock_log.info.call_args[0][0])

    def test_no_fix_hint_when_only_unfixable_warnings(self):
        # Uses .error (the ParseWarning shape) rather than .message to exercise that fallback.
        warnings = [_Warning(self.filename, 5, error='invalid expectation line', fixable=False)]
        error_count, mock_log = self._run(warnings, {self.filename: {5}})

        self.assertEqual(error_count, 1)
        self.assertEqual(_CapturingHandler.reported, [(5, 'apitest/expectations', 'invalid expectation line')])
        mock_log.info.assert_not_called()


if __name__ == '__main__':
    unittest.main()
