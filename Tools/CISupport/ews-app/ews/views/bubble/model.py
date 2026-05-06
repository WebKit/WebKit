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
from datetime import datetime
from typing import Optional, Tuple, Union


@dataclass(frozen=True)
class StepSnapshot:
    uid: str
    state_string: str
    result: Optional[int]
    started_at: Optional[int]


@dataclass(frozen=True)
class BuildSnapshot:
    builder_id: int
    builder_name: str
    builder_display_name: str
    number: int
    result: Optional[int]
    state_string: str
    started_at: Optional[int]
    complete_at: Optional[int]
    steps_by_uid_asc: Tuple[StepSnapshot, ...]
    steps_by_uid_desc: Tuple[StepSnapshot, ...]
    last_step: Optional[StepSnapshot]


@dataclass(frozen=True)
class ChangeSnapshot:
    change_id: str
    created: datetime
    sent_to_buildbot: bool
    sent_to_commit_queue: bool
    obsolete: bool
    pr_number: int
    pr_project: str
    comment_id: int


@dataclass(frozen=True)
class BubbleInputs:
    change: ChangeSnapshot
    queue: str
    builds: Tuple[BuildSnapshot, ...]
    is_parent_build: bool
    queue_position: Union[int, str, None]
    hide_icons: bool
    sent_to_buildbot: bool


@dataclass(frozen=True)
class BubbleData:
    name: str
    state: str
    url: Optional[str]
    details_message: Optional[str]
    builder_full_name: Optional[str]
    os_details: Optional[str]
    build_timestamp_iso: Optional[str]
    queue_position: Union[int, str, None]
