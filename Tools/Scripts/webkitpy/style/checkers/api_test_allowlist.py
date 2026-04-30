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

# Valid entry format: BINARY.SUITE or BINARY.SUITE.TESTCASE
# BINARY is like TestWTF, TestWebKitAPI, TestIPC, TestWGSL
# SUITE can contain slashes for parametrized tests
# TESTCASE is the individual test name
_ENTRY_PATTERN = re.compile(r'^(Test\w+)\.([^.]+(?:/[^.]+)*)(?:\.(.+))?$')


class APITestAllowlistChecker(object):
    """Processes api_tests/allowlist.txt to enforce allowlist conventions."""

    def __init__(self, file_path, handle_style_error):
        self.file_path = file_path
        self.handle_style_error = handle_style_error

    def check(self, lines):
        seen = set()
        suites = set()
        prev_entry = ""

        for line_number, line in enumerate(lines, 1):
            line = line.rstrip('\n\r')
            stripped = line.strip()

            # Skip empty lines and comments
            if not stripped or stripped.startswith('#'):
                continue

            # Check for valid format
            match = _ENTRY_PATTERN.match(stripped)
            if not match:
                self.handle_style_error(line_number, 'api-test-allowlist/format', 5, f"Invalid entry format: '{stripped}' (expected BINARY.SUITE or BINARY.SUITE.TESTCASE)")
                continue

            binary, suite, testcase = match.groups()
            suite_key = f'{binary}.{suite}'

            # Check for duplicates
            if stripped in seen:
                self.handle_style_error(line_number, 'api-test-allowlist/duplicate', 5, f"Duplicate entry: '{stripped}'")
            seen.add(stripped)

            # Track suites (entries without testcase)
            if testcase is None:
                suites.add(suite_key)
            else:
                # Check if suite is already listed (redundant entry)
                if suite_key in suites:
                    self.handle_style_error(line_number, 'api-test-allowlist/redundant', 5, f"Redundant entry: '{stripped}' (suite '{suite_key}' already listed)")

            # Check alphabetical order
            if stripped < prev_entry:
                self.handle_style_error(line_number, 'api-test-allowlist/order', 5, f"Entry not in alphabetical order: '{stripped}' should come before '{prev_entry}'")
            prev_entry = stripped
