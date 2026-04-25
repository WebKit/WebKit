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

import unittest

from webkitscmpy.commit_parser import CommitMessageParser

COMMIT_MSG_BASE = '''Commit Message Title
https://bugs.example.com
rdar://1234567

Reviewed by NOBODY (OOPS!).

Commit description.
'''


class TestCommitParser(unittest.TestCase):

    def test_basic_commit(self):
        commit_message_parser = CommitMessageParser()
        commit_message_parser.parse_message(
            COMMIT_MSG_BASE + '''
* Change.py:
''')
        self.assertEqual(['Commit Message Title', 'https://bugs.example.com', 'rdar://1234567'], commit_message_parser.title_lines)
        self.assertEqual(['Reviewed by NOBODY (OOPS!).'], commit_message_parser.reviewed_by_lines)
        self.assertEqual(['Commit description.'], commit_message_parser.description_lines)
        self.assertEqual(['* Change.py:'], commit_message_parser.modified_files_lines)

    def test_changelog_comments(self):
        commit_message_parser = CommitMessageParser()
        commit_message_parser.parse_message(
            COMMIT_MSG_BASE + '''
* Change.py: Comment on the file.
* File.py: Added. Comment 2.
''')
        self.assertEqual(
            [
                '* Change.py: Comment on the file.',
                '* File.py: Added. Comment 2.',
            ],
            commit_message_parser.modified_files_lines,
        )
        self.assertEqual(
            [
                '* Change.py: Comment on the file.',
                '* File.py: Added. Comment 2.',
                '* new_file.cpp: Added.',
            ],
            commit_message_parser.apply_comments_to_modified_files_lines(
                [
                    '* Change.py:',
                    '* File.py: Added.',
                    '* new_file.cpp: Added.',
                ],
            ),
        )

    def test_removed_changes(self):
        commit_message_parser = CommitMessageParser()
        commit_message_parser.parse_message(
            COMMIT_MSG_BASE + '''
* Change.py: Comment on the file.
* File.py: Added. Comment 2.
(Class.function):
''')
        self.assertEqual(
            [
                '* Change.py: Comment on the file.',
                '* File.py: Added. Comment 2.',
                '(Class.function):',
            ],
            commit_message_parser.modified_files_lines,
        )
        self.assertEqual(['* Change.py: Comment on the file.'], commit_message_parser.apply_comments_to_modified_files_lines(['* Change.py:']))

    def test_return_deleted_simple(self):
        commit_message_parser = CommitMessageParser()
        commit_message_parser.parse_message(
            COMMIT_MSG_BASE + '''
* file1.py: Comment.
* file2.py:
* file3.py:
   - A newline comment.
(Class.function): Another comment.
''')
        self.assertEqual(
            [
                '* file1.py: Comment.',
                '* file2.py:',
                '* file3.py:',
                '   - A newline comment.',
                '(Class.function): Another comment.',
            ],
            commit_message_parser.modified_files_lines,
        )
        self.assertEqual(
            [
                '* file1.py: Comment.',
                '* file3.py:',
                '   - A newline comment.',
                '(Class.function): Another comment.'
            ],
            commit_message_parser.apply_comments_to_modified_files_lines(
                [
                    '* file1.py: Removed.',
                    '* file2.py: Added.',
                    '* file3.py: Removed.',
                    '(Class.function): Deleted.'
                ],
                return_deleted=True),
        )

    def test_return_deleted_additions(self):
        self.maxDiff = None
        commit_message_parser = CommitMessageParser()
        commit_message_parser.parse_message(
            COMMIT_MSG_BASE + '''
* file1.py: Comment.
* file2.py: Added.
* file3.py: Added.
   - A newline comment.
(Class.function): Another comment.
''')
        self.assertEqual(
            [
                '* file1.py: Comment.',
                '* file2.py: Added.',
                '* file3.py: Added.',
                '   - A newline comment.',
                '(Class.function): Another comment.',
            ],
            commit_message_parser.modified_files_lines,
        )
        self.assertEqual(
            [
                '* file1.py: Comment.', '* file3.py: Added.',
                '   - A newline comment.',
                '(Class.function): Another comment.',
            ],
            commit_message_parser.apply_comments_to_modified_files_lines(
                [
                    '* file1.py: Removed.',
                    '* file2.py: Added.',
                    '* file3.py: Removed.',
                    '(Class.function): Deleted.',
                ],
                return_deleted=True),
        )
