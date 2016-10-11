# Copyright (C) 2011 Patrick Gansterer <paroga@paroga.com>
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

"""Checks WebKit style for ChangeLog files."""

from common import TabChecker, match, search, searchIgnorecase
from sys import maxsize
from webkitpy.common.checkout.changelog import parse_bug_id_from_changelog


class ChangeLogChecker(object):
    """Processes text lines for checking style."""

    categories = set(['changelog/bugnumber', 'changelog/filechangedescriptionwhitespace'])

    def __init__(self, file_path, handle_style_error, should_line_be_checked):
        self.file_path = file_path
        self.handle_style_error = handle_style_error
        self.should_line_be_checked = should_line_be_checked
        self._tab_checker = TabChecker(file_path, handle_style_error)

    def check_entry(self, first_line_checked, entry_lines):
        if not entry_lines:
            return
        for line in entry_lines:
            if parse_bug_id_from_changelog(line):
                break
            if searchIgnorecase("Unreviewed", line):
                break
            if searchIgnorecase("build", line) and searchIgnorecase("fix", line):
                break
        else:
            self.handle_style_error(first_line_checked,
                                    "changelog/bugnumber", 5,
                                    "ChangeLog entry has no bug number")
        # check file change descriptions for style violations
        line_no = first_line_checked - 1
        for line in entry_lines:
            line_no = line_no + 1
            # filter file change descriptions
            if not match('\s*\*\s', line):
                continue
            if search(':\s*$', line) or search(':\s', line):
                continue
            self.handle_style_error(line_no,
                                    "changelog/filechangedescriptionwhitespace", 5,
                                    "Need whitespace between colon and description")

        # check for a lingering "No new tests (OOPS!)." left over from prepare-changeLog.
        line_no = first_line_checked - 1
        for line in entry_lines:
            line_no = line_no + 1
            if match('\s*No new tests \(OOPS!\)\.$', line):
                self.handle_style_error(line_no,
                                        "changelog/nonewtests", 5,
                                        "You should remove the 'No new tests' and either add and list tests, or explain why no new tests were possible.")

        self.check_for_unwanted_security_phrases(first_line_checked, entry_lines)

    def check(self, lines):
        self._tab_checker.check(lines)
        first_line_checked = 0
        entry_lines = []

        for line_index, line in enumerate(lines):
            if not self.should_line_be_checked(line_index + 1):
                # If we transitioned from finding changed lines to
                # unchanged lines, then we are done.
                if first_line_checked:
                    break
                continue
            if not first_line_checked:
                first_line_checked = line_index + 1
            entry_lines.append(line)

        self.check_entry(first_line_checked, entry_lines)

    def contains_phrase_in_first_line_or_across_two_lines(self, phrase, line1, line2):
        return searchIgnorecase(phrase, line1) or ((not searchIgnorecase(phrase, line2)) and searchIgnorecase(phrase, line1 + " " + line2))

    def check_for_unwanted_security_phrases(self, first_line_checked, lines):
        unwanted_security_phrases = [
            "arbitrary code execution", "buffer overflow", "buffer overrun",
            "buffer underrun", "dangling pointer", "double free", "fuzzer", "fuzzing", "fuzz test",
            "invalid cast", "jsfunfuzz", "malicious", "memory corruption", "security bug",
            "security flaw", "use after free", "use-after-free", "UXSS",
            "WTFCrashWithSecurityImplication",
            "spoof",  # Captures spoof, spoofed, spoofing
            "vulnerab",  # Captures vulnerable, vulnerability, vulnerabilities
        ]

        lines_with_single_spaces = []
        for line in lines:
            lines_with_single_spaces.append(" ".join(line.split()))

        found_unwanted_security_phrases = []
        last_index = len(lines_with_single_spaces) - 1
        first_line_number_with_unwanted_phrase = maxsize
        for unwanted_phrase in unwanted_security_phrases:
            for line_index, line in enumerate(lines_with_single_spaces):
                next_line = "" if line_index >= last_index else lines_with_single_spaces[line_index + 1]
                if self.contains_phrase_in_first_line_or_across_two_lines(unwanted_phrase, line, next_line):
                    found_unwanted_security_phrases.append(unwanted_phrase)
                    first_line_number_with_unwanted_phrase = min(first_line_number_with_unwanted_phrase, first_line_checked + line_index)

        if len(found_unwanted_security_phrases) > 0:
            self.handle_style_error(first_line_number_with_unwanted_phrase,
                                    "changelog/unwantedsecurityterms", 3,
                                    "Please consider whether the use of security-sensitive phrasing could help someone exploit WebKit: {}".format(", ".join(found_unwanted_security_phrases)))
