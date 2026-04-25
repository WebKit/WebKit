# Copyright (C) 2025 Apple Inc. All rights reserved.
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

"""Unit tests for swift.py."""

import os
import platform
import subprocess
import unittest

from webkitpy.style.checkers.swift import SwiftChecker


def _has_swift_format():
    try:
        proc = subprocess.run(
            ["/usr/bin/swift", "format", "--help"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except FileNotFoundError:
        return False
    else:
        return proc.returncode == 0


@unittest.skipIf(
    not _has_swift_format(),
    reason="requires swift-format",
)
class SwiftCheckerTest(unittest.TestCase):

    """Tests the SwiftChecker class."""

    def test_init(self):
        """Test __init__() method."""
        def _mock_handle_style_error(self):
            pass

        checker = SwiftChecker("foo.txt", _mock_handle_style_error)
        self.assertEqual(checker.file_path, "foo.txt")
        self.assertEqual(checker.handle_style_error, _mock_handle_style_error)

    def test_check(self):
        """Test check() method."""
        errors = []

        def _mock_handle_style_error(line_number, category, confidence, message):
            error = (line_number, category, confidence, message)
            errors.append(error)

        current_dir = os.path.dirname(__file__)
        file_path = os.path.join(current_dir, "swift_unittest_input.swift")

        checker = SwiftChecker(file_path, _mock_handle_style_error)
        checker.check(lines=[])

        expected_errors = [
            (3, "NeverForceUnwrap", 5, "do not force unwrap \'URL(string: \"https://www.apple.com\")\'"),
            (2, "Spacing", 5, "remove 1 space"),
            (3, "Indentation", 5, "replace leading whitespace with 4 spaces"),
        ]

        self.assertEqual(errors, expected_errors)

    def test_check_unsafe(self):
        """Test _check_unsafe() method."""
        errors = []

        def _mock_handle_style_error(line_number, category, confidence, message):
            errors.append((line_number, category, confidence, message))

        checker = SwiftChecker("foo.swift", _mock_handle_style_error)

        lines = [
            'let x = unsafe ptr.pointee',          # 1: flagged
            '// unsafe is fine in comments',         # 2: not flagged
            '@unsafe func foo() {}',                 # 3: not flagged (@unsafe is ok)
            'let s = "unsafe pointer"',              # 4: not flagged (string)
            '/* unsafe block comment */',             # 5: not flagged (block comment)
            'func safeFunc() {}',                    # 6: not flagged
            'let y = unsafe something',              # 7: flagged
        ]

        checker._check_unsafe(lines)

        expected_errors = [
            (1, 'webkit/unsafe', 5, "Please avoid new use of 'unsafe' in WebKit. See https://github.com/WebKit/WebKit/wiki/Safer-Swift-Guidelines."),
            (7, 'webkit/unsafe', 5, "Please avoid new use of 'unsafe' in WebKit. See https://github.com/WebKit/WebKit/wiki/Safer-Swift-Guidelines."),
        ]

        self.assertEqual(errors, expected_errors)

    def test_check_safe(self):
        """Test that @safe is flagged."""
        errors = []

        def _mock_handle_style_error(line_number, category, confidence, message):
            errors.append((line_number, category, confidence, message))

        checker = SwiftChecker("foo.swift", _mock_handle_style_error)

        lines = [
            '@safe func bar() {}',                   # 1: flagged
            '@unsafe func baz() {}',                  # 2: not flagged
            'func safeFunc() {}',                     # 3: not flagged
        ]

        checker._check_unsafe(lines)

        expected_errors = [
            (1, 'webkit/unsafe', 5, "Please avoid new use of '@safe' in WebKit. See https://github.com/WebKit/WebKit/wiki/Safer-Swift-Guidelines."),
        ]

        self.assertEqual(errors, expected_errors)
