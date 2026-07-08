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

from webkitbugspy import Issue, radar
from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture
from webkitscmpy import local
from webkitscmpy.program.apply import Apply


class TestApply(unittest.TestCase):
    @staticmethod
    def _patch(name, contents=b'PATCH'):
        return Issue.Attachment(name=name, contents=contents)

    @staticmethod
    def _issue(patches=None, link='rdar://5', tracker_name='Radar'):
        issue = MagicMock(link=link)
        issue.patches = patches
        issue.tracker.NAME = tracker_name
        return issue

    def _repo(self, modified=None):
        repository = MagicMock(spec=local.Git)
        repository.modified.return_value = modified or {}
        repository.executable.return_value = 'git'
        repository.root_path = '/repo'
        return repository

    def test_resolve_issue_via_link(self):
        issue = MagicMock()
        with patch('webkitscmpy.program.apply.Tracker.from_string', return_value=issue):
            self.assertIs(Apply.resolve_issue('rdar://7'), issue)

    def test_resolve_issue_bare_id_via_radar_tracker(self):
        issue = MagicMock()
        tracker = MagicMock(spec=radar.Tracker)
        tracker.issue.return_value = issue
        with patch('webkitscmpy.program.apply.Tracker.from_string', return_value=None), \
             patch('webkitscmpy.program.apply.Tracker._trackers', [tracker]):
            self.assertIs(Apply.resolve_issue('179875292'), issue)
            tracker.issue.assert_called_once_with(179875292)

    def test_resolve_issue_does_not_coerce_non_numeric_string(self):
        # A malformed link must NOT be reduced to its digits and resolved as the wrong radar.
        tracker = MagicMock(spec=radar.Tracker)
        with patch('webkitscmpy.program.apply.Tracker.from_string', return_value=None), \
             patch('webkitscmpy.program.apply.Tracker._trackers', [tracker]):
            self.assertIsNone(Apply.resolve_issue('rdar://problem/42?x=9'))
            tracker.issue.assert_not_called()

    def test_select_patch_by_name(self):
        patches = [self._patch('triage-fix.patch'), self._patch('triage-revert.patch')]
        self.assertIs(Apply.select_patch(patches, name='triage-revert.patch'), patches[1])

    def test_select_patch_by_name_missing(self):
        self.assertIsNone(Apply.select_patch([self._patch('triage-fix.patch')], name='absent.patch'))

    def test_select_patch_single(self):
        patches = [self._patch('only.patch')]
        self.assertIs(Apply.select_patch(patches), patches[0])

    def test_select_patch_prompts_with_preferred_default(self):
        patches = [self._patch('triage-revert.patch'), self._patch('triage-fix.patch'), self._patch('other.patch')]
        with patch('webkitscmpy.program.apply.Terminal.choose', return_value='other.patch') as choose:
            selected = Apply.select_patch(patches)
        self.assertEqual(selected.name, 'other.patch')
        self.assertEqual(choose.call_args.kwargs['default'], 'triage-revert.patch')
        self.assertEqual(choose.call_args.kwargs['options'], ['other.patch', 'triage-fix.patch', 'triage-revert.patch'])

    def test_select_patch_defaults_to_first_when_none_preferred(self):
        patches = [self._patch('zeta.patch'), self._patch('alpha.patch')]
        with patch('webkitscmpy.program.apply.Terminal.choose', return_value='alpha.patch') as choose:
            Apply.select_patch(patches)
        self.assertEqual(choose.call_args.kwargs['default'], 'alpha.patch')

    @patch('webkitscmpy.program.apply.run')
    def test_main_rejects_non_git_repository(self, mock_run):
        with OutputCapture() as captured:
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), MagicMock(spec=local.Svn))
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), "Can only 'apply' on a native Git repository\n")
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_rejects_dirty_tree(self, mock_run):
        with OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo(modified={'f': 1}))
        self.assertEqual(result, 1)
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_issue_unresolvable(self, mock_run):
        with patch.object(Apply, 'resolve_issue', return_value=None), OutputCapture() as captured:
            result = Apply.main(Namespace(issue='bogus', name=None, commit=True), self._repo())
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), "Could not resolve 'bogus' to an issue\n")
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_attachments_unsupported(self, mock_run):
        issue = self._issue(patches=None, tracker_name='GitHub Issue')
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(Namespace(issue='https://github.example.com/x/1', name=None, commit=True), self._repo())
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), 'GitHub Issue does not support patch attachments\n')
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_no_patches(self, mock_run):
        issue = self._issue(patches=[])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('No patches are attached', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_named_patch_missing(self, mock_run):
        issue = self._issue(patches=[self._patch('triage-fix.patch')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(Namespace(issue='rdar://5', name='absent.patch', commit=True), self._repo())
        self.assertEqual(result, 1)
        self.assertIn("No 'absent.patch' patch is attached", captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_on_download_failure(self, mock_run):
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=None)])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('Could not download', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_on_empty_content(self, mock_run):
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'   \n')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=False), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('is empty', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_commits_with_git_am(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(mock_run.call_args.args[0][:2], ['git', 'am'])
        self.assertEqual(mock_run.call_args.kwargs.get('cwd'), '/repo')

    @patch('webkitscmpy.program.apply.run')
    def test_main_no_commit_uses_git_apply_3way(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=False), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(mock_run.call_args.args[0][:3], ['git', 'apply', '--3way'])

    @patch('webkitscmpy.program.apply.run')
    def test_main_aborts_am_on_conflict(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo())
        self.assertEqual(result, 1)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertEqual(commands[0][:2], ['git', 'am'])
        self.assertIn(['git', 'am', '--abort'], commands)

    @patch('webkitscmpy.program.apply.run')
    def test_main_no_commit_failure_does_not_abort(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=False), self._repo())
        self.assertEqual(result, 1)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertNotIn(['git', 'am', '--abort'], commands)
        self.assertEqual(len(commands), 1)

    @patch('webkitscmpy.program.apply.run')
    def test_main_removes_tempfile_even_on_failure(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo())
        patch_path = mock_run.call_args_list[0].args[0][2]
        self.assertFalse(os.path.exists(patch_path))

    @patch('webkitscmpy.program.apply.run')
    def test_main_encodes_str_content(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents='PATCH-AS-STR')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=False), self._repo())
        self.assertEqual(result, 0)

    def test_main_downloads_named_patch_from_radar(self):
        # End-to-end through real webkitbugspy + the radar mock: resolve rdar://1, fetch the named
        # patch's bytes via Issue.patches / Attachment.contents(), and hand them to `git am`.
        issues = [dict(
            id=1,
            title='Build failure',
            timestamp=1639536160,
            modified=1710884407,
            opened=True,
            creator=bmocks.USERS['Felix Filer'],
            assignee=bmocks.USERS['Tim Contributor'],
            description='triage output',
            attachments=[
                dict(fileName='triage-fix.patch', data=b'FIX BYTES'),
                dict(fileName='triage-revert.patch', data=b'REVERT BYTES'),
            ],
        )]
        written = {}

        def fake_run(command, **kwargs):
            if command[1] == 'am':
                with open(command[2], 'rb') as patch_file:
                    written['content'] = patch_file.read()
            return MagicMock(returncode=0)

        with bmocks.Radar(issues=issues), \
             patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]), \
             patch('webkitscmpy.program.apply.run', side_effect=fake_run), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://1', name='triage-fix.patch', commit=True), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(written.get('content'), b'FIX BYTES')

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_previews_diff_when_interactive(self, mock_run, mock_htmldiff):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo())
        self.assertEqual(result, 0)
        mock_htmldiff.assert_called_once()
        self.assertEqual(mock_htmldiff.call_args.kwargs.get('block'), True)
        self.assertEqual(mock_run.call_args.args[0][:2], ['git', 'am'])

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_skips_diff_when_not_interactive(self, mock_run, mock_htmldiff):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=False), OutputCapture():
            result = Apply.main(Namespace(issue='rdar://5', name=None, commit=True), self._repo())
        self.assertEqual(result, 0)
        mock_htmldiff.assert_not_called()


if __name__ == '__main__':
    unittest.main()
