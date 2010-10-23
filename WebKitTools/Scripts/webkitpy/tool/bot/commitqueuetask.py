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
from webkitpy.common.net.layouttestresults import LayoutTestResults


class CommitQueueTaskDelegate(object):
    def run_command(self, command):
        raise NotImplementedError("subclasses must implement")

    def command_passed(self, message, patch):
        raise NotImplementedError("subclasses must implement")

    def command_failed(self, message, script_error, patch):
        raise NotImplementedError("subclasses must implement")

    def refetch_patch(self, patch):
        raise NotImplementedError("subclasses must implement")

    def layout_test_results(self):
        raise NotImplementedError("subclasses must implement")

    def report_flaky_tests(self, patch, flaky_tests):
        raise NotImplementedError("subclasses must implement")


class CommitQueueTask(object):
    def __init__(self, delegate, patch):
        self._delegate = delegate
        self._patch = patch
        self._script_error = None

    def _validate(self):
        # Bugs might get closed, or patches might be obsoleted or r-'d while the
        # commit-queue is processing.
        self._patch = self._delegate.refetch_patch(self._patch)
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
            self._delegate.run_command(command)
            self._delegate.command_passed(success_message, patch=self._patch)
            return True
        except ScriptError, e:
            self._script_error = e
            self.failure_status_id = self._delegate.command_failed(failure_message, script_error=self._script_error, patch=self._patch)
            return False

    def _apply(self):
        return self._run_command([
            "apply-attachment",
            "--force-clean",
            "--non-interactive",
            self._patch.id(),
        ],
        "Applied patch",
        "Patch does not apply")

    def _build(self):
        return self._run_command([
            "build",
            "--no-clean",
            "--no-update",
            "--build-style=both",
        ],
        "Built patch",
        "Patch does not build")

    def _build_without_patch(self):
        return self._run_command([
            "build",
            "--force-clean",
            "--no-update",
            "--build-style=both",
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
            "--non-interactive",
        ],
        "Able to pass tests without patch",
        "Unable to pass tests without patch (tree is red?)")

    def _failing_tests_from_last_run(self):
        results = self._delegate.layout_test_results()
        if not results:
            return None
        return results.failing_tests()

    def _land(self):
        # Unclear if this should pass --quiet or not.  If --parent-command always does the reporting, then it should.
        return self._run_command([
            "land-attachment",
            "--force-clean",
            "--ignore-builders",
            "--non-interactive",
            "--parent-command=commit-queue",
            self._patch.id(),
        ],
        "Landed patch",
        "Unable to land patch")

    def _report_flaky_tests(self, flaky_tests):
        self._delegate.report_flaky_tests(self._patch, flaky_tests)

    def _test_patch(self):
        if self._patch.is_rollout():
            return True
        if self._test():
            return True

        first_failing_tests = self._failing_tests_from_last_run()
        if self._test():
            self._report_flaky_tests(first_failing_tests)
            return True

        second_failing_tests = self._failing_tests_from_last_run()
        if first_failing_tests != second_failing_tests:
            self._report_flaky_tests(first_failing_tests + second_failing_tests)
            return False

        if self._build_and_test_without_patch():
            raise self._script_error  # The error from the previous ._test() run is real, report it.
        return False  # Tree must be red, just retry later.

    def run(self):
        if not self._validate():
            return False
        if not self._apply():
            raise self._script_error
        if not self._build():
            if not self._build_without_patch():
                return False
            raise self._script_error
        if not self._test_patch():
            return False
        # Make sure the patch is still valid before landing (e.g., make sure
        # no one has set commit-queue- since we started working on the patch.)
        if not self._validate():
            return False
        if not self._land():
            raise self._script_error
        return True
