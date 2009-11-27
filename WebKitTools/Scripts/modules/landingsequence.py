#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

from modules.comments import bug_comment_from_commit_text
from modules.logging import log
from modules.scm import ScriptError, CheckoutNeedsUpdate
from modules.webkitlandingscripts import WebKitLandingScripts, commit_message_for_this_commit
from modules.webkitport import WebKitPort
from modules.workqueue import WorkQueue

class LandingSequence:
    def __init__(self, patch, options, tool):
        self._patch = patch
        self._options = options
        self._tool = tool
        self._port = WebKitPort.port(self._options)

    def run(self):
        self.clean()
        self.update()
        self.apply_patch()
        self.build()
        self.test()
        commit_log = self.commit()
        self.close_patch(commit_log)
        self.close_bug()

    def run_and_handle_errors(self):
        try:
            self.run()
        except CheckoutNeedsUpdate, e:
            log("Commit failed because the checkout is out of date.  Please update and try again.")
            log("You can pass --no-build to skip building/testing after update if you believe the new commits did not affect the results.")
            WorkQueue.exit_after_handled_error(e)
        except ScriptError, e:
            if not self._options.quiet:
                log(e.message_with_output())
            if self._options.non_interactive:
                # Mark the patch as commit-queue- and comment in the bug.
                self._tool.bugs.reject_patch_from_commit_queue(self._patch["id"], e.message_with_output())
            WorkQueue.exit_after_handled_error(e)

    def clean(self):
        self._tool.steps.clean_working_directory(self._tool.scm(), self._options)

    def update(self):
        self._tool.scm().update_webkit()

    def apply_patch(self):
        log("Processing patch %s from bug %s." % (self._patch["id"], self._patch["bug_id"]))
        self._tool.scm().apply_patch(self._patch, force=self._options.non_interactive)

    def build(self):
        # Make sure the tree is still green after updating, before building this patch.
        # The first patch ends up checking tree status twice, but that's OK.
        self._tool.steps.ensure_builders_are_green(self._tool.buildbot, self._options)
        self._tool.steps.build_webkit(quiet=self._options.quiet, port=self._port)

    def test(self):
        # When running non-interactively we don't want to launch Safari and we want to exit after the first failure.
        self._tool.run_tests(launch_safari=not self._options.non_interactive, fail_fast=self._options.non_interactive, quiet=self._options.quiet, port=self._port)

    def commit(self):
        commit_message = commit_message_for_this_commit(self._tool.scm())
        return self._tool.scm().commit_with_message(commit_message.message())

    def close_patch(self, commit_log):
        comment_text = bug_comment_from_commit_text(self._tool.scm(), commit_log)
        self._tool.bugs.clear_attachment_flags(self._patch["id"], comment_text)

    def close_bug(self):
        # Check to make sure there are no r? or r+ patches on the bug before closing.
        # Assume that r- patches are just previous patches someone forgot to obsolete.
        patches = self._tool.bugs.fetch_patches_from_bug(self._patch["bug_id"])
        for patch in patches:
            review_flag = patch.get("review")
            if review_flag == "?" or review_flag == "+":
                log("Not closing bug %s as attachment %s has review=%s.  Assuming there are more patches to land from this bug." % (patch["bug_id"], patch["id"], review_flag))
                return
        self._tool.bugs.close_bug_as_fixed(self._patch["bug_id"], "All reviewed patches have been landed.  Closing bug.")


class ConditionalLandingSequence(LandingSequence):
    def __init__(self, patch, options, tool):
        LandingSequence.__init__(self, patch, options, tool)

    def update(self):
        if self._options.update:
            LandingSequence.update(self)

    def build(self):
        if self._options.build:
            LandingSequence.build(self)

    def test(self):
        if self._options.build and self._options.test:
            LandingSequence.test(self)

    def close_bug(self):
        if self._options.close_bug:
            LandingSequence.close_bug(self)

