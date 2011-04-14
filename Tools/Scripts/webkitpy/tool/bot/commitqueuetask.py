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
from webkitpy.tool.bot.expectedfailures import ExpectedFailures


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
        self._results_archive_from_patch_test_run = None
        self._expected_failures = ExpectedFailures()

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
        success = self._run_command([
            "build-and-test",
            "--no-clean",
            "--no-update",
            # Notice that we don't pass --build, which means we won't build!
            "--test",
            "--non-interactive",
        ],
        "Passed tests",
        "Patch does not pass tests")

        self._expected_failures.shrink_expected_failures(self._delegate.layout_test_results(), success)
        return success

    def _build_and_test_without_patch(self):
        success = self._run_command([
            "build-and-test",
            "--force-clean",
            "--no-update",
            "--build",
            "--test",
            "--non-interactive",
        ],
        "Able to pass tests without patch",
        "Unable to pass tests without patch (tree is red?)")

        self._expected_failures.shrink_expected_failures(self._delegate.layout_test_results(), success)
        return success

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

    def _results_failed_different_tests(self, first, second):
        first_failing_tests = [] if not first else first.failing_tests()
        second_failing_tests = [] if not second else second.failing_tests()
        return first_failing_tests != second_failing_tests

    def _test_patch(self):
        if self._test():
            return True

        # Note: archive_last_layout_test_results deletes the results directory, making these calls order-sensitve.
        # We could remove this dependency by building the layout_test_results from the archive.
        first_results = self._delegate.layout_test_results()
        first_results_archive = self._delegate.archive_last_layout_test_results(self._patch)

        if self._expected_failures.failures_were_expected(first_results):
            return True

        if self._test():
            # Only report flaky tests if we were successful at parsing results.html and archiving results.
            if first_results and first_results_archive:
                self._report_flaky_tests(first_results.failing_test_results(), first_results_archive)
            return True

        second_results = self._delegate.layout_test_results()
        if self._results_failed_different_tests(first_results, second_results):
            # We could report flaky tests here, but we would need to be careful
            # to use similar checks to ExpectedFailures._can_trust_results
            # to make sure we don't report constant failures as flakes when
            # we happen to hit the --exit-after-N-failures limit.
            # See https://bugs.webkit.org/show_bug.cgi?id=51272
            return False

        # Archive (and remove) second results so layout_test_results() after
        # build_and_test_without_patch won't use second results instead of the clean-tree results.
        second_results_archive = self._delegate.archive_last_layout_test_results(self._patch)

        if self._build_and_test_without_patch():
            # The error from the previous ._test() run is real, report it.
            return self.report_failure(first_results_archive)

        clean_tree_results = self._delegate.layout_test_results()
        self._expected_failures.grow_expected_failures(clean_tree_results)

        return False  # Tree must be redder than we expected, just retry later.

    def results_archive_from_patch_test_run(self, patch):
        assert(self._patch.id() == patch.id())  # CommitQueueTask is not currently re-useable.
        return self._results_archive_from_patch_test_run

    def report_failure(self, results_archive=None):
        if not self._validate():
            return False
        self._results_archive_from_patch_test_run = results_archive
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
