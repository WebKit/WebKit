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

import os
import unittest
from argparse import Namespace
from unittest.mock import MagicMock, patch

from webkitcorepy import OutputCapture
from webkitscmpy import local
from webkitscmpy.program.apply import Apply


class TestApply(unittest.TestCase):
    @staticmethod
    def _attachment(file_name, content=b'PATCH', locked=False):
        attachment = MagicMock(fileName=file_name, locked=locked)
        attachment.content.return_value = content
        return attachment

    def _issue_with_attachments(self, attachments):
        issue = MagicMock(id=42, link='rdar://42')
        issue.tracker.client.radar_for_id.return_value.attachments.items.return_value = attachments
        return issue

    def _repo(self, modified=None):
        repository = MagicMock(spec=local.Git)
        repository.modified.return_value = modified or {}
        repository.executable.return_value = 'git'
        repository.root_path = '/repo'
        return repository

    def test_attachment_name(self):
        self.assertEqual(Apply.attachment_name('revert'), 'triage-revert.patch')
        self.assertEqual(Apply.attachment_name('fix'), 'triage-fix.patch')

    def test_contents_returns_matching_attachment(self):
        issue = self._issue_with_attachments([
            self._attachment('unrelated.txt', content=b'NOPE'),
            self._attachment('triage-revert.patch', content=b'REVERT'),
        ])
        self.assertEqual(Apply.contents(issue, 'triage-revert.patch'), b'REVERT')

    def test_contents_missing_returns_none(self):
        issue = self._issue_with_attachments([self._attachment('triage-fix.patch')])
        with OutputCapture():
            self.assertIsNone(Apply.contents(issue, 'triage-revert.patch'))

    def test_contents_locked_returns_none(self):
        issue = self._issue_with_attachments([self._attachment('triage-revert.patch', locked=True)])
        with OutputCapture():
            self.assertIsNone(Apply.contents(issue, 'triage-revert.patch'))

    def test_contents_without_attachment_support_returns_none(self):
        issue = MagicMock(id=1, link='rdar://1')
        issue.tracker.client = None
        with OutputCapture():
            self.assertIsNone(Apply.contents(issue, 'triage-revert.patch'))

    def test_contents_fetch_error_returns_none(self):
        # radar_for_id raises (not returns None) for a missing/unauthorized radar.
        issue = MagicMock(id=7, link='rdar://7')
        issue.tracker.client.radar_for_id.side_effect = RuntimeError('access denied')
        with OutputCapture() as captured:
            self.assertIsNone(Apply.contents(issue, 'triage-revert.patch'))
        self.assertIn('Could not read attachments', captured.stderr.getvalue())

    def test_resolve_issue_via_link(self):
        issue = MagicMock()
        with patch('webkitscmpy.program.apply.Tracker.from_string', return_value=issue):
            self.assertIs(Apply.resolve_issue('rdar://7'), issue)

    def test_resolve_issue_bare_id_via_radar_tracker(self):
        issue = MagicMock()
        tracker = MagicMock()
        tracker.issue.return_value = issue
        with patch('webkitscmpy.program.apply.Tracker.from_string', return_value=None), \
             patch.object(Apply, 'radar_tracker', return_value=tracker):
            self.assertIs(Apply.resolve_issue('179875292'), issue)
            tracker.issue.assert_called_once_with(179875292)

    def test_resolve_issue_does_not_coerce_non_numeric_string(self):
        # A malformed link must NOT be reduced to its digits and resolved as the wrong radar.
        with patch('webkitscmpy.program.apply.Tracker.from_string', return_value=None), \
             patch.object(Apply, 'radar_tracker') as radar_tracker:
            self.assertIsNone(Apply.resolve_issue('rdar://problem/42?x=9'))
            radar_tracker.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_rejects_non_git_repository(self, mock_run):
        with OutputCapture() as captured:
            result = Apply.main(Namespace(which='revert', issue='rdar://5', commit=True), MagicMock(spec=local.Svn))
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), "Can only 'apply' on a native Git repository\n")
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_rejects_dirty_tree(self, mock_run):
        with OutputCapture():
            result = Apply.main(Namespace(which='revert', issue='rdar://5', commit=True), self._repo(modified={'f': 1}))
        self.assertEqual(result, 1)
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_issue_unresolvable(self, mock_run):
        with patch.object(Apply, 'resolve_issue', return_value=None), OutputCapture() as captured:
            result = Apply.main(Namespace(which='revert', issue='bogus', commit=True), self._repo())
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), "Could not resolve 'bogus' to a radar\n")
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_attachment_missing(self, mock_run):
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value=None), OutputCapture():
            result = Apply.main(Namespace(which='revert', issue='rdar://5', commit=True), self._repo())
        self.assertEqual(result, 1)
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_on_empty_content(self, mock_run):
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value=b'   \n'), OutputCapture() as captured:
            result = Apply.main(Namespace(which='fix', issue='rdar://5', commit=False), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('is empty', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_commits_with_git_am(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value=b'PATCH'), OutputCapture():
            result = Apply.main(Namespace(which='revert', issue='rdar://5', commit=True), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(mock_run.call_args.args[0][:2], ['git', 'am'])
        self.assertEqual(mock_run.call_args.kwargs.get('cwd'), '/repo')

    @patch('webkitscmpy.program.apply.run')
    def test_main_no_commit_uses_git_apply_3way(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value=b'PATCH'), OutputCapture():
            result = Apply.main(Namespace(which='fix', issue='rdar://5', commit=False), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(mock_run.call_args.args[0][:3], ['git', 'apply', '--3way'])

    @patch('webkitscmpy.program.apply.run')
    def test_main_aborts_am_on_conflict(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value=b'PATCH'), OutputCapture():
            result = Apply.main(Namespace(which='revert', issue='rdar://5', commit=True), self._repo())
        self.assertEqual(result, 1)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertEqual(commands[0][:2], ['git', 'am'])
        self.assertIn(['git', 'am', '--abort'], commands)

    @patch('webkitscmpy.program.apply.run')
    def test_main_no_commit_failure_does_not_abort(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value=b'PATCH'), OutputCapture():
            result = Apply.main(Namespace(which='fix', issue='rdar://5', commit=False), self._repo())
        self.assertEqual(result, 1)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertNotIn(['git', 'am', '--abort'], commands)
        self.assertEqual(len(commands), 1)

    @patch('webkitscmpy.program.apply.run')
    def test_main_removes_tempfile_even_on_failure(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value=b'PATCH'), OutputCapture():
            Apply.main(Namespace(which='revert', issue='rdar://5', commit=True), self._repo())
        patch_path = mock_run.call_args_list[0].args[0][2]
        self.assertFalse(os.path.exists(patch_path))

    @patch('webkitscmpy.program.apply.run')
    def test_main_encodes_str_content(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        with patch.object(Apply, 'resolve_issue', return_value=MagicMock(link='rdar://5')), \
             patch.object(Apply, 'contents', return_value='PATCH-AS-STR'), OutputCapture():
            result = Apply.main(Namespace(which='fix', issue='rdar://5', commit=False), self._repo())
        self.assertEqual(result, 0)


if __name__ == '__main__':
    unittest.main()
