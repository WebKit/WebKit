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

import json
from unittest import mock

from django.test import RequestFactory, TestCase

from ews.common.buildbot import Buildbot
from ews.common.github import GitHubEWS
from ews.config import SUCCESS
from ews.models.build import Build
from ews.models.buildbotinstance import BuildbotInstance
from ews.models.patch import Change
from ews.views.status import Status
from ews.views.statusbubble import StatusBubble
import ews.config as config

CHANGE_ID = '1234567890abcdef1234567890abcdef12345678'
PR_NUMBER = 4242
PR_PROJECT = 'WebKit/WebKit'
HOSTNAME = config.BUILDBOT_SERVER_HOST


def save_build(build_id=100, result=None, state_string='Running', started_at=1700000000, complete_at=None, builder_id=10, number=5):
    return Build.save_build(change_id=CHANGE_ID, hostname=HOSTNAME, build_id=build_id, builder_id=builder_id, builder_name='macOS-Sequoia-Release-WK2-Tests-EWS',
                            builder_display_name='mac-wk2', number=number, result=result, state_string=state_string, started_at=started_at,
                            complete_at=complete_at, pr_number=PR_NUMBER, pr_project=PR_PROJECT)


class BuildUpdateTest(TestCase):
    def test_late_start_event_does_not_overwrite_finished_build(self):
        self.assertEqual(save_build(), SUCCESS)
        self.assertEqual(save_build(result=Buildbot.FAILURE, state_string='Failed layout-tests', complete_at=1700000001), SUCCESS)
        self.assertEqual(save_build(), SUCCESS)

        build = Build.get_existing_build(BuildbotInstance.get_uid(HOSTNAME, 100))
        self.assertEqual(build.result, Buildbot.FAILURE)
        self.assertEqual(build.complete_at, 1700000001)
        self.assertEqual(build.state_string, 'Failed layout-tests')

    def test_start_event_after_finish_event_with_no_prior_start(self):
        self.assertEqual(save_build(result=Buildbot.SUCCESS, state_string='build successful', complete_at=1700000001), SUCCESS)
        self.assertEqual(save_build(), SUCCESS)

        build = Build.get_existing_build(BuildbotInstance.get_uid(HOSTNAME, 100))
        self.assertEqual(build.result, Buildbot.SUCCESS)
        self.assertEqual(build.complete_at, 1700000001)

    def test_start_then_finish_updates_build(self):
        self.assertEqual(save_build(), SUCCESS)
        build = Build.get_existing_build(BuildbotInstance.get_uid(HOSTNAME, 100))
        self.assertIsNone(build.result)
        self.assertIsNone(build.complete_at)

        self.assertEqual(save_build(result=Buildbot.SUCCESS, state_string='build successful', complete_at=1700000060), SUCCESS)
        build = Build.get_existing_build(BuildbotInstance.get_uid(HOSTNAME, 100))
        self.assertEqual(build.result, Buildbot.SUCCESS)
        self.assertEqual(build.complete_at, 1700000060)
        self.assertEqual(build.state_string, 'build successful')


def save_queue_build(queue, build_id, result, builder_id=None, state_string='state'):
    return Build.save_build(change_id=CHANGE_ID, hostname=HOSTNAME, build_id=build_id, builder_id=builder_id or build_id, builder_name=queue.title() + '-EWS',
                            builder_display_name=queue, number=1, result=result, state_string=state_string, started_at=1700000000,
                            complete_at=1700000060, pr_number=PR_NUMBER, pr_project=PR_PROJECT)


class StatusViewTest(TestCase):
    def setUp(self):
        Change.save_change(CHANGE_ID, pr_number=PR_NUMBER, pr_project=PR_PROJECT)
        patcher = mock.patch.dict(Buildbot.QUEUE_TRIGGERS, {'mac-wk2': 'mac', 'api-mac': 'mac'})
        patcher.start()
        self.addCleanup(patcher.stop)

    def status(self):
        return json.loads(Status().get(RequestFactory().get('/status/'), CHANGE_ID).content)

    def test_testers_of_a_skipped_build_are_reported_skipped(self):
        save_queue_build('mac', 1, Buildbot.SKIPPED)
        status = self.status()
        self.assertEqual(status['mac']['state'], Buildbot.SKIPPED)
        self.assertEqual(status['mac-wk2']['state'], Buildbot.SKIPPED)
        self.assertEqual(status['api-mac']['state'], Buildbot.SKIPPED)

    def test_testers_of_a_passed_build_are_still_pending(self):
        save_queue_build('mac', 1, Buildbot.SUCCESS)
        status = self.status()
        self.assertEqual(status['mac']['state'], Buildbot.SUCCESS)
        self.assertNotIn('mac-wk2', status)
        self.assertNotIn('api-mac', status)


class SkippedBuildBubbleTest(TestCase):
    NO_RELEVANT_CHANGES = f"Pull request {PR_NUMBER} doesn't have relevant changes"

    def setUp(self):
        Change.save_change(CHANGE_ID, pr_number=PR_NUMBER, pr_project=PR_PROJECT)
        for patcher in [
            mock.patch.dict(Buildbot.QUEUE_TRIGGERS, {'mac-wk2': 'mac'}),
            mock.patch.dict(Buildbot.icons_for_queues_mapping, {'mac': 'buildOnly', 'mac-wk2': 'testOnly', 'webkitpy': 'testOnly'}),
        ]:
            patcher.start()
            self.addCleanup(patcher.stop)

    def change(self):
        return Change.get_change(CHANGE_ID)

    def test_build_skipped_for_no_relevant_changes_is_marked_skipped(self):
        save_queue_build('mac', 1, Buildbot.SKIPPED, state_string=self.NO_RELEVANT_CHANGES)
        cell = GitHubEWS().github_status_for_buildbot_queue(self.change(), 'mac')
        self.assertIn(f'[{GitHubEWS.ICON_BUILD_SKIPPED} {StatusBubble.BUILDER_ICON} mac]', cell)
        self.assertIn("this PR doesn't change any files relevant to this queue", cell)

    def test_tester_of_a_build_skipped_for_no_relevant_changes_is_marked_skipped(self):
        save_queue_build('mac', 1, Buildbot.SKIPPED, state_string=self.NO_RELEVANT_CHANGES)
        cell = GitHubEWS().github_status_for_buildbot_queue(self.change(), 'mac-wk2')
        self.assertIn(f'[{GitHubEWS.ICON_BUILD_SKIPPED} {StatusBubble.TESTER_ICON} mac-wk2]', cell)
        self.assertIn("this PR doesn't change any files relevant to this queue", cell)

    def test_path_filtered_queue_skipped_for_no_relevant_changes_is_marked_skipped(self):
        save_queue_build('webkitpy', 1, Buildbot.SKIPPED, state_string=self.NO_RELEVANT_CHANGES)
        cell = GitHubEWS().github_status_for_buildbot_queue(self.change(), 'webkitpy')
        self.assertIn(f'[{GitHubEWS.ICON_BUILD_SKIPPED} {StatusBubble.TESTER_ICON} webkitpy]', cell)

    def test_build_skipped_for_an_outdated_hash_is_struck_out(self):
        save_queue_build('mac', 1, Buildbot.SKIPPED, state_string=f'Hash {CHANGE_ID[:8]} on PR {PR_NUMBER} is outdated')
        cell = GitHubEWS().github_status_for_buildbot_queue(self.change(), 'mac')
        self.assertIn(f'[{GitHubEWS.ICON_EMPTY_SPACE} ~~{StatusBubble.BUILDER_ICON} mac~~]', cell)
        self.assertIn('Commit was outdated', cell)

    def test_web_bubble_for_no_relevant_changes_explains_the_skip(self):
        save_queue_build('mac', 1, Buildbot.SKIPPED, state_string=self.NO_RELEVANT_CHANGES)
        bubble = StatusBubble()._build_bubble(self.change(), 'mac')
        self.assertEqual(bubble['state'], 'skipped')
        self.assertIn(StatusBubble.NO_RELEVANT_CHANGES_MESSAGE, bubble['details_message'])
