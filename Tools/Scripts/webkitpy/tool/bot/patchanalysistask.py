# Copyright (c) 2010 Google Inc. All rights reserved.
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

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.jsctestresults import JSCTestResults


class UnableToApplyPatch(Exception):
    def __init__(self, patch):
        Exception.__init__(self)
        self.patch = patch


class PatchIsNotValid(Exception):
    def __init__(self, patch, failure_message):
        Exception.__init__(self)
        self.patch = patch
        self.failure_message = failure_message


class PatchIsNotApplicable(Exception):
    def __init__(self, patch):
        Exception.__init__(self)
        self.patch = patch


class PatchAnalysisTaskDelegate(object):
    def parent_command(self):
        raise NotImplementedError("subclasses must implement")

    def run_command(self, command):
        raise NotImplementedError("subclasses must implement")

    def refetch_patch(self, patch):
        raise NotImplementedError("subclasses must implement")

    def expected_failures(self):
        raise NotImplementedError("subclasses must implement")

    def test_results(self):
        raise NotImplementedError("subclasses must implement")

    def archive_last_test_results(self, patch):
        raise NotImplementedError("subclasses must implement")

    def build_style(self):
        raise NotImplementedError("subclasses must implement")

    # We could make results_archive optional, but for now it's required.
    def report_flaky_tests(self, patch, flaky_tests, results_archive):
        raise NotImplementedError("subclasses must implement")


class PatchAnalysisTask(object):
    def __init__(self, delegate, patch):
        self._delegate = delegate
        self._patch = patch
        self._script_error = None
        self._results_archive_from_patch_test_run = None
        self._results_from_patch_test_run = None
        self.error = None

    def _run_command(self, command, success_message, failure_message):
        if not self.validate():
            raise PatchIsNotValid(self._patch, self.error)
        try:
            self._delegate.run_command(command)
            return True
        except ScriptError as e:
            self._script_error = e
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

    def _check_patch_relevance(self):
        args = [
            "check-patch-relevance",
            "--quiet",
        ]

        if hasattr(self._delegate, 'group'):
            args.append("--group=%s" % self._delegate.group())

        return self._run_command(args, "Checked relevance of patch", "Patch was not relevant")

    def _build(self):
        args = [
            "build",
            "--no-clean",
            "--no-update",
            "--build-style=%s" % self._delegate.build_style(),
        ]

        if hasattr(self._delegate, 'group'):
            args.append("--group=%s" % self._delegate.group())

        return self._run_command(args, "Built patch", "Patch does not build")

    def _build_without_patch(self):
        args = [
            "build",
            "--force-clean",
            "--no-update",
            "--build-style=%s" % self._delegate.build_style(),
        ]

        if hasattr(self._delegate, 'group'):
            args.append("--group=%s" % self._delegate.group())

        return self._run_command(args, "Able to build without patch", "Unable to build without patch")

    def _test(self):
        args = [
            "build-and-test",
            "--no-clean",
            "--no-update",
            # Notice that we don't pass --build, which means we won't build!
            "--test",
            "--non-interactive",
            "--build-style=%s" % self._delegate.build_style(),
        ]

        if hasattr(self._delegate, 'group'):
            args.append("--group=%s" % self._delegate.group())

        return self._run_command(args, "Passed tests", "Patch does not pass tests")

    def _build_and_test_without_patch(self):
        args = [
            "build-and-test",
            "--force-clean",
            "--no-update",
            "--test",
            "--non-interactive",
            "--build-style=%s" % self._delegate.build_style(),
        ]

        if getattr(self._delegate, 'should_build', True):
            args.append("--build")

        if hasattr(self._delegate, 'group'):
            args.append("--group=%s" % self._delegate.group())

        return self._run_command(args, "Able to pass tests without patch", "Unable to pass tests without patch (tree is red?)")

    def _land(self):
        # Unclear if this should pass --quiet or not.  If --parent-command always does the reporting, then it should.
        return self._run_command([
            "land-attachment",
            "--force-clean",
            "--non-interactive",
            "--parent-command=" + self._delegate.parent_command(),
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

    def _should_defer_patch_or_throw(self, failures_with_patch, results_archive_for_failures_with_patch, script_error, failure_id):
        self._build_and_test_without_patch()
        clean_tree_results = self._delegate.test_results()

        if clean_tree_results.did_exceed_test_failure_limit():
            # We cannot know whether the failures we saw in the test runs with the patch are expected.
            return True

        failures_introduced_by_patch = frozenset(failures_with_patch) - frozenset(clean_tree_results.failing_test_results())
        if failures_introduced_by_patch:
            self.failure_status_id = failure_id
            # report_failure will either throw or return false.
            return not self.report_failure(results_archive_for_failures_with_patch, LayoutTestResults(failures_introduced_by_patch, did_exceed_test_failure_limit=False), script_error)

        # In this case, we know that all of the failures that we saw with the patch were
        # also present without the patch, so we don't need to defer.
        return False

    # FIXME: Abstract out common parts of the retry logic.
    def _retry_jsc_tests(self):
        first_results = self._delegate.test_results()
        first_script_error = self._script_error
        first_failure_status_id = self.failure_status_id
        if first_results is None:
            return False

        if self._test():
            return True
        second_results = self._delegate.test_results()
        second_script_error = self._script_error
        if second_results is None:
            return False

        consistently_failing_test_results = JSCTestResults.intersection(first_results, second_results)

        self._build_and_test_without_patch()
        clean_tree_results = self._delegate.test_results()
        if clean_tree_results is None:
            return False

        if consistently_failing_test_results.is_subset(clean_tree_results):
            return True

        self.failure_status_id = first_failure_status_id
        return self.report_failure(None, consistently_failing_test_results, first_script_error)

    def _retry_layout_tests(self):
        # Note: archive_last_test_results deletes the results directory, making these calls order-sensitve.
        # We could remove this dependency by building the test_results from the archive.
        first_results = self._delegate.test_results()
        first_results_archive = self._delegate.archive_last_test_results(self._patch)
        first_script_error = self._script_error
        first_failure_status_id = self.failure_status_id

        if self._test() and not first_results.did_exceed_test_failure_limit():
            # Only report flaky tests if we were successful at parsing results.json and archiving results.
            if first_results and first_results_archive:
                self._report_flaky_tests(first_results.failing_test_results(), first_results_archive)
            return True

        second_results = self._delegate.test_results()
        second_results_archive = self._delegate.archive_last_test_results(self._patch)
        second_script_error = self._script_error
        second_failure_status_id = self.failure_status_id

        if second_results.did_exceed_test_failure_limit() and first_results.did_exceed_test_failure_limit():
            self._build_and_test_without_patch()
            clean_tree_results = self._delegate.test_results()

            if (len(first_results.failing_tests()) - len(clean_tree_results.failing_tests())) <= 5:
                return False

            self.failure_status_id = first_failure_status_id

            return self.report_failure(first_results_archive, first_results, first_script_error)

        if second_results.did_exceed_test_failure_limit():
            self._should_defer_patch_or_throw(first_results.failing_test_results(), first_results_archive,
                                              first_script_error, first_failure_status_id)
            return False

        if first_results.did_exceed_test_failure_limit():
            self._should_defer_patch_or_throw(second_results.failing_test_results(), second_results_archive,
                                              second_script_error, second_failure_status_id)
            return False

        if self._results_failed_different_tests(first_results, second_results):
            first_failing_results_set = frozenset(first_results.failing_test_results())
            second_failing_results_set = frozenset(second_results.failing_test_results())

            tests_that_only_failed_first = first_failing_results_set.difference(second_failing_results_set)
            self._report_flaky_tests(tests_that_only_failed_first, first_results_archive)

            tests_that_only_failed_second = second_failing_results_set.difference(first_failing_results_set)
            self._report_flaky_tests(tests_that_only_failed_second, second_results_archive)

            tests_that_consistently_failed = first_failing_results_set.intersection(second_failing_results_set)
            if tests_that_consistently_failed:
                if self._should_defer_patch_or_throw(tests_that_consistently_failed, first_results_archive,
                                                     first_script_error, first_failure_status_id):
                    return False  # Defer patch

            # At this point we know that at least one test flaked, but no consistent failures
            # were introduced. This is a bit of a grey-zone.
            return False  # Defer patch

        if self._should_defer_patch_or_throw(first_results.failing_test_results(), first_results_archive,
                                             first_script_error, first_failure_status_id):
            return False  # Defer patch

        # At this point, we know that the first and second runs had the exact same failures,
        # and that those failures are all present on the clean tree, so we can say with certainty
        # that the patch is good.
        return True

    def _test_patch(self):
        if self._test():
            return True

        if hasattr(self._delegate, 'group') and self._delegate.group() == "jsc":
            return self._retry_jsc_tests()
        else:
            return self._retry_layout_tests()

    def results_archive_from_patch_test_run(self, patch):
        assert(self._patch.id() == patch.id())  # PatchAnalysisTask is not currently re-useable.
        return self._results_archive_from_patch_test_run

    def results_from_patch_test_run(self, patch):
        assert(self._patch.id() == patch.id())  # PatchAnalysisTask is not currently re-useable.
        return self._results_from_patch_test_run

    def report_failure(self, results_archive=None, results=None, script_error=None):
        if not self.validate():
            return False
        self._results_archive_from_patch_test_run = results_archive
        self._results_from_patch_test_run = results
        raise script_error or self._script_error

    def validate(self):
        raise NotImplementedError("subclasses must implement")

    def run(self):
        raise NotImplementedError("subclasses must implement")
