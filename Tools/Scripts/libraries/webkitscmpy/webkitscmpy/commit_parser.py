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

import logging
import re
import sys

from enum import Enum


class ParseState(Enum):
    STATE_TITLE = 1
    STATE_REVIEWED_BY = 2
    STATE_DESCRIPTION = 3
    STATE_TESTS = 4
    STATE_MODIFIED_FILES = 5


MODIFIED_FILE_PATTERN = re.compile('^(\\*.*:)(.*)$')
MODIFIED_FILE_ADDED_PATTERN = re.compile('^\\*.*: Added.$')
MODIFIED_FILE_REMOVED_PATTERN = re.compile('^\\*.*: Removed.$')
MODIFIED_FUNCTION_PATTERN = re.compile('^(\\(.*\\):)(.*)$')
MODIFIED_FUNCTION_DELETED_PATTERN = re.compile('^\\(.*\\): Deleted.$')
TEST_PATTERN = re.compile('^Tests?: .*$')
NO_TEST_PATTERN = re.compile('^No new tests \\(OOPS!\\)\\.$')
TEST_CONTENT_PATTERN = re.compile('^ +.*$')
REVIEWED_BY_PATTERN = re.compile('^Reviewed by .*$')


class CommitMessageParser:
    def __init__(self):
        """
        Parsing happens in two phases:
        Pass 1 separates the commit message in 5 parts described below.
        Pass 2 gathers the comments corresponding to the modified files & functions.
        """

        # Result of parsing pass 1:
        # These are all represented as a list of strings.
        #
        # The title of the commit message.
        self.title_lines = []
        # The "Reviewed by" line.
        self.reviewed_by_lines = []
        # The description of the commit.
        self.description_lines = []
        # The tests information.
        self.tests_lines = []
        # The modified files & functions.
        self.modified_files_lines = []

        # Result of parsing pass 2:
        #
        # The map of comments. The key is the line containing the filename returned by prepare-ChangeLog minus the comments.
        # The value is a list of strings.
        self.files_to_comments = {}
        # The map to the list of functions with comments. The key is the line containing the filename returned by
        # prepare-ChangeLog minus the comments. The value is another map that has the comments of the functions.
        # On this map, the key is the line containing the function name returned by prepare-ChangeLog minus the comments.
        # The value is a list of strings.
        self.files_to_functions = {}

    def parse_file(self, path):
        """Parse the commit message

        :param path:
        Path to the file containing the commit message to parse.
        """
        with open(path) as f:
            self.parse_message(f.readlines())

    def parse_message(self, message):
        """Parse the commit message

        :param message:
        String containing the commit message to parse.
        """
        state = ParseState.STATE_TITLE

        self.title_lines = []
        self.reviewed_by_lines = []
        self.description_lines = []
        self.tests_lines = []
        self.modified_files_lines = []

        for line in message.split('\n'):
            line = line.rstrip()

            if line.startswith('#'):
                continue

            if state == ParseState.STATE_TITLE:
                if REVIEWED_BY_PATTERN.match(line):
                    state = ParseState.STATE_REVIEWED_BY
                    self.reviewed_by_lines.append(line)
                elif MODIFIED_FILE_PATTERN.match(line):
                    state = ParseState.STATE_MODIFIED_FILES
                    self.modified_files_lines.append(line)
                elif TEST_PATTERN.match(line) or NO_TEST_PATTERN.match(line):
                    state = ParseState.STATE_TESTS
                    self.tests_lines.append(line)
                else:
                    self.title_lines.append(line)

            # Parse the section with "Reviewed by"
            elif state == ParseState.STATE_REVIEWED_BY:
                if REVIEWED_BY_PATTERN.match(line):
                    self.reviewed_by_lines.append(line)
                elif line == '':
                    state = ParseState.STATE_DESCRIPTION
                else:
                    state = ParseState.STATE_DESCRIPTION
                    self.description_lines.append(line)

            # Parse the section with the description of the commit.
            elif state == ParseState.STATE_DESCRIPTION:
                if MODIFIED_FILE_PATTERN.match(line):
                    state = ParseState.STATE_MODIFIED_FILES
                    self.modified_files_lines.append(line)
                elif TEST_PATTERN.match(line) or NO_TEST_PATTERN.match(line):
                    state = ParseState.STATE_TESTS
                    self.tests_lines.append(line)
                else:
                    self.description_lines.append(line)

            # Parse the test section.
            elif state == ParseState.STATE_TESTS:
                if TEST_PATTERN.match(line) or NO_TEST_PATTERN.match(line):
                    self.tests_lines.append(line)
                elif TEST_CONTENT_PATTERN.match(line):
                    self.tests_lines.append(line)
                elif MODIFIED_FILE_PATTERN.match(line):
                    state = ParseState.STATE_MODIFIED_FILES
                    self.modified_files_lines.append(line)
                elif line == '':
                    state = ParseState.STATE_MODIFIED_FILES
                else:
                    state = ParseState.STATE_MODIFIED_FILES
                    self.modified_files_lines.append(line)

            # Parse the section with the list of modified files & functions.
            elif state == ParseState.STATE_MODIFIED_FILES:
                self.modified_files_lines.append(line)

        self.title_lines = self.delete_trailing_blank_lines(self.title_lines)
        self.description_lines = self.delete_trailing_blank_lines(self.description_lines)
        self.modified_files_lines = self.delete_trailing_blank_lines(self.modified_files_lines)

        self._parse_modified_files_lines()

    def _parse_modified_files_lines(self):
        """Parse the list of modified files & functions passed as a list of strings.

        :param modified_file_lines:
        The modified files & functions. It's passed as a list of strings.
        It's in the format generated by prepare-ChangeLog.
        """
        current_file = None
        current_function = None

        current_comments = []
        files_to_comments = {}
        files_to_functions = {}

        for line in self.modified_files_lines:
            if MODIFIED_FILE_REMOVED_PATTERN.match(line):
                current_file = line
                current_function = None
                current_comments = ['']
                files_to_comments[current_file] = current_comments
                files_to_functions[current_file] = {}

            elif MODIFIED_FILE_ADDED_PATTERN.match(line):
                current_file = line
                current_function = None
                current_comments = ['']
                files_to_comments[current_file] = current_comments
                files_to_functions[current_file] = {}

            else:
                m = MODIFIED_FILE_PATTERN.match(line)
                if m:
                    current_file = m.group(1)
                    comment = m.group(2)

                    current_function = None
                    current_comments = [comment]

                    files_to_comments[current_file] = current_comments
                    files_to_functions[current_file] = {}

                elif MODIFIED_FUNCTION_DELETED_PATTERN.match(line):
                    if not current_file:
                        logging.warning("Ignoring a function outside of a context of a file: " + line)
                        continue

                    current_function = line
                    current_comments = ['']
                    files_to_functions[current_file][current_function] = current_comments

                else:
                    m = MODIFIED_FUNCTION_PATTERN.match(line)
                    if m:
                        current_function = m.group(1)
                        comment = m.group(2)
                        current_comments = [comment]

                        files_to_functions[current_file][current_function] = current_comments
                    else:
                        if not current_function and not current_file:
                            logging.warning("Ignoring a comment outside of a context of a file or a function: " + line)
                            continue

                        current_comments.append(line)

        self.files_to_comments = files_to_comments
        self.files_to_functions = files_to_functions

    def apply_comments_to_modified_files_lines(self, modified_files_lines, return_deleted=False):
        """Apply the comments given via files_to_comments & files_to_functions to the list of modified files & functions
        passed as modified_files_lines.

        :param modified_file_lines:
        The modified files & functions along with test information. It's passed as a list of strings.
        It's in the format generated by prepare-ChangeLog.

        :return:
        The function returns a list of string that is the result of applying the comments to the updated list of
        modified files & functions.
        """
        result = []
        deleted_result = []
        for line in modified_files_lines:
            if line == '':
                result.append(line)

            elif TEST_PATTERN.match(line) or NO_TEST_PATTERN.match(line):
                result.append(line)

            elif TEST_CONTENT_PATTERN.match(line):
                result.append(line)

            elif MODIFIED_FILE_REMOVED_PATTERN.match(line):
                current_file = line
                current_function = None
                lines = self.files_to_comments.get(current_file)
                if not lines:
                    current_file = current_file.removesuffix(' Removed.')
                    lines = self.files_to_comments.get(current_file)
                if not lines:
                    current_file += ' Added.'
                    lines = self.files_to_comments.get(current_file)
                if lines:
                    if len(lines) > 0:
                        line = current_file + lines[0]
                        result.append(line)
                        result += lines[1:]
                        deleted_result.append(line)
                        deleted_result += lines[1:]
                else:
                    result.append(line)
                    deleted_result.append(line)

            elif MODIFIED_FILE_ADDED_PATTERN.match(line):
                current_file = line
                current_function = None
                lines = self.files_to_comments.get(current_file)
                if not lines:
                    current_file = current_file.removesuffix(' Added.')
                    lines = self.files_to_comments.get(current_file)
                if lines:
                    if len(lines) > 0:
                        line = current_file + lines[0]
                        result.append(line)
                        result += lines[1:]
                else:
                    result.append(line)

            else:
                m = MODIFIED_FILE_PATTERN.match(line)
                if m:
                    current_file = m.group(1)
                    current_function = None
                    lines = self.files_to_comments.get(current_file)
                    if lines:
                        if len(lines) > 0:
                            line = current_file + lines[0]
                            result.append(line)
                            result += lines[1:]
                    else:
                        result.append(line)

                elif MODIFIED_FUNCTION_DELETED_PATTERN.match(line):
                    if not current_file:
                        logging.warning("Ignoring a function outside of a context of a file: " + line)
                        continue

                    current_function = line

                    current_functions = self.files_to_functions.get(current_file)
                    if current_functions:
                        lines = current_functions.get(current_function)
                        if not lines:
                            current_function = current_function.removesuffix(' Deleted.')
                            lines = current_functions.get(current_function)
                        if lines:
                            if len(lines) > 0:
                                line = current_function + lines[0]
                                result.append(line)
                                result += lines[1:]
                                deleted_result.append(line)
                                deleted_result += lines[1:]
                        else:
                            result.append(line)
                            deleted_result.append(line)
                    else:
                        result.append(line)
                        deleted_result.append(line)

                else:
                    m = MODIFIED_FUNCTION_PATTERN.match(line)
                    if m:
                        current_function = m.group(1)
                        comment = m.group(2)
                        current_functions = self.files_to_functions.get(current_file)
                        if current_functions:
                            lines = current_functions.get(current_function)
                            if lines:
                                if len(lines) > 0:
                                    line = current_function + lines[0]
                                    result.append(line)
                                    result += lines[1:]
                            else:
                                result.append(line)
                        else:
                            result.append(line)
                    else:
                        logging.warning("Ignoring an unexpected line: " + line)

        if return_deleted:
            return deleted_result
        return result

    def _file_identity(self, line):
        # The "* path:" prefix is identical whether the file is modified, Added. or
        # Removed., so a file keeps one identity as its status changes.
        match = MODIFIED_FILE_PATTERN.match(line)
        return match.group(1) if match else None

    def _group_into_blocks(self, lines):
        # A file header opens a block; the function, comment and blank lines after it
        # belong to it. Lines before the first header are returned as the preamble.
        preamble = []
        blocks = []
        for line in lines:
            identity = self._file_identity(line)
            if identity is not None:
                blocks.append((identity, [line]))
            elif blocks:
                blocks[-1][1].append(line)
            else:
                preamble.append(line)
        return preamble, blocks

    def reconcile_with_changed_files(self, modified_files_lines):
        """Update the original modified-files section to match the current diff.

        The original section is kept as written -- order, comments, spacing -- except:
        a file no longer changed keeps only its trailing notes, a file that gained
        functions has the new markers added before its trailing notes, and files new
        since the original message are appended at the end.

        modified_files_lines is the regenerated list (prepare-ChangeLog format).
        """
        _, current_blocks = self._group_into_blocks(modified_files_lines)
        current_order = []
        generated_block = {}
        for identity, lines in current_blocks:
            if identity not in generated_block:
                current_order.append(identity)
            generated_block[identity] = lines
        current_identities = set(current_order)

        preamble, original_blocks = self._group_into_blocks(self.modified_files_lines)
        original_identities = {identity for identity, _ in original_blocks}

        result = list(preamble)
        for identity, lines in original_blocks:
            if identity in current_identities:
                result.extend(self._add_new_functions(lines, generated_block[identity]))
            else:
                result.extend(self._trailing_notes(lines))
        for identity in current_order:
            if identity not in original_identities:
                result.extend(generated_block[identity])
        return result

    def _function_identity(self, line):
        # The "(function):" prefix is identical across plain, commented and Deleted.
        # lines, so a function keeps one identity as its status changes.
        match = MODIFIED_FUNCTION_PATTERN.match(line)
        return match.group(1) if match else None

    def _trailing_notes(self, block_lines):
        # A file that's no longer changed loses its file and function markers (and the
        # notes on them); only free-form notes survive. Surrounding blanks are trimmed
        # so the removed markers don't leave stray blank lines behind.
        notes = [
            line for line in block_lines
            if self._file_identity(line) is None and self._function_identity(line) is None
        ]
        start = 0
        while start < len(notes) and not notes[start].strip():
            start += 1
        end = len(notes)
        while end > start and not notes[end - 1].strip():
            end -= 1
        return notes[start:end]

    def _add_new_functions(self, original_lines, generated_lines):
        original_functions = {
            identity for identity in map(self._function_identity, original_lines)
            if identity is not None
        }
        new_function_lines = [
            line for line in generated_lines
            if self._function_identity(line) is not None
            and self._function_identity(line) not in original_functions
        ]
        if not new_function_lines:
            return list(original_lines)

        # After the last marker, i.e. before any trailing notes.
        insert_at = len(original_lines)
        while insert_at > 0:
            previous = original_lines[insert_at - 1]
            if self._file_identity(previous) is not None or self._function_identity(previous) is not None:
                break
            insert_at -= 1
        return original_lines[:insert_at] + new_function_lines + original_lines[insert_at:]

    def delete_trailing_blank_lines(self, lines):
        chomp_at = 0
        for line in reversed(lines):
            if line != '':
                break

            chomp_at -= 1

        if chomp_at < 0:
            return lines[:chomp_at]

        return lines
