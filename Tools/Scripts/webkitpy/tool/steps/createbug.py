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

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options


class CreateBug(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.cc,
            Options.cc_radar,
            Options.component,
            Options.blocks,
        ]

    def run(self, state):
        # No need to create a bug if we already have one.
        if state.get("bug_id"):
            return
        cc = self._options.cc
        if not cc:
            cc = state.get("bug_cc")
        cc_radar = self._options.cc_radar
        if cc_radar:
            if cc:
                cc = "webkit-bug-importer@group.apple.com,%s" % cc
            else:
                cc = "webkit-bug-importer@group.apple.com"
        if self._options.blocks:
            blocked_bugs = [int(self._options.blocks)]
        else:
            blocked_bugs = state.get("bug_id_list", [])
        blocks = ", ".join(str(bug) for bug in blocked_bugs if bug)
        state["bug_id"] = self._tool.bugs.create_bug(state["bug_title"], state["bug_description"], blocked=blocks, component=self._options.component, cc=cc)
        state["created_new_bug"] = True
        for blocked_bug in blocked_bugs:
            if blocked_bug:
                status = self._tool.bugs.fetch_bug(blocked_bug).status()
                if status == 'RESOLVED':
                    self._tool.bugs.reopen_bug(blocked_bug, "Re-opened since this is blocked by bug %s" % state["bug_id"])
