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

from webkitpy.common.checkout.changelog import ChangeLog
from webkitpy.common.net.bugzilla import BugzillaError
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.system.deprecated_logging import error


class PrepareChangeLog(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.port,
            Options.quiet,
            Options.email,
            Options.git_commit,
        ]

    def _ensure_bug_url(self, state):
        if not state.get("bug_id"):
            return
        bug_id = state.get("bug_id")
        changelogs = self.cached_lookup(state, "changelogs")
        for changelog_path in changelogs:
            changelog = ChangeLog(changelog_path)
            if not changelog.latest_entry().bug_id():
                changelog.set_short_description_and_bug_url(
                    self.cached_lookup(state, "bug_title"),  # Will raise an exception if not authorized to retrieve the bug title.
                    self._tool.bugs.bug_url_for_bug_id(bug_id))

    def run(self, state):
        if self.cached_lookup(state, "changelogs"):
            self._ensure_bug_url(state)
            return
        os.chdir(self._tool.scm().checkout_root)
        args = [self.port().script_path("prepare-ChangeLog")]
        if state.get("bug_id"):
            args.append("--bug=%s" % state["bug_id"])
        if self._options.email:
            args.append("--email=%s" % self._options.email)

        if self._tool.scm().supports_local_commits():
            args.append("--merge-base=%s" % self._tool.scm().merge_base(self._options.git_commit))

        try:
            self._tool.executive.run_and_throw_if_fail(args, self._options.quiet)
        except ScriptError, e:
            error("Unable to prepare ChangeLogs.")
        state["diff"] = None # We've changed the diff
