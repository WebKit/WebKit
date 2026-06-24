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
    maxDiff = None

    @staticmethod
    def lines(text):
        return text.strip('\n').split('\n')

    def assert_reconcile(self, before, changed, after):
        # `changed` is prepare-ChangeLog output: files sorted, comments stripped.
        commit_message_parser = CommitMessageParser()
        commit_message_parser.parse_message(COMMIT_MSG_BASE + before)
        self.assertEqual(
            self.lines(after),
            commit_message_parser.reconcile_with_changed_files(self.lines(changed)),
        )

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

    def test_reconcile_preserves_original_verbatim(self):
        self.assert_reconcile(
            before='''
* B.cpp: Comment b.
(B::foo):
(B::bar):
* A.cpp: Comment a.
(A::baz):
''',
            changed='''
* A.cpp:
(A::baz):
* B.cpp:
(B::bar):
(B::foo):
''',
            after='''
* B.cpp: Comment b.
(B::foo):
(B::bar):
* A.cpp: Comment a.
(A::baz):
''',
        )

    def test_reconcile_preserves_spacing(self):
        self.assert_reconcile(
            before='''
* Source/A.cpp: Comment.
(A::foo):

* LayoutTests/t.html: Comment.
''',
            changed='''
* LayoutTests/t.html:
* Source/A.cpp:
(A::foo):
''',
            after='''
* Source/A.cpp: Comment.
(A::foo):

* LayoutTests/t.html: Comment.
''',
        )

    def test_reconcile_drops_files_no_longer_changed(self):
        self.assert_reconcile(
            before='''
* B.cpp: Comment b.
(B::foo):
* A.cpp: Comment a.
''',
            changed='''
* A.cpp:
''',
            after='''
* A.cpp: Comment a.
''',
        )

    def test_reconcile_removed_file_keeps_trailing_notes(self):
        self.assert_reconcile(
            before='''
* Gone.cpp: File-level note.
(Gone::init): Function comment.
- A trailing note.
- Another trailing note.
* Kept.cpp: Still here.
''',
            changed='''
* Kept.cpp:
''',
            after='''
- A trailing note.
- Another trailing note.
* Kept.cpp: Still here.
''',
        )

    def test_reconcile_inserts_new_function_before_trailing_note(self):
        self.assert_reconcile(
            before='''
* Bar.cpp: Explanation.
(Bar::old): Did a thing.
- Trailing note.
''',
            changed='''
* Bar.cpp:
(Bar::old):
(Bar::new):
''',
            after='''
* Bar.cpp: Explanation.
(Bar::old): Did a thing.
(Bar::new):
- Trailing note.
''',
        )

    def test_reconcile_inserts_new_function_at_end_without_note(self):
        self.assert_reconcile(
            before='''
* Bar.cpp: Explanation.
(Bar::old): Did a thing.
''',
            changed='''
* Bar.cpp:
(Bar::old):
(Bar::new):
''',
            after='''
* Bar.cpp: Explanation.
(Bar::old): Did a thing.
(Bar::new):
''',
        )

    def test_reconcile_appends_new_files_at_end(self):
        self.assert_reconcile(
            before='''
* A.cpp: Comment a.
(A::foo):
''',
            changed='''
* A.cpp:
* C.cpp: Added.
(C::C):
''',
            after='''
* A.cpp: Comment a.
(A::foo):
* C.cpp: Added.
(C::C):
''',
        )

    def test_reconcile_keeps_original_status_and_comments(self):
        self.assert_reconcile(
            before='''
* Foo.cpp: Added. My note.
''',
            changed='''
* Foo.cpp:
''',
            after='''
* Foo.cpp: Added. My note.
''',
        )

    def test_reconcile_without_original_uses_generated(self):
        self.assert_reconcile(
            before='',
            changed='''
* A.cpp:
(A::foo):
* B.cpp:
''',
            after='''
* A.cpp:
(A::foo):
* B.cpp:
''',
        )

    def test_reconcile_keep_drop_and_add_together(self):
        self.assert_reconcile(
            before='''
* Kept.cpp: Keep me.
(Kept::a): Did a.
* Gone.cpp: Drop me.
(Gone::b): Old work.
- A note worth keeping.
''',
            changed='''
* Kept.cpp:
(Kept::a):
(Kept::new):
* New.cpp: Added.
(New::ctor):
''',
            after='''
* Kept.cpp: Keep me.
(Kept::a): Did a.
(Kept::new):
- A note worth keeping.
* New.cpp: Added.
(New::ctor):
''',
        )

    def test_reconcile_new_function_when_file_had_none(self):
        self.assert_reconcile(
            before='''
* Bar.cpp: Explanation.
''',
            changed='''
* Bar.cpp:
(Bar::new):
''',
            after='''
* Bar.cpp: Explanation.
(Bar::new):
''',
        )

    def test_reconcile_new_function_before_multiline_notes(self):
        self.assert_reconcile(
            before='''
* Bar.cpp:
(Bar::old):
- Note one.
- Note two.
''',
            changed='''
* Bar.cpp:
(Bar::old):
(Bar::new):
''',
            after='''
* Bar.cpp:
(Bar::old):
(Bar::new):
- Note one.
- Note two.
''',
        )

    def test_reconcile_multiple_new_functions_keep_order(self):
        self.assert_reconcile(
            before='''
* Bar.cpp:
(Bar::old):
''',
            changed='''
* Bar.cpp:
(Bar::old):
(Bar::new1):
(Bar::new2):
''',
            after='''
* Bar.cpp:
(Bar::old):
(Bar::new1):
(Bar::new2):
''',
        )

    def test_reconcile_keeps_function_no_longer_regenerated(self):
        self.assert_reconcile(
            before='''
* A.cpp: Comment.
(A::keep): Keep.
(A::gone): No longer touched.
''',
            changed='''
* A.cpp:
(A::keep):
''',
            after='''
* A.cpp: Comment.
(A::keep): Keep.
(A::gone): No longer touched.
''',
        )

    def test_reconcile_keeps_function_status(self):
        self.assert_reconcile(
            before='''
* Foo.cpp:
(Foo::bar): Deleted.
''',
            changed='''
* Foo.cpp:
(Foo::bar):
''',
            after='''
* Foo.cpp:
(Foo::bar): Deleted.
''',
        )

    def test_reconcile_removed_file_trims_separator_blank(self):
        self.assert_reconcile(
            before='''
* Gone.cpp:
(Gone::a):
- Keep this note.

* Kept.cpp:
''',
            changed='''
* Kept.cpp:
''',
            after='''
- Keep this note.
* Kept.cpp:
''',
        )
