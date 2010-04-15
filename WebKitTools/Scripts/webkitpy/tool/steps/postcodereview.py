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


class PostCodeReview(AbstractStep):
    @classmethod
    def options(cls):
        return [
            Options.cc,
            Options.description,
            Options.fancy_review,
            Options.review,
        ]

    def run(self, state):
        if not self._options.fancy_review:
            return
        # FIXME: This will always be None because we don't retrieve the issue
        #        number from the ChangeLog yet.
        codereview_issue = state.get("codereview_issue")
        message = self._options.description
        if not message:
            # If we have an issue number, then the message becomes the label
            # of the new patch. Otherwise, it becomes the title of the whole
            # issue.
            if codereview_issue:
                message = "Updated patch"
            elif state.get("bug_title"):
                # This is the common case for the the first "upload" command.
                message = state.get("bug_title")
            elif state.get("bug_id"):
                # This is the common case for the "post" command and
                # subsequent runs of the "upload" command.  In this case,
                # I'd rather add the new patch to the existing issue, but
                # that's not implemented yet.
                message = "Code review for %s" % self._tool.bugs.bug_url_for_bug_id(state["bug_id"])
            else:
                # Unreachable with our current commands, but we might hit
                # this case if we support bug-less code reviews.
                message = "Code review"
        created_issue = self._tool.codereview.post(message=message,
                                                   codereview_issue=codereview_issue,
                                                   cc=self._options.cc)
        if created_issue:
            # FIXME: Record the issue number in the ChangeLog.
            state["codereview_issue"] = created_issue
