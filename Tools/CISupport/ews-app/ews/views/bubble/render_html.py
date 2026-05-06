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

from typing import Any, Dict

from ews.views.bubble.model import BubbleData


def _assemble_html_details(data: BubbleData) -> str:
    parts = []
    if data.builder_full_name:
        parts.append(data.builder_full_name)
    if data.details_message:
        parts.append(data.details_message)
    suffix_lines = []
    if data.os_details:
        suffix_lines.append(data.os_details)
    if data.build_timestamp_iso:
        suffix_lines.append(data.build_timestamp_iso)
    if suffix_lines:
        parts.append('\n'.join(suffix_lines))
    return '\n\n'.join(parts)


def render_html_bubble(data: BubbleData) -> Dict[str, Any]:
    out = {
        'name': data.name,
        'state': data.state,
    }
    if data.url:
        out['url'] = data.url
    details = _assemble_html_details(data)
    if details:
        out['details_message'] = details
    if data.queue_position is not None:
        out['queue_position'] = data.queue_position
    return out
