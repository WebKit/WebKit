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

import unittest

from .utils import getWorkerPassword


class TestGetWorkerPassword(unittest.TestCase):
    def test_password_from_passwords_json(self):
        self.assertEqual(getWorkerPassword('bot1', {'bot1': 'secret1'}, is_test_mode_enabled=False), 'secret1')

    def test_missing_password(self):
        password = getWorkerPassword('bot1', {'bot2': 'secret2'}, is_test_mode_enabled=False)
        self.assertNotEqual(password, 'password')
        self.assertGreaterEqual(len(password), 32)

    def test_empty_password(self):
        password = getWorkerPassword('bot1', {'bot1': ''}, is_test_mode_enabled=False)
        self.assertNotIn(password, ['', 'password'])

    def test_missing_passwords_are_unique(self):
        self.assertNotEqual(
            getWorkerPassword('bot1', {}, is_test_mode_enabled=False),
            getWorkerPassword('bot2', {}, is_test_mode_enabled=False),
        )

    def test_regex(self):
        passwords = {'ews3...': 'regex'}
        self.assertEqual(getWorkerPassword('ews3000', passwords, is_test_mode_enabled=False), 'regex')
        self.assertEqual(getWorkerPassword('ews3001', passwords, is_test_mode_enabled=False), 'regex')

    def test_regex_full_match(self):
        passwords = {'ews3...': 'regex'}
        self.assertNotEqual(getWorkerPassword('ews4000', passwords, is_test_mode_enabled=False), 'regex')
        self.assertNotEqual(getWorkerPassword('ews300', passwords, is_test_mode_enabled=False), 'regex')
        self.assertNotEqual(getWorkerPassword('ews30000', passwords, is_test_mode_enabled=False), 'regex')

    def test_regex_exact_match_preferred(self):
        passwords = {'ews3...': 'regex', 'ews3000': 'exact'}
        self.assertEqual(getWorkerPassword('ews3000', passwords, is_test_mode_enabled=False), 'exact')
        self.assertEqual(getWorkerPassword('ews3001', passwords, is_test_mode_enabled=False), 'regex')

    def test_regex_multiple_matches_raises(self):
        passwords = {r'ews3\d{3}': 'specific', 'ews.*': 'broad'}
        with self.assertRaises(ValueError):
            getWorkerPassword('ews3000', passwords, is_test_mode_enabled=False)
        self.assertEqual(getWorkerPassword('ews4000', passwords, is_test_mode_enabled=False), 'broad')

    def test_invalid_regex_ignored(self):
        passwords = {'ews3[': 'invalid', 'ews3...': 'regex'}
        self.assertEqual(getWorkerPassword('ews3000', passwords, is_test_mode_enabled=False), 'regex')

    def test_regex_empty_password(self):
        password = getWorkerPassword('ews3000', {'ews3...': ''}, is_test_mode_enabled=False)
        self.assertNotIn(password, ['', 'password'])

    def test_test_mode(self):
        self.assertEqual(getWorkerPassword('bot1', {}, is_test_mode_enabled=True), 'password')
