# Copyright (c) 2010 Google Inc. All rights reserved.
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

from webkitpy.common.system.executive import ScriptError


class CommitQueueTask(object):
    def __init__(self, tool, commit_queue, patch):
        self._tool = tool
        self._commit_queue = commit_queue
        self._patch = patch
        self._script_error = None

    def _validate(self):
        # Bugs might get closed, or patches might be obsoleted or r-'d while the
        # commit-queue is processing.
        self._patch = self._tool.bugs.fetch_attachment(self._patch.id())
        if self._patch.is_obsolete():
            return False
        if self._patch.bug().is_closed():
            return False
        if not self._patch.committer():
            return False
        # Reviewer is not required. Missing reviewers will be caught during
        # the ChangeLog check during landing.
        return True

    def _run_command(self, command, success_message, failure_message):
        try:
            self._commit_queue.run_webkit_patch(command)
            self._commit_queue.command_passed(success_message, patch=self._patch)
            return True
        except ScriptError, e:
            self._script_error = e
            self.failure_status_id = self._commit_queue.command_failed(failure_message, script_error=self._script_error, patch=self._patch)
            return False

    def _apply(self):
        return self._run_command([
            "apply-attachment",
            "--force-clean",
            "--non-interactive",
            "--quiet",
            self._patch.id(),
        ],
        "Applied patch",
        "Patch does not apply")

    def _build(self):
        return self._run_command([
            "build",
            "--no-clean",
            "--no-update",
            "--build",
            "--build-style=both",
            "--quiet",
        ],
        "Built patch",
        "Patch does not build")

    def _build_without_patch(self):
        return self._run_command([
            "build",
            "--force-clean",
            "--no-update",
            "--build",
            "--build-style=both",
            "--quiet",
        ],
        "Able to build without patch",
        "Unable to build without patch")

    def _test(self):
        return self._run_command([
            "build-and-test",
            "--no-clean",
            "--no-update",
            # Notice that we don't pass --build, which means we won't build!
            "--test",
            "--quiet",
            "--non-interactive",
        ],
        "Passed tests",
        "Patch does not pass tests")

    def _build_and_test_without_patch(self):
        return self._run_command([
            "build-and-test",
            "--force-clean",
            "--no-update",
            "--build",
            "--test",
            "--quiet",
            "--non-interactive",
        ],
        "Able to pass tests without patch",
        "Unable to pass tests without patch (tree is red?)")

    def _land(self):
        return self._run_command([
            "land-attachment",
            "--force-clean",
            "--ignore-builders",
            "--quiet",
            "--non-interactive",
            "--parent-command=commit-queue",
            self._patch.id(),
        ],
        "Landed patch",
        "Unable to land patch")

    def run(self):
        if not self._validate():
            return False
        if not self._apply():
            raise self._script_error
        if not self._build():
            if not self._build_without_patch():
                return False
            raise self._script_error
        if not self._patch.is_rollout():
            if not self._test():
                if not self._test():
                    if not self._build_and_test_without_patch():
                        return False
                    raise self._script_error
        # Make sure the patch is still valid before landing (e.g., make sure
        # no one has set commit-queue- since we started working on the patch.)
        if not self._validate():
            return False
        if not self._land():
            raise self._script_error
        return True
