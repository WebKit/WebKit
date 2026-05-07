# Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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

from __future__ import unicode_literals

import datetime
import json
import os

from unittest.mock import patch

from django.test import TestCase
from django.utils import timezone as django_timezone

from ews.common.buildbot import Buildbot
from ews.common.github import GitHubEWS
from ews.views.bubble.compute import compute_bubble
from ews.views.bubble.config import BUILDER_ICON, TESTER_ICON, BubbleConfig
from ews.views.bubble.model import (
    BubbleData,
    BubbleInputs,
    BuildSnapshot,
    ChangeSnapshot,
    StepSnapshot,
)
from ews.views.bubble.render_html import render_html_bubble
from ews.views.bubble.render_markdown import GithubIcons, render_markdown_bubble


class BuildbotAllQueuesTest(TestCase):
    def setUp(self):
        self._prev_all_queues = Buildbot.all_queues
        self._prev_icons = dict(Buildbot.icons_for_queues_mapping)
        self._prev_names = dict(Buildbot.queue_name_by_shortname_mapping)
        self._prev_pr_column = dict(Buildbot.pr_column_by_shortname)
        self._prev_misc_handlers = list(Buildbot.pr_comment_misc_handlers)

    def tearDown(self):
        Buildbot.all_queues = self._prev_all_queues
        Buildbot.icons_for_queues_mapping = self._prev_icons
        Buildbot.queue_name_by_shortname_mapping = self._prev_names
        Buildbot.pr_column_by_shortname = self._prev_pr_column
        Buildbot.pr_comment_misc_handlers = self._prev_misc_handlers

    def test_all_queues_populated_from_status_bubble_order(self):
        canned = {
            'builders': [
                {'name': 'Style-EWS', 'shortname': 'style', 'icon': 'testOnly'},
                {'name': 'iOS-Build-EWS', 'shortname': 'ios', 'icon': 'buildOnly'},
                {'name': 'Merge-Queue', 'shortname': 'merge', 'icon': 'buildAndTest'},
            ],
            'status_bubble_order': ['style', 'ios'],
        }
        Buildbot.all_queues = []
        with patch.object(Buildbot, 'fetch_config', classmethod(lambda cls: canned)):
            Buildbot.update_icons_for_queues_mapping()
        self.assertEqual(Buildbot.all_queues, ['style', 'ios'])

    def test_all_queues_preserved_when_key_missing(self):
        Buildbot.all_queues = ['style', 'ios']
        canned = {'builders': []}
        with patch.object(Buildbot, 'fetch_config', classmethod(lambda cls: canned)):
            Buildbot.update_icons_for_queues_mapping()
        self.assertEqual(Buildbot.all_queues, ['style', 'ios'])

    def test_all_queues_preserved_when_fetch_fails(self):
        Buildbot.all_queues = ['style', 'ios']
        with patch.object(Buildbot, 'fetch_config', classmethod(lambda cls: {})):
            Buildbot.update_icons_for_queues_mapping()
        self.assertEqual(Buildbot.all_queues, ['style', 'ios'])

    def test_pr_column_by_shortname_populated(self):
        canned = {
            'builders': [
                {'name': 'Style-EWS', 'shortname': 'style', 'pr_column': 'misc'},
                {'name': 'iOS-Build-EWS', 'shortname': 'ios', 'pr_column': 'embedded'},
            ],
        }
        Buildbot.pr_column_by_shortname = {}
        with patch.object(Buildbot, 'fetch_config', classmethod(lambda cls: canned)):
            Buildbot.update_icons_for_queues_mapping()
        self.assertEqual(Buildbot.pr_column_by_shortname, {'style': 'misc', 'ios': 'embedded'})

    def test_pr_comment_misc_handlers_populated(self):
        canned = {
            'builders': [],
            'pr_comment_misc_handlers': ['merge', 'unsafe-merge'],
        }
        Buildbot.pr_comment_misc_handlers = []
        with patch.object(Buildbot, 'fetch_config', classmethod(lambda cls: canned)):
            Buildbot.update_icons_for_queues_mapping()
        self.assertEqual(Buildbot.pr_comment_misc_handlers, ['merge', 'unsafe-merge'])

    def test_pr_comment_misc_handlers_preserved_when_key_missing(self):
        Buildbot.pr_comment_misc_handlers = ['merge', 'unsafe-merge']
        with patch.object(Buildbot, 'fetch_config', classmethod(lambda cls: {'builders': []})):
            Buildbot.update_icons_for_queues_mapping()
        self.assertEqual(Buildbot.pr_comment_misc_handlers, ['merge', 'unsafe-merge'])


class GitHubEWSStatusBubbleRowsTest(TestCase):
    def setUp(self):
        self._prev_all_queues = Buildbot.all_queues
        self._prev_pr_column = dict(Buildbot.pr_column_by_shortname)
        self._prev_misc_handlers = list(Buildbot.pr_comment_misc_handlers)

    def tearDown(self):
        Buildbot.all_queues = self._prev_all_queues
        Buildbot.pr_column_by_shortname = self._prev_pr_column
        Buildbot.pr_comment_misc_handlers = self._prev_misc_handlers

    def test_grid_layout_with_canned_data(self):
        Buildbot.all_queues = ['style', 'ios', 'ios-sim', 'mac', 'wpe', 'win', 'win-tests']
        Buildbot.pr_column_by_shortname = {
            'style': 'misc', 'ios': 'embedded', 'ios-sim': 'embedded',
            'mac': 'macos', 'wpe': 'linux', 'win': 'windows', 'win-tests': 'windows',
        }
        Buildbot.pr_comment_misc_handlers = ['merge', 'unsafe-merge']
        rows = GitHubEWS._compute_status_bubble_rows()
        # Each row has 5 columns; height = max column length = misc(3)
        self.assertEqual(len(rows), 3)
        for row in rows:
            self.assertEqual(len(row), 5)
        # Column display order is [misc, embedded, macos, linux, windows].
        misc_col = [row[0] for row in rows]
        self.assertEqual(misc_col, ['merge', 'unsafe-merge', 'style'])
        embedded_col = [row[1] for row in rows]
        self.assertEqual(embedded_col, ['ios', 'ios-sim', ''])
        macos_col = [row[2] for row in rows]
        self.assertEqual(macos_col, ['mac', '', ''])
        linux_col = [row[3] for row in rows]
        self.assertEqual(linux_col, ['wpe', '', ''])
        windows_col = [row[4] for row in rows]
        self.assertEqual(windows_col, ['win', 'win-tests', ''])

    def test_grid_respects_status_bubble_order(self):
        Buildbot.all_queues = ['ios-sim', 'ios']  # reversed ordering
        Buildbot.pr_column_by_shortname = {'ios': 'embedded', 'ios-sim': 'embedded'}
        Buildbot.pr_comment_misc_handlers = []
        rows = GitHubEWS._compute_status_bubble_rows()
        embedded_col = [row[1] for row in rows]
        self.assertEqual(embedded_col, ['ios-sim', 'ios'])

    def test_grid_skips_shortnames_without_pr_column(self):
        Buildbot.all_queues = ['mac', 'orphan']
        Buildbot.pr_column_by_shortname = {'mac': 'macos'}
        Buildbot.pr_comment_misc_handlers = []
        rows = GitHubEWS._compute_status_bubble_rows()
        flat = [cell for row in rows for cell in row]
        self.assertNotIn('orphan', flat)
        self.assertIn('mac', flat)


class GitHubEWSLegacyDriftGuardTest(TestCase):
    # Drift guard: the computed grid should partition the real config.json into
    # the same column membership as the pre-refactor STATUS_BUBBLE_ROWS hardcoded
    # grid (with `merge` and `unsafe-merge` moved to the top of Misc). Remove
    # once the derived values are trusted.
    LEGACY_COLUMN_MEMBERSHIP = {
        'misc':     {'merge', 'unsafe-merge', 'style', 'bindings', 'webkitperl', 'webkitpy', 'jsc-x86-64', 'jsc-debug-arm64', 'services'},
        'embedded': {'ios', 'ios-sim', 'ios-wk2', 'ios-wk2-wpt', 'api-ios', 'ios-safer-cpp', 'vision', 'vision-sim', 'vision-wk2', 'tv', 'tv-sim', 'watch', 'watch-sim'},
        'macos':    {'mac', 'mac-AS-debug', 'api-mac', 'api-mac-debug', 'mac-wk1', 'mac-wk2', 'mac-AS-debug-wk2', 'mac-wk2-stress', 'mac-intel-wk2', 'mac-safer-cpp'},
        'linux':    {'wpe', 'wpe-wk2', 'api-wpe', 'gtk3-libwebrtc', 'gtk', 'gtk-wk2', 'api-gtk', 'playstation', 'jsc-armv7', 'jsc-armv7-tests'},
        'windows':  {'win', 'win-tests'},
    }

    def setUp(self):
        self._prev_all_queues = Buildbot.all_queues
        self._prev_pr_column = dict(Buildbot.pr_column_by_shortname)
        self._prev_misc_handlers = list(Buildbot.pr_comment_misc_handlers)
        cwd = os.path.dirname(os.path.abspath(__file__))
        config_path = os.path.join(cwd, '..', '..', 'ews-build', 'config.json')
        with open(config_path) as f:
            config = json.load(f)
        Buildbot.all_queues = list(config.get('status_bubble_order', []))
        Buildbot.pr_column_by_shortname = {
            b['shortname']: b['pr_column']
            for b in config.get('builders', [])
            if b.get('pr_column')
        }
        Buildbot.pr_comment_misc_handlers = list(config.get('pr_comment_misc_handlers', []))

    def tearDown(self):
        Buildbot.all_queues = self._prev_all_queues
        Buildbot.pr_column_by_shortname = self._prev_pr_column
        Buildbot.pr_comment_misc_handlers = self._prev_misc_handlers

    def test_column_membership_matches_legacy(self):
        rows = GitHubEWS._compute_status_bubble_rows()
        columns = {key: set() for key in GitHubEWS.PR_COLUMN_DISPLAY_ORDER}
        for row in rows:
            for i, key in enumerate(GitHubEWS.PR_COLUMN_DISPLAY_ORDER):
                if row[i]:
                    columns[key].add(row[i])
        self.assertEqual(columns, self.LEGACY_COLUMN_MEMBERSHIP)

    def test_misc_column_starts_with_merge_handlers(self):
        rows = GitHubEWS._compute_status_bubble_rows()
        misc_col = [row[0] for row in rows if row[0]]
        self.assertEqual(misc_col[:2], ['merge', 'unsafe-merge'])


# ---------------------------------------------------------------------------
# Status bubble compute / render tests
# ---------------------------------------------------------------------------

NOW = datetime.datetime(2026, 5, 5, 12, 0, tzinfo=datetime.timezone.utc)


class _Fixtures(object):
    @staticmethod
    def cfg(**overrides):
        defaults = dict(
            icons_for_queues={'ios-sim': 'buildAndTest', 'style': 'testOnly',
                              'mac': 'buildOnly', 'misc-q': '', 'wpe': 'buildOnly'},
            queue_name_by_shortname={'ios-sim': 'iOS-Simulator-EWS', 'mac': 'macOS-EWS',
                                     'style': 'Style-EWS', 'wpe': 'WPE-EWS'},
            all_queues=('style', 'ios-sim', 'mac', 'wpe'),
            pr_column_by_shortname={},
            pr_comment_misc_handlers=('merge', 'unsafe-merge'),
            queue_triggers={'ios-wk2': 'ios-sim'},
            buildbot_server_host='ews-build.test',
        )
        defaults.update(overrides)
        return BubbleConfig(**defaults)

    @staticmethod
    def step(uid='1', state='Passed tests', result=Buildbot.SUCCESS, started_at=1700000000):
        return StepSnapshot(uid=uid, state_string=state, result=result, started_at=started_at)

    @staticmethod
    def build(*, result=Buildbot.SUCCESS, steps=(), builder_display_name='ios-sim',
              builder_name='iOS-Simulator-EWS', builder_id=42, number=7,
              state_string='', started_at=1700000000, complete_at=1700000060):
        asc = tuple(steps)
        desc = tuple(reversed(asc))
        return BuildSnapshot(
            builder_id=builder_id,
            builder_name=builder_name,
            builder_display_name=builder_display_name,
            number=number,
            result=result,
            state_string=state_string,
            started_at=started_at,
            complete_at=complete_at,
            steps_by_uid_asc=asc,
            steps_by_uid_desc=desc,
            last_step=asc[-1] if asc else None,
        )

    @staticmethod
    def change(*, created=None, sent_to_buildbot=True, sent_to_commit_queue=False, obsolete=False):
        return ChangeSnapshot(
            change_id='abc123',
            created=created or (NOW - datetime.timedelta(hours=1)),
            sent_to_buildbot=sent_to_buildbot,
            sent_to_commit_queue=sent_to_commit_queue,
            obsolete=obsolete,
            pr_number=999,
            pr_project='WebKit/WebKit',
            comment_id=-1,
        )

    @staticmethod
    def inputs(*, builds=(), is_parent_build=False, queue='ios-sim',
               queue_position=None, hide_icons=False, sent_to_buildbot=True,
               change=None):
        return BubbleInputs(
            change=change or _Fixtures.change(),
            queue=queue,
            builds=tuple(builds),
            is_parent_build=is_parent_build,
            queue_position=queue_position,
            hide_icons=hide_icons,
            sent_to_buildbot=sent_to_buildbot,
        )


class ComputeBubbleBuildOutcomesTest(TestCase):
    def test_in_progress_no_failed_step_yields_started(self):
        build = _Fixtures.build(result=None, steps=(_Fixtures.step(state='Compiling'),))
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'started')
        self.assertIn('Build is in-progress', data.details_message)

    def test_in_progress_with_failed_step_yields_provisional_fail(self):
        steps = (_Fixtures.step(uid='1', state='ok', result=Buildbot.SUCCESS),
                 _Fixtures.step(uid='2', state='boom', result=Buildbot.FAILURE))
        build = _Fixtures.build(result=None, steps=steps)
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'provisional-fail')

    def test_success_non_parent_builder_only_says_built_successfully(self):
        cfg = _Fixtures.cfg(icons_for_queues={'mac': 'buildOnly'})
        build = _Fixtures.build(result=Buildbot.SUCCESS, builder_display_name='mac')
        data = compute_bubble(_Fixtures.inputs(builds=[build], queue='mac'), cfg, now=NOW)
        self.assertEqual(data.state, 'pass')
        self.assertEqual(data.details_message, 'Built successfully')

    def test_success_non_parent_tester_only_says_passed_tests(self):
        cfg = _Fixtures.cfg(icons_for_queues={'ios-wk2': 'testOnly'})
        build = _Fixtures.build(result=Buildbot.SUCCESS, builder_display_name='ios-wk2')
        data = compute_bubble(_Fixtures.inputs(builds=[build], queue='ios-wk2'), cfg, now=NOW)
        self.assertEqual(data.details_message, 'Passed tests')

    def test_success_non_parent_style_queue_says_passed_style_check(self):
        cfg = _Fixtures.cfg(icons_for_queues={'style': 'testOnly'})
        build = _Fixtures.build(result=Buildbot.SUCCESS, builder_display_name='style')
        data = compute_bubble(_Fixtures.inputs(builds=[build], queue='style'), cfg, now=NOW)
        self.assertEqual(data.details_message, 'Passed style check')

    def test_success_non_parent_build_and_test_says_built_and_passed(self):
        cfg = _Fixtures.cfg(icons_for_queues={'ios-sim': 'buildAndTest'})
        build = _Fixtures.build(result=Buildbot.SUCCESS)
        data = compute_bubble(_Fixtures.inputs(builds=[build], queue='ios-sim'), cfg, now=NOW)
        self.assertEqual(data.details_message, 'Built successfully and passed tests')

    def test_success_non_parent_with_no_icon_says_pass(self):
        cfg = _Fixtures.cfg(icons_for_queues={})
        build = _Fixtures.build(result=Buildbot.SUCCESS, builder_display_name='unknown')
        data = compute_bubble(_Fixtures.inputs(builds=[build], queue='unknown'), cfg, now=NOW)
        self.assertEqual(data.details_message, 'Pass')

    def test_success_parent_build_within_window_yields_started_waiting(self):
        cfg = _Fixtures.cfg()
        change = _Fixtures.change(created=NOW - datetime.timedelta(days=1))
        build = _Fixtures.build(result=Buildbot.SUCCESS)
        data = compute_bubble(
            _Fixtures.inputs(builds=[build], is_parent_build=True, change=change),
            cfg, now=NOW,
        )
        self.assertEqual(data.state, 'started')
        self.assertEqual(data.details_message, 'Waiting to run tests.')
        self.assertIn('iOS-Simulator-EWS', data.url)

    def test_success_parent_build_outside_hide_window_returns_none(self):
        cfg = _Fixtures.cfg()
        old = NOW - datetime.timedelta(days=cfg.days_to_hide_bubble + 1)
        change = _Fixtures.change(created=old)
        build = _Fixtures.build(result=Buildbot.SUCCESS)
        data = compute_bubble(
            _Fixtures.inputs(builds=[build], is_parent_build=True, change=change),
            cfg, now=NOW,
        )
        self.assertIsNone(data)

    def test_warnings_yields_pass_state_with_warning_message(self):
        build = _Fixtures.build(result=Buildbot.WARNINGS,
                                steps=(_Fixtures.step(state='warn step'),))
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'pass')
        self.assertTrue(data.details_message.startswith('Warning'))

    def test_failure_yields_fail_state_with_recent_failure_message(self):
        steps = (_Fixtures.step(uid='1', state='compile failed', result=Buildbot.FAILURE),)
        build = _Fixtures.build(result=Buildbot.FAILURE, steps=steps)
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'fail')
        self.assertEqual(data.details_message, 'compile failed')

    def test_failure_with_retry_message_yields_provisional_fail(self):
        steps = (_Fixtures.step(uid='1', state='retrying build now',
                                result=Buildbot.SUCCESS),)
        build = _Fixtures.build(result=Buildbot.FAILURE, steps=steps)
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'provisional-fail')

    def test_skipped_bug_already_closed_appends_message(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string='Bug 12345 is already closed')
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'skipped')
        self.assertIn('Bug was already closed', data.details_message)

    def test_skipped_patch_marked_r_minus_appends_message(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string='Patch 555 is marked r-')
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertIn('marked r-', data.details_message)

    def test_skipped_patch_obsolete_appends_message(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string='Patch 555 is obsolete')
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertIn('Patch was obsolete', data.details_message)

    def test_skipped_pull_request_already_closed_appends_message(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string='Pull request 99 is already closed')
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertIn('Pull Request was already closed', data.details_message)

    def test_skipped_hash_outdated_appends_message(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string='Hash abc on PR 99 is outdated')
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertIn('Commit was outdated', data.details_message)

    def test_skipped_skip_ews_label_overrides_message(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string='Skipping as PR 99 has skip-ews label')
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(
            data.details_message,
            'EWS skipped this build as PR had skip-ews label when EWS attempted to process it.',
        )

    def test_skipped_with_extra_eligible_builds_appends_step_log(self):
        old_build = _Fixtures.build(result=Buildbot.FAILURE, number=6,
                                    steps=(_Fixtures.step(state='earlier failure',
                                                          result=Buildbot.FAILURE),))
        skipped = _Fixtures.build(result=Buildbot.SKIPPED, number=7,
                                  state_string='Bug 1 is already closed')
        data = compute_bubble(_Fixtures.inputs(builds=[skipped, old_build]),
                              _Fixtures.cfg(), now=NOW)
        self.assertIn('Some messages were logged while the change was still eligible', data.details_message)
        self.assertIn('earlier failure', data.details_message)

    def test_exception_yields_error_state(self):
        build = _Fixtures.build(result=Buildbot.EXCEPTION,
                                steps=(_Fixtures.step(state='boom'),))
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'error')
        self.assertIn('unexpected error', data.details_message)

    def test_retry_yields_provisional_fail_state(self):
        build = _Fixtures.build(result=Buildbot.RETRY,
                                steps=(_Fixtures.step(state='retry step'),))
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'provisional-fail')
        self.assertIn('being retried', data.details_message)

    def test_cancelled_yields_cancelled_state(self):
        build = _Fixtures.build(result=Buildbot.CANCELLED,
                                steps=(_Fixtures.step(state='cancel step'),))
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'cancelled')
        self.assertIn('cancelled', data.details_message)

    def test_unknown_result_code_yields_error_state(self):
        build = _Fixtures.build(result=99,
                                steps=(_Fixtures.step(state='who knows'),))
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'error')

    def test_builder_full_name_and_timestamp_present(self):
        build = _Fixtures.build(result=Buildbot.SUCCESS, complete_at=1700000123)
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.builder_full_name, 'iOS Simulator EWS')
        self.assertIsNotNone(data.build_timestamp_iso)
        self.assertTrue(data.build_timestamp_iso.startswith('[['))

    def test_os_details_present_when_step_starts_with_os_prefix(self):
        steps = (_Fixtures.step(state='OS: macOS 14.0', result=Buildbot.SUCCESS),)
        build = _Fixtures.build(result=Buildbot.SUCCESS, steps=steps)
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.os_details, 'OS: macOS 14.0')


class ComputeBubbleQueuePositionTest(TestCase):
    def test_no_build_with_integer_queue_position_yields_none_state_with_position(self):
        data = compute_bubble(_Fixtures.inputs(queue_position=3),
                              _Fixtures.cfg(), now=NOW)
        self.assertEqual(data.state, 'none')
        self.assertEqual(data.queue_position, 3)
        self.assertIn('Position in queue: 3', data.details_message)

    def test_no_build_with_unknown_queue_position_omits_position_field(self):
        cfg = _Fixtures.cfg()
        data = compute_bubble(_Fixtures.inputs(queue_position=cfg.unknown_queue_position),
                              cfg, now=NOW)
        self.assertEqual(data.state, 'none')
        self.assertIsNone(data.queue_position)
        self.assertIn('Position in queue: ?', data.details_message)

    def test_no_build_with_none_queue_position_returns_none(self):
        data = compute_bubble(_Fixtures.inputs(queue_position=None),
                              _Fixtures.cfg(), now=NOW)
        self.assertIsNone(data)

    def test_no_build_falls_back_to_parent_queue_full_name_for_url(self):
        cfg = _Fixtures.cfg(
            queue_triggers={'ios-wk2': 'ios-sim'},
            queue_name_by_shortname={'ios-sim': 'iOS-Simulator-EWS'},
        )
        data = compute_bubble(_Fixtures.inputs(queue='ios-wk2', queue_position=2),
                              cfg, now=NOW)
        self.assertIn('iOS-Simulator-EWS', data.url)

    def test_no_build_unknown_queue_full_name_omits_url(self):
        cfg = _Fixtures.cfg(queue_name_by_shortname={})
        data = compute_bubble(_Fixtures.inputs(queue='nope', queue_position=1),
                              cfg, now=NOW)
        self.assertIsNone(data.url)


class ComputeBubbleVisibilityTest(TestCase):
    def test_no_build_and_not_sent_to_buildbot_hides_bubble(self):
        data = compute_bubble(_Fixtures.inputs(sent_to_buildbot=False, queue_position=1),
                              _Fixtures.cfg(), now=NOW)
        self.assertIsNone(data)

    def test_no_build_and_sent_to_buildbot_shows_bubble(self):
        data = compute_bubble(_Fixtures.inputs(sent_to_buildbot=True, queue_position=1),
                              _Fixtures.cfg(), now=NOW)
        self.assertIsNotNone(data)

    def test_skipped_irrelevant_changes_patch_phrasing_hides_bubble(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string="Patch 1 doesn't have relevant changes")
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertIsNone(data)

    def test_skipped_irrelevant_changes_pr_phrasing_hides_bubble(self):
        build = _Fixtures.build(result=Buildbot.SKIPPED,
                                state_string="Pull request 1 doesn't have relevant changes")
        data = compute_bubble(_Fixtures.inputs(builds=[build]), _Fixtures.cfg(), now=NOW)
        self.assertIsNone(data)

    def test_hide_icons_true_omits_builder_and_tester_icons(self):
        build = _Fixtures.build(result=Buildbot.SUCCESS)
        data = compute_bubble(
            _Fixtures.inputs(builds=[build], hide_icons=True), _Fixtures.cfg(), now=NOW,
        )
        self.assertEqual(data.name, 'ios-sim')
        self.assertNotIn(BUILDER_ICON, data.name)
        self.assertNotIn(TESTER_ICON, data.name)

    def test_hide_icons_false_prepends_tester_icon_for_tester_queue(self):
        cfg = _Fixtures.cfg(icons_for_queues={'style': 'testOnly'})
        build = _Fixtures.build(result=Buildbot.SUCCESS, builder_display_name='style')
        data = compute_bubble(_Fixtures.inputs(builds=[build], queue='style'), cfg, now=NOW)
        self.assertTrue(data.name.startswith(TESTER_ICON))

    def test_hide_icons_false_prepends_builder_icon_for_builder_queue(self):
        cfg = _Fixtures.cfg(icons_for_queues={'mac': 'buildOnly'})
        build = _Fixtures.build(result=Buildbot.SUCCESS, builder_display_name='mac')
        data = compute_bubble(_Fixtures.inputs(builds=[build], queue='mac'), cfg, now=NOW)
        self.assertTrue(data.name.startswith(BUILDER_ICON))

    def test_hide_icons_false_prepends_both_icons_for_build_and_test_queue(self):
        cfg = _Fixtures.cfg(icons_for_queues={'ios-sim': 'buildAndTest'})
        build = _Fixtures.build(result=Buildbot.SUCCESS)
        data = compute_bubble(_Fixtures.inputs(builds=[build]), cfg, now=NOW)
        self.assertIn(BUILDER_ICON, data.name)
        self.assertIn(TESTER_ICON, data.name)


class RenderHtmlBubbleTest(TestCase):
    def test_render_html_bubble_returns_template_keys(self):
        data = BubbleData(name='ios', state='pass', url='http://x',
                          details_message='Built successfully',
                          builder_full_name='iOS Simulator EWS',
                          os_details='OS: macOS', build_timestamp_iso='[[t]]',
                          queue_position=None)
        out = render_html_bubble(data)
        self.assertEqual(out['name'], 'ios')
        self.assertEqual(out['state'], 'pass')
        self.assertEqual(out['url'], 'http://x')
        self.assertIn('iOS Simulator EWS', out['details_message'])
        self.assertIn('Built successfully', out['details_message'])
        self.assertIn('OS: macOS', out['details_message'])
        self.assertIn('[[t]]', out['details_message'])

    def test_render_html_bubble_omits_queue_position_when_none(self):
        data = BubbleData(name='ios', state='pass', url=None,
                          details_message='ok', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None,
                          queue_position=None)
        out = render_html_bubble(data)
        self.assertNotIn('queue_position', out)

    def test_render_html_bubble_includes_queue_position_when_set(self):
        data = BubbleData(name='ios', state='none', url=None,
                          details_message='waiting', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None,
                          queue_position=4)
        out = render_html_bubble(data)
        self.assertEqual(out['queue_position'], 4)

    def test_render_html_bubble_appends_os_and_timestamp_with_single_newline_between(self):
        data = BubbleData(name='ios', state='pass', url=None,
                          details_message='body', builder_full_name='Builder',
                          os_details='OS: x', build_timestamp_iso='[[t]]',
                          queue_position=None)
        out = render_html_bubble(data)
        self.assertEqual(out['details_message'], 'Builder\n\nbody\n\nOS: x\n[[t]]')

    def test_render_html_bubble_omits_url_when_none(self):
        data = BubbleData(name='ios', state='pass', url=None,
                          details_message='ok', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None,
                          queue_position=None)
        out = render_html_bubble(data)
        self.assertNotIn('url', out)


_GH_ICONS = GithubIcons(
    pass_='[PASS]', fail='[FAIL]', waiting='[WAIT]', ongoing='[ONGOING]',
    ongoing_with_failures='[ONGOING-FAIL]', error='[ERROR]', empty='[EMPTY]',
)


class RenderMarkdownBubbleTest(TestCase):
    def test_pass_state_emits_pass_icon_and_url(self):
        data = BubbleData(name='ios', state='pass', url='http://x',
                          details_message='ok', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('[PASS]', out)
        self.assertIn('http://x', out)

    def test_fail_state_emits_fail_icon(self):
        data = BubbleData(name='ios', state='fail', url='http://x',
                          details_message='broken', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('[FAIL]', out)

    def test_provisional_fail_in_progress_uses_ongoing_with_failures_icon(self):
        data = BubbleData(name='ios', state='provisional-fail', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=True,
                                     escape=lambda s: s)
        self.assertIn('[ONGOING-FAIL]', out)

    def test_provisional_fail_not_in_progress_uses_ongoing_icon(self):
        data = BubbleData(name='ios', state='provisional-fail', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('[ONGOING]', out)

    def test_cancelled_strikes_through_name(self):
        data = BubbleData(name='ios', state='cancelled', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('~~ios~~', out)
        self.assertIn('[EMPTY]', out)

    def test_skipped_strikes_through_name(self):
        data = BubbleData(name='ios', state='skipped', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('~~ios~~', out)

    def test_hover_text_is_escaped(self):
        data = BubbleData(name='ios', state='pass', url='http://x',
                          details_message='one|two\nthree', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        recorded = []
        def spy(s):
            recorded.append(s)
            return s.replace('|', '\\|').replace('\n', ' ')
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False, escape=spy)
        self.assertEqual(recorded, ['one|two\nthree'])
        self.assertIn('one\\|two three', out)

    def test_started_state_uses_ongoing_icon(self):
        data = BubbleData(name='ios', state='started', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=True,
                                     escape=lambda s: s)
        self.assertIn('[ONGOING]', out)

    def test_error_state_uses_error_icon(self):
        data = BubbleData(name='ios', state='error', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('[ERROR]', out)

    def test_none_state_uses_waiting_icon(self):
        data = BubbleData(name='ios', state='none', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('[WAIT]', out)

    def test_unknown_state_falls_back_to_error_icon(self):
        data = BubbleData(name='ios', state='totally-unknown', url='http://x',
                          details_message='', builder_full_name=None,
                          os_details=None, build_timestamp_iso=None, queue_position=None)
        out = render_markdown_bubble(data, icons=_GH_ICONS, is_in_progress=False,
                                     escape=lambda s: s)
        self.assertIn('[ERROR]', out)


# ---------------------------------------------------------------------------
# Status bubble end-to-end and DB-backed tests
# ---------------------------------------------------------------------------

class StatusBubbleTemplateRenderTest(TestCase):
    """Renders a real Change through StatusBubble.get() and asserts on HTML."""

    def setUp(self):
        from ews.models.patch import Change
        from ews.models.build import Build
        Build.objects.all().delete()
        Change.objects.all().delete()
        self.change = Change.objects.create(
            change_id='abc123', bug_id=1,
            sent_to_buildbot=True, sent_to_commit_queue=False, obsolete=False,
        )
        Build.objects.create(
            change=self.change, uid='b1', builder_id=42, builder_name='iOS-Simulator-EWS',
            builder_display_name='ios-sim', number=7,
            result=Buildbot.SUCCESS, state_string='', started_at=1700000000,
            complete_at=1700000060,
        )
        self._test_cfg = BubbleConfig(
            icons_for_queues={'ios-sim': 'buildAndTest'},
            queue_name_by_shortname={'ios-sim': 'iOS-Simulator-EWS'},
            all_queues=('ios-sim',),
            pr_column_by_shortname={},
            pr_comment_misc_handlers=(),
            queue_triggers={},
            buildbot_server_host='ews-build.test',
        )
        self._cfg_patch = patch.object(
            BubbleConfig, 'from_buildbot_globals',
            classmethod(lambda cls: self._test_cfg),
        )
        self._cfg_patch.start()
        self._now_patch = patch.object(
            django_timezone, 'now',
            lambda: NOW,
        )
        self._now_patch.start()

    def tearDown(self):
        self._cfg_patch.stop()
        self._now_patch.stop()

    def _get(self):
        return self.client.get('/status-bubble/abc123/').content.decode('utf-8')

    def test_get_returns_anchor_with_status_pass_class(self):
        self.assertIn('class="status pass"', self._get())

    def test_get_renders_href_to_buildbot_build_page(self):
        self.assertIn('href="https://ews-build.test/#/builders/42/builds/7"', self._get())

    def test_get_renders_title_attribute_with_details_message(self):
        body = self._get()
        self.assertIn('title="', body)
        self.assertIn('Built successfully and passed tests', body)

    def test_get_omits_submit_form_when_sent_to_buildbot(self):
        self.assertNotIn('Submit for EWS analysis', self._get())

    def test_get_renders_submit_form_when_not_sent_to_buildbot(self):
        self.change.sent_to_buildbot = False
        self.change.save()
        self.assertIn('Submit for EWS analysis', self._get())

    def test_get_renders_none_state_with_queue_position_when_no_build(self):
        from ews.models.build import Build
        Build.objects.all().delete()
        body = self._get()
        self.assertIn('class="status none"', body)
        self.assertIn('<span class="queue_position">#1</span>', body)
        self.assertIn('Position in queue: 1', body)
        self.assertIn('href="https://ews-build.test/#/builders/iOS-Simulator-EWS"', body)


class BubbleDataFetcherQueuePositionTest(TestCase):
    """DB-backed tests for the queue-position SQL in fetch_queue_position."""

    def setUp(self):
        from ews.models.patch import Change
        from ews.models.build import Build
        Build.objects.all().delete()
        Change.objects.all().delete()
        self._cfg = BubbleConfig(
            icons_for_queues={'ios-sim': 'buildAndTest'},
            queue_name_by_shortname={'ios-sim': 'iOS-Simulator-EWS'},
            all_queues=('ios-sim',),
            pr_column_by_shortname={},
            pr_comment_misc_handlers=(),
            queue_triggers={'ios-wk2': 'ios-sim'},
            buildbot_server_host='ews-build.test',
        )

    def _make_change(self, change_id, *, created, sent_to_buildbot=True, obsolete=False):
        from ews.models.patch import Change
        change = Change.objects.create(
            change_id=change_id, bug_id=0,
            sent_to_buildbot=sent_to_buildbot, sent_to_commit_queue=False,
            obsolete=obsolete,
        )
        Change.objects.filter(pk=change.pk).update(created=created)
        return Change.objects.get(pk=change.pk)

    def _make_build(self, change, *, builder_display_name, created):
        from ews.models.build import Build
        build = Build.objects.create(
            change=change, uid='u-{}-{}'.format(change.change_id, builder_display_name),
            builder_id=1, builder_name='Bldr', builder_display_name=builder_display_name,
            number=1, result=Buildbot.SUCCESS, state_string='', started_at=1, complete_at=2,
        )
        Build.objects.filter(pk=build.pk).update(created=created)
        return Build.objects.get(pk=build.pk)

    def test_position_one_when_no_prior_changes(self):
        from ews.views.bubble.fetcher import BubbleDataFetcher
        ch = self._make_change('c1', created=NOW - datetime.timedelta(hours=1))
        fetcher = BubbleDataFetcher(self._cfg, now_provider=lambda: NOW)
        self.assertEqual(fetcher.fetch_queue_position(ch, 'ios-sim', None), 1)

    def test_position_counts_only_unprocessed_prior_changes(self):
        from ews.views.bubble.fetcher import BubbleDataFetcher
        prior_processed = self._make_change('c1', created=NOW - datetime.timedelta(hours=3))
        self._make_build(prior_processed, builder_display_name='ios-sim',
                         created=NOW - datetime.timedelta(hours=2))
        self._make_change('c2', created=NOW - datetime.timedelta(hours=2))
        target = self._make_change('c3', created=NOW - datetime.timedelta(hours=1))
        fetcher = BubbleDataFetcher(self._cfg, now_provider=lambda: NOW)
        # Two prior changes, one processed, so position = 2.
        self.assertEqual(fetcher.fetch_queue_position(target, 'ios-sim', None), 2)

    def test_outside_hide_window_returns_none(self):
        from ews.views.bubble.fetcher import BubbleDataFetcher
        ch = self._make_change('c1', created=NOW - datetime.timedelta(days=10))
        fetcher = BubbleDataFetcher(self._cfg, now_provider=lambda: NOW)
        self.assertIsNone(fetcher.fetch_queue_position(ch, 'ios-sim', None))

    def test_between_check_and_hide_window_returns_unknown_position_marker(self):
        from ews.views.bubble.fetcher import BubbleDataFetcher
        ch = self._make_change('c1', created=NOW - datetime.timedelta(days=2))
        fetcher = BubbleDataFetcher(self._cfg, now_provider=lambda: NOW)
        self.assertEqual(
            fetcher.fetch_queue_position(ch, 'ios-sim', None),
            self._cfg.unknown_queue_position,
        )

    def test_parent_queue_fallback_subtracts_parent_built_changes(self):
        from ews.views.bubble.fetcher import BubbleDataFetcher
        prior = self._make_change('c1', created=NOW - datetime.timedelta(hours=3))
        self._make_build(prior, builder_display_name='ios-sim',
                         created=NOW - datetime.timedelta(hours=2))
        target = self._make_change('c2', created=NOW - datetime.timedelta(hours=1))
        fetcher = BubbleDataFetcher(self._cfg, now_provider=lambda: NOW)
        # Target is on ios-wk2 whose parent is ios-sim. Prior built on ios-sim
        # is subtracted, leaving 0 unprocessed -> position 1.
        self.assertEqual(fetcher.fetch_queue_position(target, 'ios-wk2', 'ios-sim'), 1)


class GitHubEWSMiscHandlerEmptyCellTest(TestCase):
    """Covers the `| ` empty-cell branch in github_status_for_buildbot_queue:
    when a queue listed in pr_comment_misc_handlers has no build for the change,
    the markdown comment must render an empty table cell rather than a bubble."""

    def setUp(self):
        from ews.models.patch import Change
        from ews.models.build import Build
        Build.objects.all().delete()
        Change.objects.all().delete()
        self.change = Change.objects.create(
            change_id='abc123', bug_id=1,
            sent_to_buildbot=True, sent_to_commit_queue=False, obsolete=False,
        )
        self._test_cfg = BubbleConfig(
            icons_for_queues={'merge': '', 'unsafe-merge': '', 'ios-sim': 'buildAndTest'},
            queue_name_by_shortname={'ios-sim': 'iOS-Simulator-EWS'},
            all_queues=('merge', 'unsafe-merge', 'ios-sim'),
            pr_column_by_shortname={},
            pr_comment_misc_handlers=('merge', 'unsafe-merge'),
            queue_triggers={},
            buildbot_server_host='ews-build.test',
        )
        self._cfg_patch = patch.object(
            BubbleConfig, 'from_buildbot_globals',
            classmethod(lambda cls: self._test_cfg),
        )
        self._cfg_patch.start()

    def tearDown(self):
        self._cfg_patch.stop()

    def test_merge_queue_with_no_build_returns_empty_cell(self):
        self.assertEqual(
            GitHubEWS().github_status_for_buildbot_queue(self.change, 'merge'),
            u'| ',
        )

    def test_unsafe_merge_queue_with_no_build_returns_empty_cell(self):
        self.assertEqual(
            GitHubEWS().github_status_for_buildbot_queue(self.change, 'unsafe-merge'),
            u'| ',
        )

    def test_non_misc_handler_with_no_build_renders_waiting_bubble(self):
        # Sanity: misc-handler short-circuit must not swallow regular queues.
        # ios-sim has no build but isn't a misc handler, so we should get a
        # rendered bubble cell (starts with `| [`), not the bare `| `.
        result = GitHubEWS().github_status_for_buildbot_queue(self.change, 'ios-sim')
        self.assertNotEqual(result, u'| ')
        self.assertTrue(result.startswith('| ['), 'expected bubble cell, got: {!r}'.format(result))
