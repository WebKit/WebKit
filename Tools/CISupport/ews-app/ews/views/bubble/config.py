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

from dataclasses import dataclass
from typing import Mapping, Optional, Sequence


BUILDER_ICON = u'\U0001f6e0'
TESTER_ICON = u'\U0001f9ea'


@dataclass(frozen=True)
class BubbleConfig:
    icons_for_queues: Mapping[str, str]
    queue_name_by_shortname: Mapping[str, str]
    all_queues: Sequence[str]
    pr_column_by_shortname: Mapping[str, str]
    pr_comment_misc_handlers: Sequence[str]
    queue_triggers: Mapping[str, str]
    buildbot_server_host: str
    builder_icon: str = BUILDER_ICON
    tester_icon: str = TESTER_ICON
    days_to_check_queue_position: float = 0.5
    days_to_hide_bubble: int = 7
    build_retry_msg: str = 'retrying build'
    unknown_queue_position: str = '?'

    def is_tester_queue(self, queue: str) -> bool:
        return self.icons_for_queues.get(queue) in ('testOnly', 'buildAndTest')

    def is_builder_queue(self, queue: str) -> bool:
        return self.icons_for_queues.get(queue) in ('buildOnly', 'buildAndTest')

    def get_parent_queue(self, queue: str) -> Optional[str]:
        return self.queue_triggers.get(queue)

    def queue_full_name(self, queue: str) -> Optional[str]:
        return self.queue_name_by_shortname.get(queue)

    def builder_url(self, queue_full_name: str) -> str:
        return 'https://{}/#/builders/{}'.format(self.buildbot_server_host, queue_full_name)

    def build_url(self, builder_id: int, number: int) -> str:
        return 'https://{}/#/builders/{}/builds/{}'.format(self.buildbot_server_host, builder_id, number)

    @classmethod
    def from_buildbot_globals(cls) -> 'BubbleConfig':
        # Imported lazily so this module can be imported without Django settings loaded.
        from ews.common.buildbot import Buildbot
        import ews.config as ews_config
        return cls(
            icons_for_queues=dict(Buildbot.icons_for_queues_mapping),
            queue_name_by_shortname=dict(Buildbot.queue_name_by_shortname_mapping),
            all_queues=tuple(Buildbot.all_queues),
            pr_column_by_shortname=dict(Buildbot.pr_column_by_shortname),
            pr_comment_misc_handlers=tuple(Buildbot.pr_comment_misc_handlers),
            queue_triggers=dict(Buildbot.QUEUE_TRIGGERS),
            buildbot_server_host=ews_config.BUILDBOT_SERVER_HOST,
        )
