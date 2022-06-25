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


class SuggestReviewers(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.git_commit,
            Options.suggest_reviewers,
        ]

    def run(self, state):
        if not self._options.suggest_reviewers:
            return

        reviewers = self._tool.checkout().suggested_reviewers(self._options.git_commit, self._changed_files(state))[:5]
        print("The following reviewers have recently modified files in your patch:")
        print(", ".join([reviewer.full_name for reviewer in reviewers]))

        if not state.get('bug_id'):
            return
        if not self._tool.user.confirm("Would you like to CC them?"):
            return
        reviewer_emails = [reviewer.bugzilla_email() for reviewer in reviewers]
        self._tool.bugs.add_cc_to_bug(state['bug_id'], reviewer_emails)
