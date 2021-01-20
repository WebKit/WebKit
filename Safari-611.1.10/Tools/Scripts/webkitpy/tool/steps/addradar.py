# Copyright (C) 2018 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.common.checkout.changelog import *

import logging
import re

_log = logging.getLogger(__name__)


class AddRadar(AbstractStep):
    def run(self, state):
        bug_id = state["bug_id"]
        keyword = "InRadar"
        wkbi_email = ["webkit-bug-importer@group.apple.com"]
        radar_link = None

        # Only add Radar information if we are operating on a newly created bug
        if not state.get("created_new_bug"):
            return

        for changelog_path in self.cached_lookup(state, "changelogs"):
            changelog_entry = ChangeLog(changelog_path).latest_entry()
            radar_id = ChangeLogEntry._parse_radar_id(changelog_entry.contents())
            if radar_id:
                radar_link = "<rdar://problem/{}>".format(radar_id)
                break

        if radar_link:
            self._tool.bugs.add_keyword_to_bug(bug_id, keyword)
            self._tool.bugs.post_comment_to_bug(bug_id, radar_link)
            self._tool.bugs.add_cc_to_bug(bug_id, wkbi_email)
