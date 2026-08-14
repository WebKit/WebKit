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
from argparse import ArgumentParser, Namespace
from unittest.mock import MagicMock, patch

from webkitbugspy import Issue, radar
from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture
from webkitscmpy import local
from webkitscmpy.program.apply import Apply


class TestApply(unittest.TestCase):
    AMEND_RESET = ['git', 'commit', '--amend', '--reset-author', '--no-edit']
    REPLAY_RESET = [
        'git', 'rebase', '--no-autosquash',
        '--exec', 'git commit --amend --reset-author --no-edit',
        'HEAD-BEFORE-AM',
    ]

    @staticmethod
    def _args(issue='rdar://5', name=None, commit=True, reset_author=False):
        return Namespace(issue=issue, name=name, commit=commit, reset_author=reset_author)

    @staticmethod
    def _run(head='HEAD-BEFORE-AM', count='1', failures=()):
        '''Stub for `run` answering the queries that resetting the author makes, and failing the git
        sub-commands named in `failures`. A None `head` or `count` makes that query fail.'''
        def invoke(command, **kwargs):
            if command[1] == 'rev-parse':
                return MagicMock(returncode=1) if head is None else MagicMock(returncode=0, stdout='{}\n'.format(head))
            if command[1] == 'rev-list':
                return MagicMock(returncode=1) if count is None else MagicMock(returncode=0, stdout='{}\n'.format(count))
            return MagicMock(returncode=1 if command[1] in failures else 0)
        return invoke

    @staticmethod
    def _commands(mock_run, queries=False):
        '''Git commands run, without the read-only queries unless they are asked for.'''
        commands = [call.args[0] for call in mock_run.call_args_list]
        if queries:
            return commands
        return [command for command in commands if command[1] not in ('rev-parse', 'rev-list')]

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

    def test_select_patch_named_reviewed_when_interactive(self):
        patches = [self._patch('triage-fix.patch'), self._patch('triage-revert.patch')]
        with patch.object(Apply, 'review', return_value=True) as review:
            selected = Apply.select_patch(patches, name='triage-fix.patch', repository=self._repo(), interactive=True)
        self.assertIs(selected, patches[0])
        review.assert_called_once()

    def test_select_patch_named_rejected_when_interactive_returns_none(self):
        patches = [self._patch('triage-fix.patch')]
        with patch.object(Apply, 'review', return_value=False):
            self.assertIsNone(Apply.select_patch(patches, name='triage-fix.patch', repository=self._repo(), interactive=True))

    def test_select_patch_named_non_interactive_skips_review(self):
        patches = [self._patch('triage-fix.patch')]
        with patch.object(Apply, 'review') as review:
            self.assertIs(Apply.select_patch(patches, name='triage-fix.patch'), patches[0])
        review.assert_not_called()

    def test_select_patch_non_interactive_returns_single(self):
        patches = [self._patch('only.patch')]
        self.assertIs(Apply.select_patch(patches), patches[0])

    def test_select_patch_non_interactive_returns_first_in_tracker_order(self):
        patches = [self._patch('triage-revert.patch'), self._patch('triage-fix.patch')]
        self.assertIs(Apply.select_patch(patches), patches[0])

    def test_select_patch_menu_preserves_tracker_order_and_defaults_to_first(self):
        patches = [self._patch('triage-revert.patch'), self._patch('triage-fix.patch')]
        with patch('webkitscmpy.program.apply.Terminal.choose', return_value='triage-fix.patch') as choose, \
             patch.object(Apply, 'review', return_value=True):
            Apply.select_patch(patches, repository=self._repo(), interactive=True)
        self.assertEqual(choose.call_args.kwargs['options'], ['triage-revert.patch', 'triage-fix.patch'])
        self.assertEqual(choose.call_args.kwargs['default'], 'triage-revert.patch')

    def test_select_patch_offers_every_variant_in_the_menu(self):
        patches = [self._patch('triage-fix.patch'), self._patch('triage-fix[2].patch')]
        with patch('webkitscmpy.program.apply.Terminal.choose', return_value='triage-fix[2].patch') as choose, \
             patch.object(Apply, 'review', return_value=True):
            selected = Apply.select_patch(patches, repository=self._repo(), interactive=True)
        self.assertEqual(selected.name, 'triage-fix[2].patch')
        self.assertEqual(sorted(choose.call_args.kwargs['options']), ['triage-fix.patch', 'triage-fix[2].patch'])

    def test_select_patch_interactive_accepts_reviewed_patch(self):
        patches = [self._patch('triage-fix.patch')]
        with patch.object(Apply, 'review', return_value=True) as review:
            selected = Apply.select_patch(patches, repository=self._repo(), interactive=True)
        self.assertIs(selected, patches[0])
        review.assert_called_once()

    def test_select_patch_interactive_single_rejected_returns_none(self):
        patches = [self._patch('triage-fix.patch')]
        with patch.object(Apply, 'review', return_value=False):
            self.assertIsNone(Apply.select_patch(patches, repository=self._repo(), interactive=True))

    def test_select_patch_interactive_reject_returns_to_menu(self):
        patches = [self._patch('triage-fix.patch'), self._patch('triage-revert.patch')]
        with patch('webkitscmpy.program.apply.Terminal.choose', side_effect=['triage-revert.patch', 'triage-fix.patch']) as choose, \
             patch.object(Apply, 'review', side_effect=[False, True]) as review:
            selected = Apply.select_patch(patches, repository=self._repo(), interactive=True)
        self.assertEqual(selected.name, 'triage-fix.patch')
        self.assertEqual(choose.call_count, 2)
        self.assertEqual(review.call_count, 2)

    @patch('webkitscmpy.program.apply.HTMLDiff')
    def test_review_accepts_when_diff_accepted(self, mock_htmldiff):
        mock_htmldiff.return_value.__enter__.return_value.accepted = True
        with OutputCapture():
            self.assertTrue(Apply.review(self._patch('triage-fix.patch', contents=b'PATCH'), self._repo()))
        self.assertEqual(mock_htmldiff.call_args.kwargs.get('block'), True)
        mock_htmldiff.return_value.__enter__.return_value.add_lines.assert_called_once()

    @patch('webkitscmpy.program.apply.HTMLDiff')
    def test_review_rejects_when_diff_rejected(self, mock_htmldiff):
        mock_htmldiff.return_value.__enter__.return_value.accepted = False
        with OutputCapture():
            self.assertFalse(Apply.review(self._patch('triage-fix.patch', contents=b'PATCH'), self._repo()))

    @patch('webkitscmpy.program.apply.HTMLDiff')
    def test_review_rejects_undownloadable_patch(self, mock_htmldiff):
        with OutputCapture() as captured:
            self.assertFalse(Apply.review(self._patch('triage-fix.patch', contents=None), self._repo()))
        mock_htmldiff.assert_not_called()
        self.assertIn('Could not download', captured.stderr.getvalue())

    @patch('webkitscmpy.program.apply.PullRequest.main')
    def test_offer_pull_request_invokes_pull_request_when_accepted(self, pr_main):
        repository = self._repo()
        with patch('webkitscmpy.program.apply.Terminal.choose', return_value='Yes'):
            Apply.offer_pull_request(repository)
        pr_main.assert_called_once()
        self.assertIs(pr_main.call_args.args[1], repository)
        self.assertIsNone(pr_main.call_args.args[0].issue)
        self.assertTrue(hasattr(pr_main.call_args.args[0], 'verbose'))

    @patch('webkitscmpy.program.apply.PullRequest.main')
    def test_offer_pull_request_skips_when_declined(self, pr_main):
        with patch('webkitscmpy.program.apply.Terminal.choose', return_value='No'):
            Apply.offer_pull_request(self._repo())
        pr_main.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_rejects_non_git_repository(self, mock_run):
        with OutputCapture() as captured:
            result = Apply.main(self._args(), MagicMock(spec=local.Svn))
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), "Can only 'apply' on a native Git repository\n")
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_rejects_dirty_tree(self, mock_run):
        with OutputCapture():
            result = Apply.main(self._args(), self._repo(modified={'f': 1}))
        self.assertEqual(result, 1)
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_issue_unresolvable(self, mock_run):
        with patch.object(Apply, 'resolve_issue', return_value=None), OutputCapture() as captured:
            result = Apply.main(self._args(issue='bogus'), self._repo())
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), "Could not resolve 'bogus' to an issue\n")
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_attachments_unsupported(self, mock_run):
        issue = self._issue(patches=None, tracker_name='GitHub Issue')
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(self._args(issue='https://github.example.com/x/1'), self._repo())
        self.assertEqual(result, 1)
        self.assertEqual(captured.stderr.getvalue(), 'GitHub Issue does not support patch attachments\n')
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_no_patches(self, mock_run):
        issue = self._issue(patches=[])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('No patches are attached', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_when_named_patch_missing(self, mock_run):
        issue = self._issue(patches=[self._patch('triage-fix.patch')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(self._args(name='absent.patch'), self._repo())
        self.assertEqual(result, 1)
        self.assertIn("No 'absent.patch' patch is attached", captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_named_patch_rejected_reports_not_applied(self, mock_run, mock_htmldiff):
        mock_htmldiff.return_value.__enter__.return_value.accepted = False
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), OutputCapture() as captured:
            result = Apply.main(self._args(name='triage-fix.patch'), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('No patch applied', captured.stderr.getvalue())
        self.assertNotIn('is attached', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_on_download_failure(self, mock_run):
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=None)])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('Could not download', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_errors_on_empty_content(self, mock_run):
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'   \n')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(self._args(commit=False), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('is empty', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_commits_with_git_am(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(mock_run.call_args.args[0][:2], ['git', 'am'])
        self.assertEqual(mock_run.call_args.kwargs.get('cwd'), '/repo')

    @patch('webkitscmpy.program.apply.run')
    def test_main_no_commit_uses_git_apply_3way(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(commit=False), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(mock_run.call_args.args[0][:3], ['git', 'apply', '--3way'])

    def test_parser_reset_author_defaults_to_off(self):
        parser = ArgumentParser()
        Apply.parser(parser)
        self.assertFalse(parser.parse_args(['rdar://5']).reset_author)
        self.assertTrue(parser.parse_args(['rdar://5', '--reset-author']).reset_author)

    @patch('webkitscmpy.program.apply.run')
    def test_main_rejects_reset_author_without_a_commit(self, mock_run):
        with OutputCapture() as captured:
            result = Apply.main(self._args(commit=False, reset_author=True), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('--reset-author requires a commit', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.run')
    def test_main_resets_author_after_am(self, mock_run):
        mock_run.side_effect = self._run(count='1')
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 0)
        commands = self._commands(mock_run)
        self.assertEqual(commands[0][:2], ['git', 'am'])
        self.assertEqual(commands[1], self.AMEND_RESET)
        self.assertEqual(mock_run.call_args.kwargs.get('cwd'), '/repo')

    @patch('webkitscmpy.program.apply.run')
    def test_main_replays_a_multi_commit_patch_to_reset_every_author(self, mock_run):
        # Amending only reaches the last commit `git am` created, so several are replayed instead.
        mock_run.side_effect = self._run(count='3')
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 0)
        commands = self._commands(mock_run)
        self.assertEqual(commands[0][:2], ['git', 'am'])
        self.assertEqual(commands[1], self.REPLAY_RESET)
        self.assertNotIn(self.AMEND_RESET, commands)

    @patch('webkitscmpy.program.apply.run')
    def test_main_counts_commits_from_head_before_the_patch(self, mock_run):
        mock_run.side_effect = self._run(head='ffffff', count='2')
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            Apply.main(self._args(reset_author=True), self._repo())
        commands = self._commands(mock_run, queries=True)
        self.assertEqual(commands[0], ['git', 'rev-parse', 'HEAD'])
        self.assertIn(['git', 'rev-list', '--count', 'ffffff..HEAD'], commands)
        self.assertEqual(commands[-1][-1], 'ffffff')

    @patch('webkitscmpy.program.apply.run')
    def test_main_aborts_the_replay_when_it_fails(self, mock_run):
        mock_run.side_effect = self._run(count='2', failures=('rebase',))
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 0)
        self.assertIn("couldn't reset its commits' authors", captured.stderr.getvalue())
        self.assertIn(['git', 'rebase', '--abort'], self._commands(mock_run))

    @patch('webkitscmpy.program.apply.run')
    def test_main_amends_when_the_commit_count_is_unavailable(self, mock_run):
        mock_run.side_effect = self._run(count=None)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 0)
        commands = self._commands(mock_run)
        self.assertEqual(commands[1], self.AMEND_RESET)
        self.assertEqual(len(commands), 2)

    @patch('webkitscmpy.program.apply.run')
    def test_main_amends_when_head_does_not_resolve(self, mock_run):
        # An unborn branch has no HEAD to count from, so the applied commit is amended in place.
        mock_run.side_effect = self._run(head=None)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 0)
        commands = self._commands(mock_run, queries=True)
        self.assertEqual(commands[-1], self.AMEND_RESET)
        self.assertNotIn(['git', 'rev-list', '--count', 'None..HEAD'], commands)

    @patch('webkitscmpy.program.apply.run')
    def test_main_keeps_patch_author_by_default(self, mock_run):
        mock_run.side_effect = self._run()
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 0)
        commands = self._commands(mock_run, queries=True)
        self.assertNotIn(self.AMEND_RESET, commands)
        self.assertEqual(len(commands), 1)

    @patch('webkitscmpy.program.apply.run')
    def test_main_does_not_reset_author_when_am_fails(self, mock_run):
        mock_run.side_effect = self._run(failures=('am',))
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 1)
        commands = self._commands(mock_run)
        self.assertNotIn(self.AMEND_RESET, commands)
        self.assertNotIn(self.REPLAY_RESET, commands)

    @patch('webkitscmpy.program.apply.run')
    def test_main_warns_when_author_reset_fails(self, mock_run):
        mock_run.side_effect = self._run(failures=('commit',))
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture() as captured:
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 0)
        self.assertIn("couldn't reset the commit's author", captured.stderr.getvalue())

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_resets_author_before_editing_the_message(self, mock_run, mock_htmldiff):
        mock_run.side_effect = self._run(count='1')
        mock_htmldiff.return_value.__enter__.return_value.accepted = True
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), \
             patch('webkitscmpy.program.apply.Terminal.choose', return_value='No'), OutputCapture():
            result = Apply.main(self._args(reset_author=True), self._repo())
        self.assertEqual(result, 0)
        commands = self._commands(mock_run)
        self.assertEqual(commands[0][:2], ['git', 'am'])
        self.assertEqual(commands[1], self.AMEND_RESET)
        self.assertEqual(commands[2], ['git', 'commit', '--amend'])

    @patch('webkitscmpy.program.apply.run')
    def test_main_aborts_am_on_conflict(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 1)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertEqual(commands[0][:2], ['git', 'am'])
        self.assertIn(['git', 'am', '--abort'], commands)

    @patch('webkitscmpy.program.apply.run')
    def test_main_no_commit_failure_does_not_abort(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(commit=False), self._repo())
        self.assertEqual(result, 1)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertNotIn(['git', 'am', '--abort'], commands)
        self.assertEqual(len(commands), 1)

    @patch('webkitscmpy.program.apply.run')
    def test_main_removes_tempfile_even_on_failure(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            Apply.main(self._args(), self._repo())
        patch_path = mock_run.call_args_list[0].args[0][2]
        self.assertFalse(os.path.exists(patch_path))

    @patch('webkitscmpy.program.apply.run')
    def test_main_encodes_str_content(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents='PATCH-AS-STR')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), OutputCapture():
            result = Apply.main(self._args(commit=False), self._repo())
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
            result = Apply.main(self._args(issue='rdar://1', name='triage-fix.patch'), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(written.get('content'), b'FIX BYTES')

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_reviews_and_opens_editor_when_interactive(self, mock_run, mock_htmldiff):
        mock_run.return_value = MagicMock(returncode=0)
        mock_htmldiff.return_value.__enter__.return_value.accepted = True
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), \
             patch('webkitscmpy.program.apply.Terminal.choose', return_value='No'), OutputCapture():
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 0)
        self.assertEqual(mock_htmldiff.call_args.kwargs.get('block'), True)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertEqual(commands[0][:2], ['git', 'am'])
        self.assertEqual(commands[-1], ['git', 'commit', '--amend'])

    @patch('webkitscmpy.program.apply.PullRequest.main')
    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_creates_pull_request_when_accepted(self, mock_run, mock_htmldiff, pr_main):
        mock_run.return_value = MagicMock(returncode=0)
        mock_htmldiff.return_value.__enter__.return_value.accepted = True
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), \
             patch('webkitscmpy.program.apply.Terminal.choose', return_value='Yes'), OutputCapture():
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 0)
        pr_main.assert_called_once()

    @patch('webkitscmpy.program.apply.PullRequest.main')
    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_skips_pull_request_when_declined(self, mock_run, mock_htmldiff, pr_main):
        mock_run.return_value = MagicMock(returncode=0)
        mock_htmldiff.return_value.__enter__.return_value.accepted = True
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), \
             patch('webkitscmpy.program.apply.Terminal.choose', return_value='No'), OutputCapture():
            Apply.main(self._args(), self._repo())
        pr_main.assert_not_called()

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_no_editor_when_not_committing(self, mock_run, mock_htmldiff):
        mock_run.return_value = MagicMock(returncode=0)
        mock_htmldiff.return_value.__enter__.return_value.accepted = True
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), OutputCapture():
            result = Apply.main(self._args(commit=False), self._repo())
        self.assertEqual(result, 0)
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertEqual(commands[0][:3], ['git', 'apply', '--3way'])
        self.assertNotIn(['git', 'commit', '--amend', '--date=now'], commands)

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_declined_review_is_not_applied(self, mock_run, mock_htmldiff):
        mock_htmldiff.return_value.__enter__.return_value.accepted = False
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=True), OutputCapture() as captured:
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 1)
        self.assertIn('No patch applied', captured.stderr.getvalue())
        mock_run.assert_not_called()

    @patch('webkitscmpy.program.apply.HTMLDiff')
    @patch('webkitscmpy.program.apply.run')
    def test_main_skips_review_and_editor_when_not_interactive(self, mock_run, mock_htmldiff):
        mock_run.return_value = MagicMock(returncode=0)
        issue = self._issue(patches=[self._patch('triage-fix.patch', contents=b'PATCH')])
        with patch.object(Apply, 'resolve_issue', return_value=issue), \
             patch('webkitscmpy.program.apply.Terminal.isatty', return_value=False), OutputCapture():
            result = Apply.main(self._args(), self._repo())
        self.assertEqual(result, 0)
        mock_htmldiff.assert_not_called()
        commands = [call.args[0] for call in mock_run.call_args_list]
        self.assertEqual(len(commands), 1)
        self.assertEqual(commands[0][:2], ['git', 'am'])


if __name__ == '__main__':
    unittest.main()
