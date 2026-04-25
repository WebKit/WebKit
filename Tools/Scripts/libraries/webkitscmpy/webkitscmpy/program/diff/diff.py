# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re

from email.header import decode_header

from webkitscmpy import Commit


class DiffMeta(type):
    def __str__(cls):
        return cls.name


class DiffBase(object, metaclass=DiffMeta):
    FROM_COMMIT_RE = re.compile(r'^From\s+([a-f0-9A-F]+)')
    FROM_AUTHOR_RE = re.compile(r'^From:\s+(.+)$')
    SUBJECT_LINE_RE = re.compile(r'Subject:\s+\[PATCH(\s+\d+/\d+)?\] (.+)')
    URL_RE = re.compile(r'^\S+://\S+$')
    ADD_SUB_RE = re.compile(r'[^|]+\s+\|\s+(\d+)\s+(\+*)(\-*)$')

    def __init__(self, block=None, repository=None):
        self.block = block
        self.repository = repository

    def add_line(self, line):
        line = '\n' if line is None else line
        line = line + '\n' if not line or line[0] != '\n' else line

        commit_match = self.FROM_COMMIT_RE.match(line)
        if commit_match and self.repository:
            commit = self.repository.commit(hash=commit_match.group(1), include_log=False)
            return 'From {} ({})\n'.format(commit, commit_match.group(1)[:Commit.HASH_LABEL_SIZE])
        author_match = self.FROM_AUTHOR_RE.match(line)
        if author_match:
            result = 'From: '
            for part in decode_header(author_match.group(1)):
                result += part[0] if isinstance(part[0], str) else part[0].decode()
            return result
        return line

    def add_lines(self, lines):
        subject_lines = []
        for line in lines:
            stripped_line = line.strip()
            if not stripped_line:
                for s_line in subject_lines:
                    self.add_line(s_line + '\n')
                subject_lines = []
            elif subject_lines:
                if self.URL_RE.match(stripped_line):
                    subject_lines.append(stripped_line)
                else:
                    subject_lines[-1] += line.rstrip()
                continue

            subject_match = self.SUBJECT_LINE_RE.match(line)
            if subject_match:
                subject_lines.append(subject_match.group(2))
                continue
            self.add_line(line)

    def add_file(self, file):
        self.add_lines(file.readlines())
