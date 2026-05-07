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
from typing import Callable, Optional

from ews.views.bubble.model import BubbleData


@dataclass(frozen=True)
class GithubIcons:
    pass_: str
    fail: str
    waiting: str
    ongoing: str
    ongoing_with_failures: str
    error: str
    empty: str


_STATE_TO_ICON_ATTR = {
    'pass': 'pass_',
    'fail': 'fail',
    'started': 'ongoing',
    'provisional-fail': 'ongoing_with_failures',
    'error': 'error',
    'cancelled': 'empty',
    'skipped': 'empty',
    'none': 'waiting',
}


def _icon_for_state(state: str, icons: GithubIcons, *, in_progress: bool) -> str:
    if state == 'provisional-fail' and not in_progress:
        return icons.ongoing
    attr = _STATE_TO_ICON_ATTR.get(state, 'error')
    return getattr(icons, attr)


def render_markdown_bubble(
    data: BubbleData,
    *,
    icons: GithubIcons,
    is_in_progress: bool,
    escape: Callable[[str], str],
) -> str:
    name = data.name
    if data.state in ('cancelled', 'skipped'):
        name = u'~~{}~~'.format(name)

    icon = _icon_for_state(data.state, icons, in_progress=is_in_progress)
    hover = escape(data.details_message or '')
    url = data.url if data.url is not None else ''
    return u'| [{icon} {name}]({url} "{hover}") '.format(icon=icon, name=name, url=url, hover=hover)


def render_markdown_no_build_misc(name: Optional[str] = None) -> str:
    return u'| '
