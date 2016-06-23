# Copyright (C) 2013 University of Washington. All rights reserved.
# Copyright (C) 2014 Igalia S.L.
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

"""
Supports checking WebKit style in JavaScript files.

This checker is only used to check WebInspector JavaScript files.
"""

from common import TabChecker
import re


class JSChecker(object):
    """Processes JavaScript lines for checking style."""

    # FIXME: plug in a JavaScript parser to find syntax errors.
    categories = set(('js/syntax',))

    def __init__(self, file_path, handle_style_error):
        self._handle_style_error = handle_style_error
        self._tab_checker = TabChecker(file_path, handle_style_error)
        self._single_quote_checker = SingleQuoteChecker(file_path, handle_style_error)

    def check(self, lines):
        self._tab_checker.check(lines)
        self._single_quote_checker.check(lines)


class SingleQuoteChecker(object):
    """Checks there are not single quotes in source."""

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def check(self, lines):
        in_multiline_comment = False
        line_number = 0
        for line in lines:
            line = line.strip()
            line_number = line_number + 1

            if (line.endswith("*/")):
                in_multiline_comment = False
                continue

            if (line.startswith("/*") or line.startswith("*")):
                in_multiline_comment = True
                continue

            # Remove "double quoted" strings.
            line = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)

            # Remove single line comment if any.
            single_line_comment_pos = line.find('//')
            if (single_line_comment_pos != -1):
                line = line[:single_line_comment_pos]

            # The whole line was a single line comment, so continue.
            if (len(line) == 0):
                continue

            # Remove regexes.
            line = re.sub('/.+?/', '//', line)

            single_quote_pos = line.find("'")
            if (single_quote_pos != -1):
                self._handle_style_error(line_number, "js/syntax", 5, "Line contains single-quote character.")
