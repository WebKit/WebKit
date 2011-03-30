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

    def archive_last_layout_test_results(self, patch):
        raise NotImplementedError("subclasses must implement")

    # We could make results_archive optional, but for now it's required.
    def report_flaky_tests(self, patch, flaky_tests, results_archive):
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
        if not self._patch.review() != "-":
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

    def _clean(self):
        return self._run_command([
            "clean",
        ],
        "Cleaned working directory",
        "Unable to clean working directory")

    def _update(self):
        # FIXME: Ideally the status server log message should include which revision we updated to.
        return self._run_command([
            "update",
        ],
        "Updated working directory",
        "Unable to update working directory")

    def _apply(self):
        return self._run_command([
            "apply-attachment",
            "--no-update",
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

    def _failing_results_from_last_run(self):
        results = self._delegate.layout_test_results()
        if not results:
            return []  # Makes callers slighty cleaner to not have to deal with None
        return results.failing_test_results()

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

    def _report_flaky_tests(self, flaky_test_results, results_archive):
        self._delegate.report_flaky_tests(self._patch, flaky_test_results, results_archive)

    def _test_patch(self):
        if self._test():
            return True

        first_results = self._failing_results_from_last_run()
        first_failing_tests = [result.filename for result in first_results]
        first_results_archive = self._delegate.archive_last_layout_test_results(self._patch)
        if self._test():
            # Only report flaky tests if we were successful at archiving results.
            if first_results_archive:
                self._report_flaky_tests(first_results, first_results_archive)
            return True

        second_results = self._failing_results_from_last_run()
        second_failing_tests = [result.filename for result in second_results]
        if first_failing_tests != second_failing_tests:
            # We could report flaky tests here, but since run-webkit-tests
            # is run with --exit-after-N-failures=1, we would need to
            # be careful not to report constant failures as flaky due to earlier
            # flaky test making them not fail (no results) in one of the runs.
            # See https://bugs.webkit.org/show_bug.cgi?id=51272
            return False

        if self._build_and_test_without_patch():
            return self.report_failure()  # The error from the previous ._test() run is real, report it.
        return False  # Tree must be red, just retry later.

    def report_failure(self):
        if not self._validate():
            return False
        raise self._script_error

    def run(self):
        if not self._validate():
            return False
        if not self._clean():
            return False
        if not self._update():
            return False
        if not self._apply():
            return self.report_failure()
        if not self._patch.is_rollout():
            if not self._build():
                if not self._build_without_patch():
                    return False
                return self.report_failure()
            if not self._test_patch():
                return False
        # Make sure the patch is still valid before landing (e.g., make sure
        # no one has set commit-queue- since we started working on the patch.)
        if not self._validate():
            return False
        # FIXME: We should understand why the land failure occured and retry if possible.
        if not self._land():
            return self.report_failure()
        return True
