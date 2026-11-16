# Copyright (C) 2024 Apple Inc. All rights reserved.
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

import re

from webkitexpectationspy.parser import ExpectationParser
from webkitexpectationspy.suites.api_tests import APITestSuite
from webkitpy.style.checkers.common import TabChecker

# Valid entry format: BINARY.SUITE or BINARY.SUITE.TESTCASE
# BINARY is like TestWTF, TestWebKitAPI, TestIPC, TestWGSL
# SUITE can contain slashes for parametrized tests
# TESTCASE is the individual test name
_ENTRY_PATTERN = re.compile(r'^(Test\w+)\.([^.]+(?:/[^.]+)*)(?:\.(.+))?$')

_UNGATED = (frozenset(), ())


class APITestAllowlistChecker(object):
    """Processes api_tests/allowlist.txt to enforce allowlist conventions."""

    categories = set([
        'api-test-allowlist/duplicate',
        'api-test-allowlist/format',
        'api-test-allowlist/order',
        'api-test-allowlist/redundant',
    ])

    def __init__(self, file_path, handle_style_error):
        self.file_path = file_path
        self.handle_style_error = handle_style_error
        self._tab_checker = TabChecker(file_path, handle_style_error)

    def check(self, lines, line_numbers=None):
        self._tab_checker.check(lines)

        entries = []
        parsed_lines = ExpectationParser(APITestSuite()).parse(self.file_path, '\n'.join(lines))
        for line_number, (entry, warnings) in enumerate(parsed_lines, 1):
            for warning in warnings:
                self.handle_style_error(line_number, 'api-test-allowlist/format', 5, warning.error)
            if entry is None:
                continue

            entry_text = lines[line_number - 1].split('#')[0].strip()
            pattern = entry.test_pattern
            if not _ENTRY_PATTERN.match(pattern):
                self.handle_style_error(line_number, 'api-test-allowlist/format', 5, f"Invalid entry format: '{entry_text}' (expected BINARY.SUITE or BINARY.SUITE.TESTCASE)")
            elif '*' in pattern:
                self.handle_style_error(line_number, 'api-test-allowlist/format', 5, f"Wildcard entries match no tests: '{entry_text}'")
            elif not entry_text.endswith(pattern):
                self.handle_style_error(line_number, 'api-test-allowlist/format', 5, f"Allowlist entries take no expectations or modifiers: '{entry_text}'")
            else:
                entries.append((line_number, entry_text, (entry.configurations, entry.version_specifiers), pattern))

        listed = {}
        for _, entry_text, gate, pattern in entries:
            listed.setdefault((gate, pattern), entry_text)

        seen = set()
        previous_pattern_by_gate = {}
        for line_number, entry_text, gate, pattern in entries:
            suite_key = pattern.rsplit('.', 1)[0]
            covering_entries = [listed[key] for key in [(_UNGATED, pattern), (_UNGATED, suite_key), (gate, suite_key)] if key != (gate, pattern) and key in listed]
            if (gate, pattern) in seen:
                self.handle_style_error(line_number, 'api-test-allowlist/duplicate', 5, f"Duplicate entry: '{entry_text}'")
            elif covering_entries:
                self.handle_style_error(line_number, 'api-test-allowlist/redundant', 5, f"Redundant entry: '{entry_text}' ('{covering_entries[0]}' already covers it)")
            seen.add((gate, pattern))

            previous_pattern = previous_pattern_by_gate.get(gate, '')
            if pattern < previous_pattern:
                self.handle_style_error(line_number, 'api-test-allowlist/order', 5, f"Entry not in alphabetical order: '{pattern}' should come before '{previous_pattern}'")
            previous_pattern_by_gate[gate] = pattern
