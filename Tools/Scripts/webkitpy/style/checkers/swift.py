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

"""Supports checking WebKit style in Swift files."""

import re
import subprocess


class SwiftChecker(object):
    categories = set(['webkit/unsafe', 'webkit/wtf_platform'])

    _PLATFORM_CONDITIONS = [
        (re.compile(r'\bos\(macOS\)'), 'os(macOS)', 'WTF_PLATFORM_MAC'),
        (re.compile(r'\bos\(iOS\)'), 'os(iOS)', 'WTF_PLATFORM_IOS_FAMILY'),
        (re.compile(r'\bos\(watchOS\)'), 'os(watchOS)', 'WTF_PLATFORM_WATCHOS'),
        (re.compile(r'\bos\(tvOS\)'), 'os(tvOS)', 'WTF_PLATFORM_APPLETV'),
        (re.compile(r'\bos\(visionOS\)'), 'os(visionOS)', 'WTF_PLATFORM_VISION'),
        (re.compile(r'\btargetEnvironment\(macCatalyst\)'), 'targetEnvironment(macCatalyst)', 'WTF_PLATFORM_MACCATALYST'),
        (re.compile(r'\bcanImport\(UIKit\)'), 'canImport(UIKit)', 'WTF_PLATFORM_IOS_FAMILY'),
        (re.compile(r'\bcanImport\(AppKit\)'), 'canImport(AppKit)', 'WTF_PLATFORM_MAC'),
    ]

    _CONDITIONAL_DIRECTIVE_RE = re.compile(r'^\s*#(if|elseif)\b')

    def __init__(self, file_path, handle_style_error):
        self.file_path = file_path
        self.handle_style_error = handle_style_error
        self.has_swift_format_errors = False

    def _swift_format(self, file_path, lines, error):
        lint_result = subprocess.run(['/usr/bin/swift', 'format', 'lint', '--strict', file_path], capture_output=True, text=True)

        # matches <filename>:<line>: error: [<category>] <message>
        pattern = re.compile(
            r'^(?P<filename>[^:]+):'
            r'(?P<line_number>\d+):'
            r'\d+:'
            r'\s*(?P<kind>warning|error):'
            r'\s*\[(?P<category>[^\]]+)\]\s*'
            r'(?P<message>.+)$'
        )

        for line in lint_result.stderr.splitlines():
            match = re.match(pattern, line)
            if not match:
                continue

            line_number = match.group("line_number")
            category = match.group("category")
            message = match.group("message")

            self.has_swift_format_errors = True
            self.handle_style_error(int(line_number), category, 5, message)

    def _check_unsafe(self, lines):
        in_block_comment = False
        for index, line in enumerate(lines):
            if in_block_comment:
                if '*/' in line:
                    in_block_comment = False
                continue

            stripped = line.lstrip()
            if stripped.startswith('//'):
                continue

            if '/*' in line:
                if '*/' not in line[line.index('/*') + 2:]:
                    in_block_comment = True
                continue

            if re.search(r'"[^"]*\bunsafe\b[^"]*"', line):
                continue
            elif re.search(r'@unsafe\b', line):
                continue
            elif re.search(r'\bunsafe\b', line):
                self.handle_style_error(index + 1, 'webkit/unsafe', 5, "Please avoid new use of 'unsafe' in WebKit. See https://github.com/WebKit/WebKit/wiki/Safer-Swift-Guidelines.")

            if re.search(r'@safe\b', line) and not re.search(r'@unsafe\b', line):
                self.handle_style_error(index + 1, 'webkit/unsafe', 5, "Please avoid new use of '@safe' in WebKit. See https://github.com/WebKit/WebKit/wiki/Safer-Swift-Guidelines.")

    def _check_platform_conditions(self, lines):
        for index, line in enumerate(lines):
            if not self._CONDITIONAL_DIRECTIVE_RE.match(line):
                continue

            for pattern, condition, replacement in self._PLATFORM_CONDITIONS:
                for _ in pattern.finditer(line):
                    self.handle_style_error(index + 1, 'webkit/wtf_platform', 5, "Use '%s' instead of '%s' in conditional compilation, so that Swift and C++ platform checks agree." % (replacement, condition))

    def check(self, lines, line_numbers=None):
        self._swift_format(self.file_path, lines, self.handle_style_error)
        self._check_unsafe(lines)
        self._check_platform_conditions(lines)
