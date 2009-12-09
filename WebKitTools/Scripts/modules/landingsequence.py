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
from modules.executive import ScriptError
from modules.logging import log
from modules.scm import CheckoutNeedsUpdate
from modules.webkitport import WebKitPort
from modules.workqueue import WorkQueue
from modules.buildsteps import CleanWorkingDirectoryStep, UpdateStep, ApplyPatchStep, EnsureBuildersAreGreenStep, BuildStep, RunTestsStep, CommitStep, ClosePatchStep, CloseBugStep


class LandingSequenceErrorHandler():
    @classmethod
    def handle_script_error(cls, tool, patch, script_error):
        raise NotImplementedError, "subclasses must implement"

# FIXME: This class is slowing being killed and replaced with StepSequence.
class LandingSequence:
    def __init__(self, patch, options, tool):
        self._patch = patch
        self._options = options
        self._tool = tool
        self._port = WebKitPort.port(self._options.port)

    def run(self):
        self.clean()
        self.update()
        self.apply_patch()
        self.check_builders()
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
            if self._options.parent_command:
                command = self._tool.command_by_name(self._options.parent_command)
                command.handle_script_error(self._tool, self._patch, e)
            WorkQueue.exit_after_handled_error(e)

    def clean(self):
        step = CleanWorkingDirectoryStep(self._tool, self._options)
        step.run()

    def update(self):
        step = UpdateStep(self._tool, self._options)
        step.run()

    def apply_patch(self):
        step = ApplyPatchStep(self._tool, self._options, self._patch)
        step.run()

    def check_builders(self):
        step = EnsureBuildersAreGreenStep(self._tool, self._options)
        step.run()

    def build(self):
        step = BuildStep(self._tool, self._options)
        step.run()

    def test(self):
        step = RunTestsStep(self._tool, self._options)
        step.run()

    def commit(self):
        step = CommitStep(self._tool, self._options)
        return step.run()

    def close_patch(self, commit_log):
        step = ClosePatchStep(self._tool, self._options, self._patch)
        step.run(commit_log)

    def close_bug(self):
        step = CloseBugStep(self._tool, self._options, self._patch)
        step.run()
