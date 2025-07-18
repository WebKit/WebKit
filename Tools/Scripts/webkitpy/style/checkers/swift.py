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
    def __init__(self, file_path, handle_style_error):
        self.file_path = file_path
        self.handle_style_error = handle_style_error

    def _process_lines(self, file_path, lines, error):
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

            self.handle_style_error(int(line_number), category, 5, message)

    def check(self, lines):
        self._process_lines(self.file_path, lines, self.handle_style_error)
