# Copyright (c) 2009 Google Inc. All rights reserved.
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


from optparse import make_option

from webkitpy.common.checkout.commitinfo import CommitInfo
from webkitpy.common.config.committers import CommitterList
from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.net.regressionwindow import RegressionWindow
from webkitpy.common.system.user import User
from webkitpy.tool.grammar import pluralize
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand
from webkitpy.common.system.deprecated_logging import log
from webkitpy.layout_tests import port


class BugsToCommit(AbstractDeclarativeCommand):
    name = "bugs-to-commit"
    help_text = "List bugs in the commit-queue"

    def execute(self, options, args, tool):
        # FIXME: This command is poorly named.  It's fetching the commit-queue list here.  The name implies it's fetching pending-commit (all r+'d patches).
        bug_ids = tool.bugs.queries.fetch_bug_ids_from_commit_queue()
        for bug_id in bug_ids:
            print "%s" % bug_id


class PatchesInCommitQueue(AbstractDeclarativeCommand):
    name = "patches-in-commit-queue"
    help_text = "List patches in the commit-queue"

    def execute(self, options, args, tool):
        patches = tool.bugs.queries.fetch_patches_from_commit_queue()
        log("Patches in commit queue:")
        for patch in patches:
            print patch.url()


class PatchesToCommitQueue(AbstractDeclarativeCommand):
    name = "patches-to-commit-queue"
    help_text = "Patches which should be added to the commit queue"
    def __init__(self):
        options = [
            make_option("--bugs", action="store_true", dest="bugs", help="Output bug links instead of patch links"),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    @staticmethod
    def _needs_commit_queue(patch):
        if patch.commit_queue() == "+": # If it's already cq+, ignore the patch.
            log("%s already has cq=%s" % (patch.id(), patch.commit_queue()))
            return False

        # We only need to worry about patches from contributers who are not yet committers.
        committer_record = CommitterList().committer_by_email(patch.attacher_email())
        if committer_record:
            log("%s committer = %s" % (patch.id(), committer_record))
        return not committer_record

    def execute(self, options, args, tool):
        patches = tool.bugs.queries.fetch_patches_from_pending_commit_list()
        patches_needing_cq = filter(self._needs_commit_queue, patches)
        if options.bugs:
            bugs_needing_cq = map(lambda patch: patch.bug_id(), patches_needing_cq)
            bugs_needing_cq = sorted(set(bugs_needing_cq))
            for bug_id in bugs_needing_cq:
                print "%s" % tool.bugs.bug_url_for_bug_id(bug_id)
        else:
            for patch in patches_needing_cq:
                print "%s" % tool.bugs.attachment_url_for_id(patch.id(), action="edit")


class PatchesToReview(AbstractDeclarativeCommand):
    name = "patches-to-review"
    help_text = "List patches that are pending review"

    def execute(self, options, args, tool):
        patch_ids = tool.bugs.queries.fetch_attachment_ids_from_review_queue()
        log("Patches pending review:")
        for patch_id in patch_ids:
            print patch_id


class LastGreenRevision(AbstractDeclarativeCommand):
    name = "last-green-revision"
    help_text = "Prints the last known good revision"

    def execute(self, options, args, tool):
        print self._tool.buildbot.last_green_revision()


class WhatBroke(AbstractDeclarativeCommand):
    name = "what-broke"
    help_text = "Print failing buildbots (%s) and what revisions broke them" % BuildBot.default_host

    def _print_builder_line(self, builder_name, max_name_width, status_message):
        print "%s : %s" % (builder_name.ljust(max_name_width), status_message)

    def _print_blame_information_for_builder(self, builder_status, name_width, avoid_flakey_tests=True):
        builder = self._tool.buildbot.builder_with_name(builder_status["name"])
        red_build = builder.build(builder_status["build_number"])
        regression_window = builder.find_regression_window(red_build)
        if not regression_window.failing_build():
            self._print_builder_line(builder.name(), name_width, "FAIL (error loading build information)")
            return
        if not regression_window.build_before_failure():
            self._print_builder_line(builder.name(), name_width, "FAIL (blame-list: sometime before %s?)" % regression_window.failing_build().revision())
            return

        revisions = regression_window.revisions()
        first_failure_message = ""
        if (regression_window.failing_build() == builder.build(builder_status["build_number"])):
            first_failure_message = " FIRST FAILURE, possibly a flaky test"
        self._print_builder_line(builder.name(), name_width, "FAIL (blame-list: %s%s)" % (revisions, first_failure_message))
        for revision in revisions:
            commit_info = self._tool.checkout().commit_info_for_revision(revision)
            if commit_info:
                print commit_info.blame_string(self._tool.bugs)
            else:
                print "FAILED to fetch CommitInfo for r%s, likely missing ChangeLog" % revision

    def execute(self, options, args, tool):
        builder_statuses = tool.buildbot.builder_statuses()
        longest_builder_name = max(map(len, map(lambda builder: builder["name"], builder_statuses)))
        failing_builders = 0
        for builder_status in builder_statuses:
            # If the builder is green, print OK, exit.
            if builder_status["is_green"]:
                continue
            self._print_blame_information_for_builder(builder_status, name_width=longest_builder_name)
            failing_builders += 1
        if failing_builders:
            print "%s of %s are failing" % (failing_builders, pluralize("builder", len(builder_statuses)))
        else:
            print "All builders are passing!"


class ResultsFor(AbstractDeclarativeCommand):
    name = "results-for"
    help_text = "Print a list of failures for the passed revision from bots on %s" % BuildBot.default_host
    argument_names = "REVISION"

    def _print_layout_test_results(self, results):
        if not results:
            print " No results."
            return
        for title, files in results.parsed_results().items():
            print " %s" % title
            for filename in files:
                print "  %s" % filename

    def execute(self, options, args, tool):
        builders = self._tool.buildbot.builders()
        for builder in builders:
            print "%s:" % builder.name()
            build = builder.build_for_revision(args[0], allow_failed_lookups=True)
            self._print_layout_test_results(build.layout_test_results())


class FailureReason(AbstractDeclarativeCommand):
    name = "failure-reason"
    help_text = "Lists revisions where individual test failures started at %s" % BuildBot.default_host

    def _print_blame_information_for_transition(self, green_build, red_build, failing_tests):
        regression_window = RegressionWindow(green_build, red_build)
        revisions = regression_window.revisions()
        print "SUCCESS: Build %s (r%s) was the first to show failures: %s" % (red_build._number, red_build.revision(), failing_tests)
        print "Suspect revisions:"
        for revision in revisions:
            commit_info = self._tool.checkout().commit_info_for_revision(revision)
            if commit_info:
                print commit_info.blame_string(self._tool.bugs)
            else:
                print "FAILED to fetch CommitInfo for r%s, likely missing ChangeLog" % revision

    def _explain_failures_for_builder(self, builder, start_revision):
        print "Examining failures for \"%s\", starting at r%s" % (builder.name(), start_revision)
        revision_to_test = start_revision
        build = builder.build_for_revision(revision_to_test, allow_failed_lookups=True)
        layout_test_results = build.layout_test_results()
        if not layout_test_results:
            # FIXME: This could be made more user friendly.
            print "Failed to load layout test results; can't continue. (start revision = r%s)" % start_revision
            return 1

        results_to_explain = set(layout_test_results.failing_tests())
        last_build_with_results = build
        print "Starting at %s" % revision_to_test
        while results_to_explain:
            revision_to_test -= 1
            new_build = builder.build_for_revision(revision_to_test, allow_failed_lookups=True)
            if not new_build:
                print "No build for %s" % revision_to_test
                continue
            build = new_build
            latest_results = build.layout_test_results()
            if not latest_results:
                print "No results build %s (r%s)" % (build._number, build.revision())
                continue
            failures = set(latest_results.failing_tests())
            if len(failures) >= 20:
                # FIXME: We may need to move this logic into the LayoutTestResults class.
                # The buildbot stops runs after 20 failures so we don't have full results to work with here.
                print "Too many failures in build %s (r%s), ignoring." % (build._number, build.revision())
                continue
            fixed_results = results_to_explain - failures
            if not fixed_results:
                print "No change in build %s (r%s), %s unexplained failures (%s in this build)" % (build._number, build.revision(), len(results_to_explain), len(failures))
                last_build_with_results = build
                continue
            self._print_blame_information_for_transition(build, last_build_with_results, fixed_results)
            last_build_with_results = build
            results_to_explain -= fixed_results
        if results_to_explain:
            print "Failed to explain failures: %s" % results_to_explain
            return 1
        print "Explained all results for %s" % builder.name()
        return 0

    def _builder_to_explain(self):
        builder_statuses = self._tool.buildbot.builder_statuses()
        red_statuses = [status for status in builder_statuses if not status["is_green"]]
        print "%s failing" % (pluralize("builder", len(red_statuses)))
        builder_choices = [status["name"] for status in red_statuses]
        # We could offer an "All" choice here.
        chosen_name = User.prompt_with_list("Which builder to diagnose:", builder_choices)
        # FIXME: prompt_with_list should really take a set of objects and a set of names and then return the object.
        for status in red_statuses:
            if status["name"] == chosen_name:
                return (self._tool.buildbot.builder_with_name(chosen_name), status["built_revision"])

    def execute(self, options, args, tool):
        (builder, latest_revision) = self._builder_to_explain()
        start_revision = self._tool.user.prompt("Revision to walk backwards from? [%s] " % latest_revision) or latest_revision
        if not start_revision:
            print "Revision required."
            return 1
        return self._explain_failures_for_builder(builder, start_revision=int(start_revision))


class TreeStatus(AbstractDeclarativeCommand):
    name = "tree-status"
    help_text = "Print the status of the %s buildbots" % BuildBot.default_host
    long_help = """Fetches build status from http://build.webkit.org/one_box_per_builder
and displayes the status of each builder."""

    def execute(self, options, args, tool):
        for builder in tool.buildbot.builder_statuses():
            status_string = "ok" if builder["is_green"] else "FAIL"
            print "%s : %s" % (status_string.ljust(4), builder["name"])


class SkippedPorts(AbstractDeclarativeCommand):
    name = "skipped-ports"
    help_text = "Print the list of ports skipping the given layout test(s)"
    long_help = """Scans the the Skipped file of each port and figure
out what ports are skipping the test(s). Categories are taken in account too."""
    argument_names = "TEST_NAME"

    def execute(self, options, args, tool):
        class Options:
            # Required for chromium port.
            use_drt = True

        results = dict([(test_name, []) for test_name in args])
        for port_name, port_object in tool.port_factory.get_all(options=Options).iteritems():
            for test_name in args:
                if port_object.skips_layout_test(test_name):
                    results[test_name].append(port_name)

        for test_name, ports in results.iteritems():
            if ports:
                print "Ports skipping test %r: %s" % (test_name, ', '.join(ports))
            else:
                print "Test %r is not skipped by any port." % test_name
