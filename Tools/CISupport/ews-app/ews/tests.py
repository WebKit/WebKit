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

from django.test import TestCase

from ews.common.buildbot import Buildbot
from ews.config import SUCCESS
from ews.models.build import Build
from ews.models.buildbotinstance import BuildbotInstance
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
