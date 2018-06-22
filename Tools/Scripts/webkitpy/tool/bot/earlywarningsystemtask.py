# Copyright (c) 2011 Google Inc. All rights reserved.
# Copyright (C) 2017 Apple Inc. All rights reserved.
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

from webkitpy.tool.bot.patchanalysistask import PatchAnalysisTask, PatchAnalysisTaskDelegate, UnableToApplyPatch, PatchIsNotValid, PatchIsNotApplicable


class EarlyWarningSystemTaskDelegate(PatchAnalysisTaskDelegate):
    pass


class EarlyWarningSystemTask(PatchAnalysisTask):
    def __init__(self, delegate, patch, should_run_tests=True, should_build=True):
        PatchAnalysisTask.__init__(self, delegate, patch)
        self._should_run_tests = should_run_tests
        self._should_build = should_build

    def validate(self):
        # FIXME: Need a way to ask the status server for latest status of a security bug.
        # Attachments downloaded from the status server do not have an associated bug and
        # reflect the Bugzilla state at the time they were uploaded to the status server.
        # See <https://bugs.webkit.org/show_bug.cgi?id=186817>.
        self._patch = self._delegate.refetch_patch(self._patch)
        if self._patch.is_obsolete():
            self.error = "Patch is obsolete."
            return False
        if self._patch.bug() and self._patch.bug().is_closed():
            self.error = "Bug is already closed."
            return False
        if self._patch.review() == "-":
            self.error = "Patch is marked r-."
            return False
        return True

    def run(self):
        """
        Returns True if the patch passes EWS.
        Raises an exception if the patch fails EWS.
        Returns False if the patch status can not be ascertained. AbstractEarlyWarningSystem.review_patch()
        will unlock the patch so that it would be retried.
        """
        if not self._clean():
            return False
        if not self._update():
            return False
        if not self._apply():
            raise UnableToApplyPatch(self._patch)
        if not self._check_patch_relevance():
            raise PatchIsNotApplicable(self._patch)
        if self._should_build:
            if not self._build():
                if not self._build_without_patch():
                    return False
                return self.report_failure()
        if not self._should_run_tests:
            return True
        return self._test_patch()
