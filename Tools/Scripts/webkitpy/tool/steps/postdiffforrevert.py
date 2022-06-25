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

from webkitpy.common.net.bugzilla import Attachment
from webkitpy.tool.steps.abstractstep import AbstractStep


class PostDiffForRevert(AbstractStep):
    def run(self, state):
        comment_text = "Any committer can land this patch automatically by \
marking it commit-queue+.  The commit-queue will build and test \
the patch before landing to ensure that the revert will be \
successful.  This process takes approximately 15 minutes.\n\n\
If you would like to land the revert faster, you can use the \
following command:\n\n\
  webkit-patch land-attachment ATTACHMENT_ID\n\n\
where ATTACHMENT_ID is the ID of this attachment."
        self._tool.bugs.add_patch_to_bug(
            state["bug_id"],
            self.cached_lookup(state, "diff"),
            "%s%s" % (Attachment.revert_preamble, state["revision"]),
            comment_text=comment_text,
            mark_for_review=False,
            mark_for_commit_queue=True)
