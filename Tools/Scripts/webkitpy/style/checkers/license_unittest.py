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

"""Unit tests for license.py."""

import unittest

from webkitpy.style.checkers.license import LicenseChecker


GOOGLE_LICENSE_FILE = [
    '/*',
    ' * Copyright (C) 2026 Apple Inc. All rights reserved.',
    ' *',
    ' * Redistribution and use in source and binary forms, with or without',
    ' * modification, are permitted provided that the following conditions are',
    ' * met:',
    ' *',
    ' *     * Redistributions of source code must retain the above copyright',
    ' * notice, this list of conditions and the following disclaimer.',
    ' *     * Neither the name of Google Inc. nor the names of its',
    ' * contributors may be used to endorse or promote products derived from',
    ' * this software without specific prior written permission.',
    ' */',
    '',
    '#pragma once',
    '',
]

APPLE_LICENSE_FILE = [
    '/*',
    ' * Copyright (C) 2026 Apple Inc. All rights reserved.',
    ' *',
    ' * Redistribution and use in source and binary forms, with or without',
    ' * modification, are permitted provided that the following conditions',
    ' * are met:',
    ' * 1. Redistributions of source code must retain the above copyright',
    ' *    notice, this list of conditions and the following disclaimer.',
    ' */',
    '',
    '#pragma once',
    '',
]


class LicenseCheckerTest(unittest.TestCase):

    def _collect_errors(self, lines, line_numbers):
        errors = []

        def record(line_number, category, confidence, message):
            errors.append((line_number, category, confidence, message))

        LicenseChecker(record).check(lines, line_numbers)
        return errors

    def test_flags_new_file_with_google_license(self):
        errors = self._collect_errors(GOOGLE_LICENSE_FILE, range(1, len(GOOGLE_LICENSE_FILE)))
        self.assertEqual(1, len(errors), 'expected exactly one error, got: {}'.format(errors))
        line_number, category, confidence, message = errors[0]
        self.assertEqual(10, line_number)
        self.assertEqual('legal/license', category)
        self.assertEqual(5, confidence)
        self.assertIn('copied it from the wrong place', message)
        self.assertIn('Source/WebCore/LICENSE-APPLE', message)

    def test_flags_new_file_without_final_newline(self):
        lines = GOOGLE_LICENSE_FILE[:-1]
        self.assertEqual(1, len(self._collect_errors(lines, range(1, len(lines) + 1))))

    def test_ignores_new_file_with_apple_license(self):
        self.assertEqual([], self._collect_errors(APPLE_LICENSE_FILE, range(1, len(APPLE_LICENSE_FILE))))

    def test_ignores_modified_file(self):
        self.assertEqual([], self._collect_errors(GOOGLE_LICENSE_FILE, range(2, len(GOOGLE_LICENSE_FILE))))

    def test_ignores_whole_file_check(self):
        self.assertEqual([], self._collect_errors(GOOGLE_LICENSE_FILE, None))
