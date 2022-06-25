# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2013 University of Washington. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit test for js.py."""

import unittest

from webkitpy.style.checkers.js import JSChecker


class JSTestCase(unittest.TestCase):
    """TestCase for js.py"""

    def assertNoError(self, lines):
        """Asserts that the specified lines has no errors."""
        self.had_error = False

        def error_for_test(line_number, category, confidence, message):
            """Records if an error occurs."""
            self.had_error = True

        checker = JSChecker('', error_for_test)
        checker.check(lines)
        self.assertFalse(self.had_error, '%s should not have any errors.' % lines)

    def assertError(self, lines, expected_line_number):
        """Asserts that the specified lines has an error."""
        self.had_error = False

        def error_for_test(line_number, category, confidence, message):
            """Checks if the expected error occurs."""
            self.assertEqual(expected_line_number, line_number)
            self.assertTrue(category in ['whitespace/tab', 'js/syntax'])
            self.had_error = True

        checker = JSChecker('', error_for_test)
        checker.check(lines)
        self.assertTrue(self.had_error, '%s should have an error [whitespace/tab] or [js/syntax].' % lines)

    def test_no_error(self):
        """Tests for no error cases."""
        self.assertNoError([''])
        self.assertNoError(['abc def', 'ggg'])
        # Single-quotes within string are OK.
        self.assertNoError(['var foo = "''"'])
        # Single-quotes within regular expression are OK.
        self.assertNoError(["var regex = /[a-z']/"])

    def test_error(self):
        """Tests for error cases."""
        self.assertError(['\tvar foo = window;\n'], 1)
        # Single-quotes are not OK.
        self.assertError(["var foo = 'Hello world!;'\n"], 1)
        self.assertError(["foo(\"a'b\", 'c');"], 1)
        self.assertError(["foo(\"a/'b\", '/c');"], 1)
        self.assertError(["foo(/'/, 'c');"], 1)
