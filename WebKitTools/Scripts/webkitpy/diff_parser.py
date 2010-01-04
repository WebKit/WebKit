# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

"""WebKit's Python module for interacting with patches."""

import logging
import re


_regexp_compile_cache = {}


def match(pattern, string):
    """Matches the string with the pattern, caching the compiled regexp."""
    if not pattern in _regexp_compile_cache:
        _regexp_compile_cache[pattern] = re.compile(pattern)
    return _regexp_compile_cache[pattern].match(string)


def git_diff_to_svn_diff(line):
    """Converts a git formatted diff line to a svn formatted line.

    Args:
      line: A string representing a line of the diff.
    """
    conversion_patterns = (("^diff --git \w/(.+) \w/(?P<FilePath>.+)", lambda matched: "Index: " + matched.group('FilePath') + "\n"),
                           ("^new file.*", lambda matched: "\n"),
                           ("^index [0-9a-f]{7}\.\.[0-9a-f]{7} [0-9]{6}", lambda matched: "===================================================================\n"),
                           ("^--- \w/(?P<FilePath>.+)", lambda matched: "--- " + matched.group('FilePath') + "\n"),
                           ("^\+\+\+ \w/(?P<FilePath>.+)", lambda matched: "+++ " + matched.group('FilePath') + "\n"))

    for pattern, conversion in conversion_patterns:
        matched = match(pattern, line)
        if matched:
            return conversion(matched)
    return line


def get_diff_converter(first_diff_line):
    """Gets a converter function of diff lines.

    Args:
      first_diff_line: The first filename line of a diff file.
                       If this line is git formatted, we'll return a
                       converter from git to SVN.
    """
    if match(r"^diff --git \w/", first_diff_line):
        return git_diff_to_svn_diff
    return lambda input: input


_INITIAL_STATE = 1
_DECLARED_FILE_PATH = 2
_PROCESSING_CHUNK = 3


class DiffFile:
    """Contains the information for one file in a patch.

    The field "lines" is a list which contains tuples in this format:
       (deleted_line_number, new_line_number, line_string)
    If deleted_line_number is zero, it means this line is newly added.
    If new_line_number is zero, it means this line is deleted.
    """

    def __init__(self, filename):
        self.filename = filename
        self.lines = []

    def add_new_line(self, line_number, line):
        self.lines.append((0, line_number, line))

    def add_deleted_line(self, line_number, line):
        self.lines.append((line_number, 0, line))

    def add_unchanged_line(self, deleted_line_number, new_line_number, line):
        self.lines.append((deleted_line_number, new_line_number, line))


class DiffParser:
    """A parser for a patch file.

    The field "files" is a dict whose key is the filename and value is
    a DiffFile object.
    """

    def __init__(self, diff_input):
        """Parses a diff.

        Args:
          diff_input: An iterable object.
        """
        state = _INITIAL_STATE

        self.files = {}
        current_file = None
        old_diff_line = None
        new_diff_line = None
        for line in diff_input:
            line = line.rstrip("\n")
            if state == _INITIAL_STATE:
                transform_line = get_diff_converter(line)
            line = transform_line(line)

            file_declaration = match(r"^Index: (?P<FilePath>.+)", line)
            if file_declaration:
                filename = file_declaration.group('FilePath')
                current_file = DiffFile(filename)
                self.files[filename] = current_file
                state = _DECLARED_FILE_PATH
                continue

            lines_changed = match(r"^@@ -(?P<OldStartLine>\d+)(,\d+)? \+(?P<NewStartLine>\d+)(,\d+)? @@", line)
            if lines_changed:
                if state != _DECLARED_FILE_PATH and state != _PROCESSING_CHUNK:
                    logging.error('Unexpected line change without file path declaration: %r' % line)
                old_diff_line = int(lines_changed.group('OldStartLine'))
                new_diff_line = int(lines_changed.group('NewStartLine'))
                state = _PROCESSING_CHUNK
                continue

            if state == _PROCESSING_CHUNK:
                if line.startswith('+'):
                    current_file.add_new_line(new_diff_line, line[1:])
                    new_diff_line += 1
                elif line.startswith('-'):
                    current_file.add_deleted_line(old_diff_line, line[1:])
                    old_diff_line += 1
                elif line.startswith(' '):
                    current_file.add_unchanged_line(old_diff_line, new_diff_line, line[1:])
                    old_diff_line += 1
                    new_diff_line += 1
                elif line == '\\ No newline at end of file':
                    # Nothing to do.  We may still have some added lines.
                    pass
                else:
                    logging.error('Unexpected diff format when parsing a chunk: %r' % line)
