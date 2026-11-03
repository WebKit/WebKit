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

import unittest

from webkitpy.style.checkers.api_test_allowlist import APITestAllowlistChecker


class APITestAllowlistCheckerTest(unittest.TestCase):

    def check(self, content):
        errors = []
        checker = APITestAllowlistChecker('allowlist.txt', lambda *args: errors.append(args))
        checker.check(content.splitlines(), line_numbers=None)
        return errors

    def test_valid_allowlist(self):
        self.assertEqual([], self.check('''\
# Comment
TestIPC.ArgumentCoderTest/ArgumentCoderEncodingCounterTest
TestWTF.WTF_Lock.Basic  # Trailing comment
TestWebKitAPI.SwiftSuite/works()
[ ios ] TestIPC.ConnectionTest/ConnectionRunLoopTest.RunLoopOpen/ClientIsA
[ ios ] TestWebKitAPI.AnimatedResize
[ mac ] TestWebKitAPI.AnimatedResize.ResizeDoesNotHang
[ mac release ] TestWTF.WTF_Lock.ContendedShortSection
'''))

    def test_format(self):
        self.assertEqual([
            (1, 'api-test-allowlist/format', 5, 'Expecting "[", "#", or end of line instead of "TestIPC.Bar"'),
            (2, 'api-test-allowlist/format', 5, "Invalid entry format: '[ ios ] WTF_Lock.Basic' (expected BINARY.SUITE or BINARY.SUITE.TESTCASE)"),
            (3, 'api-test-allowlist/format', 5, "Wildcard entries match no tests: 'TestWTF.WTF_Lock*'"),
            (4, 'api-test-allowlist/format', 5, "Allowlist entries take no expectations or modifiers: 'TestWTF.WTF_Lock [ Skip ]'"),
        ], self.check('''\
TestIPC.Foo TestIPC.Bar
[ ios ] WTF_Lock.Basic
TestWTF.WTF_Lock*
TestWTF.WTF_Lock [ Skip ]
'''))

    def test_tab(self):
        self.assertEqual([
            (1, 'whitespace/tab', 5, 'Line contains tab character.'),
        ], self.check('[ ios ]\tTestWTF.WTF_Lock\n'))

    def test_duplicate(self):
        self.assertEqual([
            (3, 'api-test-allowlist/duplicate', 5, "Duplicate entry: '[ ios ] TestWTF.WTF_Lock.Basic'"),
        ], self.check('''\
TestIPC.Suite
[ ios ] TestWTF.WTF_Lock.Basic
[ ios ] TestWTF.WTF_Lock.Basic
'''))

    def test_redundant(self):
        self.assertEqual([
            (2, 'api-test-allowlist/redundant', 5, "Redundant entry: 'TestWTF.WTF_Lock.Basic' ('TestWTF.WTF_Lock' already covers it)"),
            (4, 'api-test-allowlist/redundant', 5, "Redundant entry: '[ ios ] TestWTF.WTF_Lock' ('TestWTF.WTF_Lock' already covers it)"),
            (5, 'api-test-allowlist/redundant', 5, "Redundant entry: '[ ios ] TestWTF.WTF_Lock.Basic' ('TestWTF.WTF_Lock.Basic' already covers it)"),
            (7, 'api-test-allowlist/redundant', 5, "Redundant entry: '[ mac ] TestIPC.Suite.test' ('[ mac ] TestIPC.Suite' already covers it)"),
        ], self.check('''\
TestWTF.WTF_Lock
TestWTF.WTF_Lock.Basic
[ ios ] TestIPC.Suite
[ ios ] TestWTF.WTF_Lock
[ ios ] TestWTF.WTF_Lock.Basic
[ mac ] TestIPC.Suite
[ mac ] TestIPC.Suite.test
'''))

    def test_redundant_before_covering_entry(self):
        self.assertEqual([
            (1, 'api-test-allowlist/redundant', 5, "Redundant entry: '[ ios ] TestWTF.WTF_Lock.Basic' ('TestWTF.WTF_Lock' already covers it)"),
        ], self.check('''\
[ ios ] TestWTF.WTF_Lock.Basic
TestWTF.WTF_Lock
'''))

    def test_order_within_each_gate(self):
        self.assertEqual([
            (2, 'api-test-allowlist/order', 5, "Entry not in alphabetical order: 'TestIPC.Suite' should come before 'TestWTF.WTF_Lock'"),
            (5, 'api-test-allowlist/order', 5, "Entry not in alphabetical order: 'TestIPC.Third' should come before 'TestWTF.Other'"),
        ], self.check('''\
TestWTF.WTF_Lock
TestIPC.Suite
[ ios ] TestIPC.Other
[ ios ] TestWTF.Other
[ ios ] TestIPC.Third
[ mac ] TestIPC.Other
'''))
