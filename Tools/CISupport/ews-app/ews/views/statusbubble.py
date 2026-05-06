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

import logging

from django.shortcuts import render
from django.utils import timezone
from django.views import View
from django.views.decorators.clickjacking import xframe_options_exempt

from ews.models.patch import Change
from ews.views.bubble.compute import compute_bubble
from ews.views.bubble.config import BUILDER_ICON, TESTER_ICON, BubbleConfig
from ews.views.bubble.fetcher import BubbleDataFetcher
from ews.views.bubble.render_html import render_html_bubble

_log = logging.getLogger(__name__)


class StatusBubble(View):
    # Backward-compatibility constants. External callers (github.py, fetcher.py,
    # views/status.py) read these — keep them in sync with BubbleConfig defaults.
    DAYS_TO_CHECK_QUEUE_POSITION = 0.5
    DAYS_TO_HIDE_BUBBLE = 7
    BUILDER_ICON = BUILDER_ICON
    TESTER_ICON = TESTER_ICON
    BUILD_RETRY_MSG = 'retrying build'
    UNKNOWN_QUEUE_POSITION = '?'

    @xframe_options_exempt
    def get(self, request, change_id):
        hide_icons = bool(request.GET.get('hide_icons', False))
        change = Change.get_change(change_id)
        config = BubbleConfig.from_buildbot_globals()
        fetcher = BubbleDataFetcher(config)
        now = timezone.now()

        bubbles = []
        if change:
            for queue in config.all_queues:
                inputs = fetcher.build_inputs(
                    change, queue,
                    hide_icons=hide_icons,
                    sent_to_buildbot=change.sent_to_buildbot,
                )
                data = compute_bubble(inputs, config, now=now)
                if data:
                    bubbles.append(render_html_bubble(data))

            if change.sent_to_commit_queue:
                cq_hide_icons = hide_icons or not change.sent_to_buildbot
                inputs = fetcher.build_inputs(
                    change, 'commit',
                    hide_icons=cq_hide_icons,
                    sent_to_buildbot=True,
                )
                data = compute_bubble(inputs, config, now=now)
                if data:
                    bubbles.insert(0, render_html_bubble(data))

        show_submit_to_ews = not (change and change.sent_to_buildbot)
        template_values = {
            'bubbles': bubbles,
            'change_id': change_id,
            'show_submit_to_ews': show_submit_to_ews,
            'show_failure_to_apply': False,
        }
        return render(request, 'statusbubble.html', template_values)

    # --- Backward-compatibility shims for callers outside the bubble package ---

    def get_latest_build_for_queue(self, change, queue, parent_queue=None):
        builds, is_parent_build = self.get_all_builds_for_queue(change, queue, parent_queue)
        if not builds:
            return (None, None)
        return (builds[0], is_parent_build)

    def get_all_builds_for_queue(self, change, queue, parent_queue=None):
        config = BubbleConfig.from_buildbot_globals()
        fetcher = BubbleDataFetcher(config)
        builds, is_parent_build = fetcher._fetch_raw_builds(change, queue, parent_queue)
        if not builds:
            return (None, None)
        return (builds, is_parent_build)
