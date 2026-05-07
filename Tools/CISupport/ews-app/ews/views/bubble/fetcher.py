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
from typing import List, Optional, Tuple, Union

from django.utils import timezone

from ews.models.build import Build
from ews.models.patch import Change
from ews.views.bubble.config import BubbleConfig
from ews.views.bubble.model import (
    BubbleInputs,
    BuildSnapshot,
    ChangeSnapshot,
    StepSnapshot,
)


MAX_BUILDS_DISPLAYED = 10


def _step_snapshot(step) -> StepSnapshot:
    return StepSnapshot(
        uid=step.uid,
        state_string=step.state_string,
        result=step.result,
        started_at=step.started_at,
    )


def _build_snapshot(build) -> BuildSnapshot:
    asc = tuple(_step_snapshot(s) for s in build.step_set.all().order_by('uid'))
    desc = tuple(reversed(asc))
    last_step = asc[-1] if asc else None
    return BuildSnapshot(
        builder_id=build.builder_id,
        builder_name=build.builder_name,
        builder_display_name=build.builder_display_name,
        number=build.number,
        result=build.result,
        state_string=build.state_string,
        started_at=build.started_at,
        complete_at=build.complete_at,
        steps_by_uid_asc=asc,
        steps_by_uid_desc=desc,
        last_step=last_step,
    )


def _change_snapshot(change) -> ChangeSnapshot:
    return ChangeSnapshot(
        change_id=change.change_id,
        created=change.created,
        sent_to_buildbot=change.sent_to_buildbot,
        sent_to_commit_queue=change.sent_to_commit_queue,
        obsolete=change.obsolete,
        pr_number=change.pr_number,
        pr_project=change.pr_project,
        comment_id=change.comment_id,
    )


class BubbleDataFetcher(object):
    def __init__(self, config: BubbleConfig, *, now_provider=None):
        self.config = config
        self._now_provider = now_provider

    def now(self) -> datetime.datetime:
        if self._now_provider is not None:
            return self._now_provider()
        return timezone.now()

    def fetch_change_snapshot(self, change) -> ChangeSnapshot:
        return _change_snapshot(change)

    def _fetch_raw_builds(self, change, queue: str, parent_queue: Optional[str]) -> Tuple[List, bool]:
        builds = [b for b in change.build_set.all() if b.builder_display_name == queue]
        is_parent_build = False
        if not builds and parent_queue:
            builds = [b for b in change.build_set.all() if b.builder_display_name == parent_queue]
            is_parent_build = True
        if not builds:
            return ([], is_parent_build)
        builds.sort(key=lambda b: b.number, reverse=True)
        return (builds, is_parent_build)

    def fetch_builds_for_queue(self, change, queue: str, parent_queue: Optional[str]) -> Tuple[Tuple[BuildSnapshot, ...], bool]:
        raw_builds, is_parent_build = self._fetch_raw_builds(change, queue, parent_queue)
        if not raw_builds:
            return ((), is_parent_build)
        capped = raw_builds[:MAX_BUILDS_DISPLAYED]
        return (tuple(_build_snapshot(b) for b in capped), is_parent_build)

    def fetch_queue_position(self, change, queue: str, parent_queue: Optional[str]) -> Union[int, str, None]:
        now = self.now()
        from_timestamp = now - datetime.timedelta(days=self.config.days_to_check_queue_position)
        hide_from_timestamp = now - datetime.timedelta(days=self.config.days_to_hide_bubble)

        if change.created < hide_from_timestamp:
            return None

        if change.created < from_timestamp:
            return self.config.unknown_queue_position

        sent_field = 'sent_to_commit_queue' if queue == 'commit' else 'sent_to_buildbot'
        previously_sent_changes = set(Change.objects
                                      .filter(created__gte=from_timestamp)
                                      .filter(**{sent_field: True})
                                      .filter(obsolete=False)
                                      .filter(created__lt=change.created))
        target_queue = parent_queue if parent_queue else queue
        recent_builds = Build.objects \
            .filter(created__gte=from_timestamp) \
            .filter(builder_display_name=target_queue)
        processed_changes = set(b.change for b in recent_builds)
        return len(previously_sent_changes - processed_changes) + 1

    def build_inputs(self, change, queue: str, *, hide_icons: bool, sent_to_buildbot: bool) -> BubbleInputs:
        parent_queue = self.config.get_parent_queue(queue)
        builds, is_parent_build = self.fetch_builds_for_queue(change, queue, parent_queue)
        queue_position = None
        if not builds:
            queue_position = self.fetch_queue_position(change, queue, parent_queue)
        return BubbleInputs(
            change=self.fetch_change_snapshot(change),
            queue=queue,
            builds=builds,
            is_parent_build=is_parent_build,
            queue_position=queue_position,
            hide_icons=hide_icons,
            sent_to_buildbot=sent_to_buildbot,
        )
