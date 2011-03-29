#!/usr/bin/env python
#
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

import re
from common import TabChecker
from webkitpy.common.net.bugzilla import parse_bug_id_from_changelog


class ChangeLogChecker(object):

    """Processes text lines for checking style."""

    def __init__(self, file_path, handle_style_error):
        self.file_path = file_path
        self.handle_style_error = handle_style_error
        self._tab_checker = TabChecker(file_path, handle_style_error)

    def check_entry(self, entry_line_number, entry_lines):
        for line in entry_lines:
            if parse_bug_id_from_changelog(line):
                break
            if re.search("Unreviewed", line, re.IGNORECASE):
                break
            if re.search("build", line, re.IGNORECASE) and re.search("fix", line, re.IGNORECASE):
                break
        else:
            self.handle_style_error(entry_line_number + 1,
                                    "changelog/bugnumber", 5,
                                    "ChangeLog entry has no bug number")

    def check(self, lines):
        self._tab_checker.check(lines)
        entry_line_number = 0
        entry_lines = []
        started_at_first_line = False

        for line_number, line in enumerate(lines):
            if re.match("^\d{4}-\d{2}-\d{2}", line):
                if line_number:
                    self.check_entry(entry_line_number, entry_lines)
                else:
                    started_at_first_line = True
                entry_line_number = line_number
                entry_lines = []

            entry_lines.append(line)

        if started_at_first_line:
            self.check_entry(entry_line_number, entry_lines)
