# Copyright (C) 2010 Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import re

from webkitpy.common.checkout.changelog import ChangeLog
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.common.system.deprecated_logging import error


class ValidateReviewer(AbstractStep):
    def run(self, state):
        # FIXME: We should figure out how to handle the current working
        #        directory issue more globally.
        os.chdir(self._tool.scm().checkout_root)
        for changelog_path in self._tool.checkout().modified_changelogs():
            changelog_entry = ChangeLog(changelog_path).latest_entry()
            if changelog_entry.reviewer():
                continue
            if re.search("unreviewed", changelog_entry.contents(), re.IGNORECASE):
                continue
            if re.search("rubber[ -]stamp", changelog_entry.contents(), re.IGNORECASE):
                continue
            reviewer_text = changelog_entry.reviewer_text()
            if reviewer_text:
                log("%s does not appear to be a valid reviewer according to committers.py.")
            error('%s neither lists a valid reviewer nor contains the string "Unreviewed" or "Rubber stamp" (case insensitive).' % changelog_path)
