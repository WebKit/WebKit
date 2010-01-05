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

from webkitpy.changelogs import ChangeLog
from webkitpy.steps.abstractstep import AbstractStep


class PrepareChangeLogForRevert(AbstractStep):
    def run(self, state):
        # First, discard the ChangeLog changes from the rollout.
        os.chdir(self._tool.scm().checkout_root)
        changelog_paths = self._tool.scm().modified_changelogs()
        self._tool.scm().revert_files(changelog_paths)

        # Second, make new ChangeLog entries for this rollout.
        # This could move to prepare-ChangeLog by adding a --revert= option.
        self._run_script("prepare-ChangeLog")
        bug_url = self._tool.bugs.bug_url_for_bug_id(state["bug_id"]) if state["bug_id"] else None
        for changelog_path in changelog_paths:
            # FIXME: Seems we should prepare the message outside of changelogs.py and then just pass in
            # text that we want to use to replace the reviewed by line.
            ChangeLog(changelog_path).update_for_revert(state["revision"], state["reason"], bug_url)
