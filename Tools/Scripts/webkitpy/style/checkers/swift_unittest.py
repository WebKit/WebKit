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


class SwiftPlatformConditionTest(unittest.TestCase):

    """Tests the _check_platform_conditions() method of SwiftChecker."""

    def _errors_for_lines(self, lines):
        errors = []

        def _mock_handle_style_error(line_number, category, confidence, message):
            errors.append((line_number, category, confidence, message))

        checker = SwiftChecker("foo.swift", _mock_handle_style_error)
        checker._check_platform_conditions(lines)
        return errors

    def _expected_error(self, line_number, replacement, condition):
        return (line_number, 'webkit/wtf_platform', 5,
                "Use '%s' instead of '%s' in conditional compilation, so that Swift and C++ platform checks agree." % (replacement, condition))

    def test_simple_conditions(self):
        """Each Swift platform condition maps to its WTF_PLATFORM_* equivalent."""
        lines = [
            '#if os(macOS)',
            '#if os(iOS)',
            '#if os(watchOS)',
            '#if os(tvOS)',
            '#if os(visionOS)',
            '#if targetEnvironment(macCatalyst)',
            '#if canImport(UIKit)',
        ]

        expected_errors = [
            self._expected_error(1, 'WTF_PLATFORM_MAC', 'os(macOS)'),
            self._expected_error(2, 'WTF_PLATFORM_IOS_FAMILY', 'os(iOS)'),
            self._expected_error(3, 'WTF_PLATFORM_WATCHOS', 'os(watchOS)'),
            self._expected_error(4, 'WTF_PLATFORM_APPLETV', 'os(tvOS)'),
            self._expected_error(5, 'WTF_PLATFORM_VISION', 'os(visionOS)'),
            self._expected_error(6, 'WTF_PLATFORM_MACCATALYST', 'targetEnvironment(macCatalyst)'),
            self._expected_error(7, 'WTF_PLATFORM_IOS_FAMILY', 'canImport(UIKit)'),
        ]

        self.assertEqual(self._errors_for_lines(lines), expected_errors)

    def test_negated_condition(self):
        """A negated condition is still reported."""
        self.assertEqual(self._errors_for_lines(['#if !os(visionOS)']),
                         [self._expected_error(1, 'WTF_PLATFORM_VISION', 'os(visionOS)')])

    def test_indented_condition(self):
        """A nested, indented directive is still reported."""
        self.assertEqual(self._errors_for_lines(['        #if os(macOS)']),
                         [self._expected_error(1, 'WTF_PLATFORM_MAC', 'os(macOS)')])

    def test_elseif_condition(self):
        """#elseif is checked in addition to #if."""
        self.assertEqual(self._errors_for_lines(['#elseif os(macOS)']),
                         [self._expected_error(1, 'WTF_PLATFORM_MAC', 'os(macOS)')])

    def test_compound_conditions(self):
        """Every platform condition on a line is reported."""
        lines = [
            '#if os(macOS) && !targetEnvironment(macCatalyst)',
            '#if ENABLE_SWIFTUI && !os(watchOS) && !os(tvOS)',
            '#if USE_APPLE_INTERNAL_SDK || (!os(tvOS) && !os(watchOS))',
            '#if arch(x86_64) && (os(macOS) || targetEnvironment(macCatalyst))',
            '#if canImport(AppKit) && !targetEnvironment(macCatalyst)',
            '#if ENABLE_WRITING_TOOLS && canImport(UIKit)',
        ]

        expected_errors = [
            self._expected_error(1, 'WTF_PLATFORM_MAC', 'os(macOS)'),
            self._expected_error(1, 'WTF_PLATFORM_MACCATALYST', 'targetEnvironment(macCatalyst)'),
            self._expected_error(2, 'WTF_PLATFORM_WATCHOS', 'os(watchOS)'),
            self._expected_error(2, 'WTF_PLATFORM_APPLETV', 'os(tvOS)'),
            self._expected_error(3, 'WTF_PLATFORM_APPLETV', 'os(tvOS)'),
            self._expected_error(3, 'WTF_PLATFORM_WATCHOS', 'os(watchOS)'),
            self._expected_error(4, 'WTF_PLATFORM_MAC', 'os(macOS)'),
            self._expected_error(4, 'WTF_PLATFORM_MACCATALYST', 'targetEnvironment(macCatalyst)'),
            self._expected_error(5, 'WTF_PLATFORM_MACCATALYST', 'targetEnvironment(macCatalyst)'),
            self._expected_error(5, 'WTF_PLATFORM_MAC', 'canImport(AppKit)'),
            self._expected_error(6, 'WTF_PLATFORM_IOS_FAMILY', 'canImport(UIKit)'),
        ]

        self.assertEqual(sorted(self._errors_for_lines(lines)), sorted(expected_errors))

    def test_repeated_condition(self):
        """A condition repeated on one line is reported once per occurrence."""
        lines = ['#if os(macOS) || os(iOS) || os(watchOS) || os(tvOS) || os(visionOS)']

        expected_errors = [
            self._expected_error(1, 'WTF_PLATFORM_MAC', 'os(macOS)'),
            self._expected_error(1, 'WTF_PLATFORM_IOS_FAMILY', 'os(iOS)'),
            self._expected_error(1, 'WTF_PLATFORM_WATCHOS', 'os(watchOS)'),
            self._expected_error(1, 'WTF_PLATFORM_APPLETV', 'os(tvOS)'),
            self._expected_error(1, 'WTF_PLATFORM_VISION', 'os(visionOS)'),
        ]

        self.assertEqual(sorted(self._errors_for_lines(lines)), sorted(expected_errors))

    def test_version_gated_can_import_is_allowed(self):
        """canImport() with a version or a non-platform module is not a platform check."""
        lines = [
            '#if canImport(RealityKit, _version: 377)',
            '#if canImport(RealityKit, _version: "403.0.3")',
            '#if canImport(AppKit, _version: "2759")',
            '#if canImport(SwiftUI, _version: "7.0.57")',
            '#if canImport(UIIntelligenceSupport, _version: 9007)',
            '#if canImport(UIIntelligenceSupport.Radar165004762)',
            '#if canImport(CoreRE)',
            '#if canImport(XPC)',
            '#if canImport(_WebKit_SwiftUI)',
            '#if canImport(WebKit_Internal)',
            '#if canImport(_USDKit_RealityKit)',
        ]

        self.assertEqual(self._errors_for_lines(lines), [])

    def test_other_conditions_are_allowed(self):
        """Conditions which are not platform checks are left alone."""
        lines = [
            '#if arch(x86_64)',
            '#if arch(arm64)',
            '#if compiler(>=6.2.3)',
            '#if swift(>=6.0)',
            '#if targetEnvironment(simulator)',
            '#if os(Windows)',
            '#if os(Linux)',
            '#if DEBUG',
        ]

        self.assertEqual(self._errors_for_lines(lines), [])

    def test_wtf_platform_conditions_are_allowed(self):
        """The preferred spelling is not reported."""
        lines = [
            '#if WTF_PLATFORM_MAC',
            '#if !WTF_PLATFORM_MAC',
            '#if WTF_PLATFORM_MAC || HAVE_UIFINDINTERACTION',
            '#if WTF_PLATFORM_IOS || WTF_PLATFORM_MACCATALYST || WTF_PLATFORM_VISION',
            '#if canImport(SwiftUI, _version: "7.0.57") && (WTF_PLATFORM_MAC || HAVE_UIFINDINTERACTION)',
        ]

        self.assertEqual(self._errors_for_lines(lines), [])

    def test_non_directive_lines_are_allowed(self):
        """Only conditional compilation directives are checked."""
        lines = [
            '#endif // os(macOS)',
            '#endif // canImport(AppKit) && !targetEnvironment(macCatalyst)',
            '// os(macOS) is not the preferred spelling',
            '/* os(iOS) */',
            'let s = "os(macOS)"',
            '#else',
            '#if compiler(>=6.2.3)',
        ]

        self.assertEqual(self._errors_for_lines(lines), [])
